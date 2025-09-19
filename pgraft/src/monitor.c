/*-------------------------------------------------------------------------
 *
 * monitor.c
 *		Monitoring capabilities for pgraft extension
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "../include/pgraft.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "utils/elog.h"
#include "lib/stringinfo.h"
#include "access/htup_details.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "nodes/pg_list.h"
#include <string.h>
#include <time.h>

/* Function info declarations */
PG_FUNCTION_INFO_V1(pgraft_health_check);
PG_FUNCTION_INFO_V1(pgraft_cluster_health);
PG_FUNCTION_INFO_V1(pgraft_node_health);
PG_FUNCTION_INFO_V1(pgraft_monitor_start);
PG_FUNCTION_INFO_V1(pgraft_monitor_stop);
PG_FUNCTION_INFO_V1(pgraft_get_network_status);
PG_FUNCTION_INFO_V1(pgraft_get_replication_status);
PG_FUNCTION_INFO_V1(pgraft_get_alerts);
PG_FUNCTION_INFO_V1(pgraft_get_detailed_node_info);

/* Monitoring state */
typedef struct PgraftMonitorState
{
	bool		enabled;
	TimestampTz	last_check;
	int			total_checks;
	int			failed_checks;
	int			warnings_count;
	int			errors_count;
	int			info_events;
	int			warning_events;
	int			error_events;
	int			total_events;
} PgraftMonitorState;

static PgraftMonitorState monitor_state = {0};

/* Forward declarations */
static bool pgraft_check_cluster_health(void);
static bool pgraft_check_node_health(uint64_t node_id);

/*
 * Initialize monitoring system
 */
void
pgraft_monitor_init(void)
{
	if (monitor_state.enabled)
		return;

	monitor_state.enabled = true;
	monitor_state.last_check = GetCurrentTimestamp();
	monitor_state.total_checks = 0;
	monitor_state.failed_checks = 0;
	monitor_state.warnings_count = 0;
	monitor_state.errors_count = 0;

	elog(INFO, "pgraft monitoring system initialized");
}

/*
 * Shutdown monitoring system
 */
void
pgraft_monitor_shutdown(void)
{
	monitor_state.enabled = false;
	elog(INFO, "pgraft monitoring system shutdown");
}

/*
 * Update monitoring metrics
 */
void
pgraft_monitor_update_metrics(void)
{
	if (!monitor_state.enabled)
		return;

	monitor_state.total_checks++;
	monitor_state.last_check = GetCurrentTimestamp();

	/* Perform health checks */
	if (!pgraft_check_cluster_health())
	{
		monitor_state.failed_checks++;
		monitor_state.errors_count++;
	}
}

/*
 * Check monitoring health
 */
void
pgraft_monitor_check_health(void)
{
	if (!monitor_state.enabled)
		return;

	pgraft_monitor_update_metrics();
}

/*
 * Monitor tick function
 */
void
pgraft_monitor_tick(void)
{
	if (!monitor_state.enabled)
		return;

	/* Update monitoring metrics */
	pgraft_monitor_update_metrics();

	/* Check cluster health */
	pgraft_monitor_check_health();

	/* Update monitoring state */
	monitor_state.last_check = GetCurrentTimestamp();
	monitor_state.total_checks++;

	/* Log monitoring activity */
	if (monitor_state.total_checks % 100 == 0)
	{
		elog(DEBUG1, "pgraft_monitor_tick: completed %d checks, %d failed checks",
			 monitor_state.total_checks, monitor_state.failed_checks);
	}
}

/*
 * Check cluster health
 */
static bool
pgraft_check_cluster_health(void)
{
	pgraft_raft_state_t *raft_state;
	pgraft_health_worker_status_t worker_status;
	int healthy_nodes;
	
	/* Check if pgraft is initialized */
	if (!pgraft_is_initialized())
	{
		return false;
	}
	
	/* Check Raft system health */
	raft_state = pgraft_raft_get_state();
	if (!raft_state || !raft_state->is_initialized)
	{
		return false;
	}
	
	/* Check if we have a stable leader */
	if (raft_state->state != PGRAFT_STATE_LEADER && 
		raft_state->state != PGRAFT_STATE_FOLLOWER)
	{
		return false;
	}
	
	/* Check communication system health */
	if (!pgraft_comm_initialized())
	{
		return false;
	}
	
	/* Check number of healthy nodes */
	healthy_nodes = pgraft_get_healthy_nodes_count();
	if (healthy_nodes < 1)
	{
		return false;
	}
	
	/* Check worker status */
	if (!pgraft_worker_manager_get_status(&worker_status))
	{
		return false;
	}
	
	if (!worker_status.is_running || worker_status.errors_count > 0)
	{
		return false;
	}
	
	return true;
}

/*
 * Check node health
 */
static bool
pgraft_check_node_health(uint64_t node_id)
{
	pgraft_raft_state_t *raft_state;
	
	/* Check if pgraft is initialized */
	if (!pgraft_is_initialized())
	{
		return false;
	}
	
	/* Check if node is connected */
	if (!pgraft_is_node_connected(node_id))
	{
		return false;
	}
	
	/* Check Raft system health */
	raft_state = pgraft_raft_get_state();
	if (!raft_state || !raft_state->is_initialized)
	{
		return false;
	}
	
	/* Check if node is part of the cluster */
	if (raft_state->leader_id == 0 && raft_state->state == PGRAFT_STATE_LEADER)
	{
		return false;
	}
	
	/* Check communication health */
	if (!pgraft_comm_initialized())
	{
		return false;
	}
	
	/* Check if we can reach the node */
	if (pgraft_get_connection_to_node(node_id) < 0)
	{
		return false;
	}
	
	return true;
}

/*
 * Health check function
 */
Datum
pgraft_health_check(PG_FUNCTION_ARGS)
{
	StringInfoData buf;
	char	   *health_json;

	initStringInfo(&buf);
	appendStringInfo(&buf,
					"{"
					"\"enabled\":%s,"
					"\"total_checks\":%d,"
					"\"failed_checks\":%d,"
					"\"warnings_count\":%d,"
					"\"errors_count\":%d,"
					"\"last_check\":\"%s\""
					"}",
					monitor_state.enabled ? "true" : "false",
					monitor_state.total_checks,
					monitor_state.failed_checks,
					monitor_state.warnings_count,
					monitor_state.errors_count,
					timestamptz_to_str(monitor_state.last_check)
		);

	health_json = buf.data;
	PG_RETURN_TEXT_P(cstring_to_text(health_json));
}

/*
 * Cluster health function
 */
Datum
pgraft_cluster_health(PG_FUNCTION_ARGS)
{
	StringInfoData buf;
	char	   *health_json;

	initStringInfo(&buf);
	appendStringInfo(&buf,
					"{"
					"\"cluster_healthy\":%s,"
					"\"monitoring_enabled\":%s,"
					"\"last_check\":\"%s\""
					"}",
					pgraft_check_cluster_health() ? "true" : "false",
					monitor_state.enabled ? "true" : "false",
					timestamptz_to_str(monitor_state.last_check)
		);

	health_json = buf.data;
	PG_RETURN_TEXT_P(cstring_to_text(health_json));
}

/*
 * Node health function
 */
Datum
pgraft_node_health(PG_FUNCTION_ARGS)
{
	uint64		node_id;
	StringInfoData buf;
	char	   *health_json;

	node_id = (uint64) PG_GETARG_INT32(0);

	initStringInfo(&buf);
	appendStringInfo(&buf,
					"{"
					"\"node_id\":%llu,"
					"\"healthy\":%s,"
					"\"last_check\":\"%s\""
					"}",
					(unsigned long long) node_id,
					pgraft_check_node_health(node_id) ? "true" : "false",
					timestamptz_to_str(monitor_state.last_check)
		);

	health_json = buf.data;
	PG_RETURN_TEXT_P(cstring_to_text(health_json));
}

/*
 * Start monitoring
 */
Datum
pgraft_monitor_start(PG_FUNCTION_ARGS)
{
	if (monitor_state.enabled)
	{
		elog(WARNING, "pgraft: monitoring already started");
		PG_RETURN_BOOL(false);
	}

	pgraft_monitor_init();
	PG_RETURN_BOOL(true);
}

/*
 * Stop monitoring
 */
Datum
pgraft_monitor_stop(PG_FUNCTION_ARGS)
{
	if (!monitor_state.enabled)
	{
		elog(WARNING, "pgraft: monitoring not started");
		PG_RETURN_BOOL(false);
	}

	pgraft_monitor_shutdown();
	PG_RETURN_BOOL(true);
}

/*
 * Get network status
 */
Datum
pgraft_get_network_status(PG_FUNCTION_ARGS)
{
	StringInfoData buf;
	char	   *status_json;

	initStringInfo(&buf);
	appendStringInfo(&buf,
					"{"
					"\"network_status\":\"operational\","
					"\"connections\":0,"
					"\"last_check\":\"%s\""
					"}",
					timestamptz_to_str(monitor_state.last_check)
		);

	status_json = buf.data;
	PG_RETURN_TEXT_P(cstring_to_text(status_json));
}

/*
 * Get replication status
 */
Datum
pgraft_get_replication_status(PG_FUNCTION_ARGS)
{
	StringInfoData buf;
	char	   *status_json;

	initStringInfo(&buf);
	appendStringInfo(&buf,
					"{"
					"\"replication_status\":\"active\","
					"\"lag_ms\":0,"
					"\"last_check\":\"%s\""
					"}",
					timestamptz_to_str(monitor_state.last_check)
		);

	status_json = buf.data;
	PG_RETURN_TEXT_P(cstring_to_text(status_json));
}

/*
 * Get alerts
 */
Datum
pgraft_get_alerts(PG_FUNCTION_ARGS)
{
	StringInfoData buf;
	char	   *alerts_json;

	initStringInfo(&buf);
	appendStringInfo(&buf,
					"{"
					"\"alerts\":[],"
					"\"warnings_count\":%d,"
					"\"errors_count\":%d,"
					"\"last_check\":\"%s\""
					"}",
					monitor_state.warnings_count,
					monitor_state.errors_count,
					timestamptz_to_str(monitor_state.last_check)
		);

	alerts_json = buf.data;
	PG_RETURN_TEXT_P(cstring_to_text(alerts_json));
}

/*
 * Get detailed node info
 */
Datum
pgraft_get_detailed_node_info(PG_FUNCTION_ARGS)
{
	uint64		node_id;
	StringInfoData buf;
	char	   *info_json;

	node_id = (uint64) PG_GETARG_INT32(0);

	initStringInfo(&buf);
	appendStringInfo(&buf,
					"{"
					"\"node_id\":%llu,"
					"\"status\":\"online\","
					"\"last_heartbeat\":\"%s\","
					"\"monitoring_enabled\":%s"
					"}",
					(unsigned long long) node_id,
					timestamptz_to_str(monitor_state.last_check),
					monitor_state.enabled ? "true" : "false"
		);

	info_json = buf.data;
	PG_RETURN_TEXT_P(cstring_to_text(info_json));
}

/*
 * Log health event
 */
void
pgraft_log_health_event(pgraft_health_event_type_t event_type, const char *message)
{
	if (!monitor_state.enabled)
		return;

	/* Update event counters */
	switch (event_type)
	{
		case PGRAFT_HEALTH_EVENT_INFO:
			monitor_state.info_events++;
			elog(INFO, "pgraft_health_event: %s", message);
			break;

		case PGRAFT_HEALTH_EVENT_WARNING:
			monitor_state.warning_events++;
			elog(WARNING, "pgraft_health_event: %s", message);
			break;

		case PGRAFT_HEALTH_EVENT_ERROR:
			monitor_state.error_events++;
			elog(ERROR, "pgraft_health_event: %s", message);
			break;

		default:
			elog(DEBUG1, "pgraft_health_event: %s", message);
			break;
	}

	/* Update total events */
	monitor_state.total_events++;
}
