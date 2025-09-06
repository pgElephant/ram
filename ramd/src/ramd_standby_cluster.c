/*-------------------------------------------------------------------------
 *
 * ramd_standby_cluster.c
 *		Complete standby cluster implementation for disaster recovery
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <libpq-fe.h>

#include "ramd_standby_cluster.h"
#include "ramd_config.h"
#include "ramd_logging.h"
#include "ramd_defaults.h"
#include "ramd_postgresql.h"
#include "ramd_callbacks.h"

/* Global state */
static ramd_standby_cluster_config_t g_standby_config = {0};
static ramd_standby_cluster_status_t g_standby_status = {0};
static ramd_disaster_recovery_plan_t g_dr_plan = {0};
static bool g_standby_initialized = false;
static pthread_mutex_t g_standby_mutex = PTHREAD_MUTEX_INITIALIZER;

bool ramd_standby_cluster_init(ramd_standby_cluster_config_t* config)
{
	if (!config)
	{
		ramd_log_error("NULL config provided to ramd_standby_cluster_init");
		return false;
	}

	pthread_mutex_lock(&g_standby_mutex);

	if (g_standby_initialized)
	{
		ramd_log_warning("Standby cluster already initialized");
		pthread_mutex_unlock(&g_standby_mutex);
		return true;
	}

	memcpy(&g_standby_config, config, sizeof(ramd_standby_cluster_config_t));

	if (!ramd_standby_cluster_config_validate(&g_standby_config))
	{
		pthread_mutex_unlock(&g_standby_mutex);
		ramd_log_error("Standby cluster configuration validation failed");
		return false;
	}

	memset(&g_standby_status, 0, sizeof(ramd_standby_cluster_status_t));
	g_standby_status.state = RAMD_STANDBY_STATE_UNKNOWN;
	g_standby_status.last_sync_time = time(NULL);

	g_standby_initialized = true;

	pthread_mutex_unlock(&g_standby_mutex);

	if (g_standby_config.enabled)
	{
		ramd_log_info("Standby cluster initialized: %s -> %s",
		              g_standby_config.standby_cluster_name,
		              g_standby_config.primary_cluster_name);

		if (pthread_create(&g_standby_config.monitor_thread, NULL,
		                   ramd_standby_monitor_thread, NULL) != 0)
		{
			ramd_log_error("Failed to start standby cluster monitoring thread");
			return false;
		}

		if (!ramd_standby_cluster_connect_to_primary())
		{
			ramd_log_error("Failed to establish connection to primary cluster");
			pthread_cancel(g_standby_config.monitor_thread);
			return false;
		}

		return ramd_standby_cluster_start_sync();
	}
	else
	{
		ramd_log_info("Standby cluster disabled in configuration");
	}

	return true;
}

void ramd_standby_cluster_cleanup(void)
{
	pthread_mutex_lock(&g_standby_mutex);

	if (!g_standby_initialized)
	{
		pthread_mutex_unlock(&g_standby_mutex);
		return;
	}

	ramd_log_info("Cleaning up standby cluster");

	if (g_standby_config.monitor_thread != 0)
	{
		pthread_cancel(g_standby_config.monitor_thread);
		pthread_join(g_standby_config.monitor_thread, NULL);
		g_standby_config.monitor_thread = 0;
	}

	if (g_standby_config.primary_connection)
	{
		PQfinish(g_standby_config.primary_connection);
		g_standby_config.primary_connection = NULL;
	}

	if (g_standby_config.use_replication_slots)
	{
		ramd_standby_cluster_remove_replication_slot();
	}

	memset(&g_standby_config, 0, sizeof(g_standby_config));
	memset(&g_standby_status, 0, sizeof(g_standby_status));
	g_standby_initialized = false;

	pthread_mutex_unlock(&g_standby_mutex);

	ramd_log_info("Standby cluster cleanup completed");
}

bool ramd_standby_cluster_is_enabled(void)
{
	pthread_mutex_lock(&g_standby_mutex);
	bool enabled = g_standby_initialized && g_standby_config.enabled;
	pthread_mutex_unlock(&g_standby_mutex);
	return enabled;
}

bool ramd_standby_cluster_start_sync(void)
{
	if (!ramd_standby_cluster_is_enabled())
	{
		ramd_log_error("Cannot start sync: standby cluster not enabled");
		return false;
	}

	pthread_mutex_lock(&g_standby_mutex);

	ramd_log_info("Starting standby cluster synchronization with primary: %s",
	              g_standby_config.primary_cluster_endpoint);

	if (!ramd_standby_cluster_setup_replication())
	{
		ramd_log_error("Failed to setup replication from primary cluster");
		return false;
	}

	if (g_standby_config.use_replication_slots)
	{
		if (!ramd_standby_cluster_create_replication_slot())
		{
			ramd_log_warning(
			    "Failed to create replication slot, continuing without slot");
		}
	}

	if (g_standby_config.use_replication_slots)
	{
		if (!ramd_standby_cluster_setup_replication_slot())
		{
			pthread_mutex_unlock(&g_standby_mutex);
			ramd_log_error("Failed to setup replication slot");
			return false;
		}
	}

	g_standby_status.state = RAMD_STANDBY_STATE_SYNCING;
	g_standby_status.last_sync_time = time(NULL);
	g_standby_status.primary_available = true;

	pthread_mutex_unlock(&g_standby_mutex);

	ramd_log_info("Standby cluster synchronization started successfully");
	return true;
}

bool ramd_standby_cluster_stop_sync(void)
{
	if (!ramd_standby_cluster_is_enabled())
		return true;

	pthread_mutex_lock(&g_standby_mutex);

	ramd_log_info("Stopping standby cluster synchronization");

	if (g_standby_config.primary_connection)
	{
		ramd_log_info("Stopping synchronization with primary cluster");

		PQexec(g_standby_config.primary_connection, "SELECT pg_stop_backup()");

		PQfinish(g_standby_config.primary_connection);
		g_standby_config.primary_connection = NULL;
	}

	g_standby_status.state = RAMD_STANDBY_STATE_DISCONNECTED;
	g_standby_status.primary_available = false;

	pthread_mutex_unlock(&g_standby_mutex);

	ramd_log_info("Standby cluster synchronization stopped");
	return true;
}

bool ramd_standby_cluster_promote(void)
{
	if (!ramd_standby_cluster_is_enabled())
	{
		ramd_log_error("Cannot promote: standby cluster not enabled");
		return false;
	}

	pthread_mutex_lock(&g_standby_mutex);

	ramd_log_info("Promoting standby cluster to primary: %s",
	              g_standby_config.standby_cluster_name);

	if (!ramd_standby_cluster_is_promotion_safe())
	{
		pthread_mutex_unlock(&g_standby_mutex);
		ramd_log_error("Promotion not safe at this time");
		return false;
	}

	g_standby_status.state = RAMD_STANDBY_STATE_PROMOTING;

	ramd_callback_context_t context;
	ramd_callback_context_init(&context, RAMD_CALLBACK_PRE_PROMOTE);
	ramd_callback_context_set_data(&context,
	                               "{\"type\":\"standby_cluster_promotion\"}");
	ramd_callback_execute_all(RAMD_CALLBACK_PRE_PROMOTE, &context);

	ramd_log_info("Stopping PostgreSQL to prepare for promotion");
	char stop_cmd[512];
	const char* pgdata = getenv("PGDATA");
	snprintf(stop_cmd, sizeof(stop_cmd), "pg_ctl stop -D %s -m fast",
	         pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR);
	int result = system(stop_cmd);
	if (result != 0)
	{
		ramd_log_warning(
		    "Failed to stop PostgreSQL gracefully, continuing with promotion");
	}

	ramd_log_info("Promoting standby to primary");
	char promote_cmd[512];
	snprintf(promote_cmd, sizeof(promote_cmd), "pg_ctl promote -D %s",
	         pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR);
	result = system(promote_cmd);
	if (result != 0)
	{
		ramd_log_error("Failed to promote standby to primary");
		return false;
	}

	ramd_log_info("Updating configuration for new primary role");

	char start_cmd[512];
	snprintf(start_cmd, sizeof(start_cmd), "pg_ctl start -D %s",
	         pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR);
	result = system(start_cmd);
	if (result != 0)
	{
		ramd_log_error("Failed to start PostgreSQL after promotion");
		return false;
	}

	if (strlen(g_standby_config.promotion_script) > 0)
	{
		ramd_log_info("Executing promotion script: %s",
		              g_standby_config.promotion_script);

		char cmd[1024];
		snprintf(cmd, sizeof(cmd), "%s %s %s",
		         g_standby_config.promotion_script,
		         g_standby_config.standby_cluster_name,
		         g_standby_config.primary_cluster_name);

		int cmd_result = system(cmd);
		if (cmd_result != 0)
		{
			ramd_log_error("Promotion script failed with exit code: %d",
			               cmd_result);
			g_standby_status.state = RAMD_STANDBY_STATE_FAILED;
			pthread_mutex_unlock(&g_standby_mutex);
			return false;
		}
	}

	g_standby_status.state = RAMD_STANDBY_STATE_PROMOTED;
	g_standby_status.last_sync_time = time(NULL);

	pthread_mutex_unlock(&g_standby_mutex);

	ramd_callback_context_init(&context, RAMD_CALLBACK_POST_PROMOTE);
	ramd_callback_execute_all(RAMD_CALLBACK_POST_PROMOTE, &context);

	ramd_log_info("Standby cluster promotion completed successfully");
	return true;
}

bool ramd_standby_cluster_demote(void)
{
	if (!ramd_standby_cluster_is_enabled())
		return true;

	pthread_mutex_lock(&g_standby_mutex);

	ramd_log_info("Demoting cluster back to standby: %s",
	              g_standby_config.standby_cluster_name);

	ramd_log_info("Demoting standby cluster from primary role");

	char stop_cmd[512];
	const char* pgdata = getenv("PGDATA");
	snprintf(stop_cmd, sizeof(stop_cmd), "pg_ctl stop -D %s -m fast",
	         pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR);
	int result = system(stop_cmd);
	if (result != 0)
	{
		ramd_log_warning(
		    "Failed to stop PostgreSQL gracefully during demotion");
	}

	char signal_file[512];
	snprintf(signal_file, sizeof(signal_file), "%s/standby.signal",
	         pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR);
	FILE* fp = fopen(signal_file, "w");
	if (fp)
	{
		fclose(fp);
		ramd_log_info("Created standby.signal file for demotion");
	}

	char start_cmd[512];
	snprintf(start_cmd, sizeof(start_cmd), "pg_ctl start -D %s",
	         pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR);
	result = system(start_cmd);
	if (result != 0)
	{
		ramd_log_error("Failed to start PostgreSQL as standby after demotion");
		return false;
	}

	g_standby_status.state = RAMD_STANDBY_STATE_SYNCING;

	pthread_mutex_unlock(&g_standby_mutex);

	ramd_log_info("Cluster demotion completed");
	return true;
}

bool ramd_standby_cluster_get_status(ramd_standby_cluster_status_t* status)
{
	if (!status)
		return false;

	pthread_mutex_lock(&g_standby_mutex);

	if (!g_standby_initialized)
	{
		pthread_mutex_unlock(&g_standby_mutex);
		return false;
	}

	ramd_standby_cluster_update_lag_info();

	memcpy(status, &g_standby_status, sizeof(ramd_standby_cluster_status_t));

	pthread_mutex_unlock(&g_standby_mutex);

	return true;
}

ramd_standby_cluster_state_t ramd_standby_cluster_get_state(void)
{
	pthread_mutex_lock(&g_standby_mutex);
	ramd_standby_cluster_state_t state = g_standby_status.state;
	pthread_mutex_unlock(&g_standby_mutex);
	return state;
}

bool ramd_standby_cluster_check_primary_health(void)
{
	if (!ramd_standby_cluster_is_enabled())
		return false;

	ramd_log_debug("Checking primary cluster health: %s:%d",
	               g_standby_config.primary_host,
	               g_standby_config.primary_port);

	char conninfo[1024];
	PGconn* health_conn;
	PGresult* res;
	bool healthy = false;

	snprintf(conninfo, sizeof(conninfo),
	         "host=%s port=%d dbname=%s user=%s connect_timeout=5",
	         g_standby_config.primary_host, g_standby_config.primary_port,
	         g_standby_config.primary_database,
	         g_standby_config.replication_user);

	health_conn = PQconnectdb(conninfo);

	if (PQstatus(health_conn) == CONNECTION_OK)
	{
		res = PQexec(health_conn,
		             "SELECT pg_is_in_recovery(), current_timestamp");

		if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
		{
			char* is_in_recovery = PQgetvalue(res, 0, 0);

			if (strcmp(is_in_recovery, "f") == 0)
			{
				healthy = true;
				ramd_log_debug("Primary cluster health check passed");
			}
			else
			{
				ramd_log_warning("Primary cluster is in recovery mode");
			}
		}
		else
		{
			ramd_log_warning("Primary cluster health check query failed: %s",
			                 PQerrorMessage(health_conn));
		}

		PQclear(res);
		PQfinish(health_conn);
	}
	else
	{
		ramd_log_warning(
		    "Failed to connect to primary cluster for health check: %s",
		    PQerrorMessage(health_conn));
		PQfinish(health_conn);
	}

	pthread_mutex_lock(&g_standby_mutex);

	if (g_standby_status.primary_available != healthy)
	{
		g_standby_status.primary_available = healthy;

		if (!healthy)
		{
			ramd_log_warning("Primary cluster health check failed");
			g_standby_status.state = RAMD_STANDBY_STATE_DISCONNECTED;

			if (g_standby_config.auto_promote_on_failure)
			{
				ramd_log_info(
				    "Auto-promotion triggered due to primary failure");
				pthread_mutex_unlock(&g_standby_mutex);
				return ramd_standby_cluster_promote();
			}
		}
		else
		{
			ramd_log_info("Primary cluster health restored");
			g_standby_status.state = RAMD_STANDBY_STATE_SYNCHRONIZED;
		}
	}

	pthread_mutex_unlock(&g_standby_mutex);

	return healthy;
}

bool ramd_standby_cluster_update_lag_info(void)
{
	pthread_mutex_lock(&g_standby_mutex);

	long lag_ms = -1;

	if (g_standby_config.primary_connection &&
	    PQstatus(g_standby_config.primary_connection) == CONNECTION_OK)
	{
		PGresult* res = PQexec(g_standby_config.primary_connection,
		                       "SELECT EXTRACT(EPOCH FROM (now() - "
		                       "pg_last_xact_replay_timestamp())) * 1000");

		if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
		{
			char* lag_str = PQgetvalue(res, 0, 0);
			if (lag_str && strlen(lag_str) > 0 && strcmp(lag_str, "") != 0)
			{
				lag_ms = (long) atof(lag_str);
			}
		}
		PQclear(res);
	}

	if (lag_ms >= 0)
	{
		g_standby_status.lag_seconds = (int) (lag_ms / 1000);
		g_standby_status.lag_bytes = lag_ms * 100;
		ramd_log_debug("Replication lag: %ld ms (%d seconds)", lag_ms,
		               g_standby_status.lag_seconds);
	}
	else
	{
		g_standby_status.lag_bytes = 0;
		g_standby_status.lag_seconds = 0;
		ramd_log_debug("Unable to measure replication lag, setting to 0");
	}

	if (g_standby_status.lag_bytes > g_standby_config.max_lag_bytes)
	{
		g_standby_status.state = RAMD_STANDBY_STATE_LAGGED;
		snprintf(g_standby_status.last_error,
		         sizeof(g_standby_status.last_error),
		         "Replication lag exceeds threshold: %lld bytes",
		         (long long) g_standby_status.lag_bytes);
	}
	else if (g_standby_status.state == RAMD_STANDBY_STATE_LAGGED)
	{
		g_standby_status.state = RAMD_STANDBY_STATE_SYNCHRONIZED;
		g_standby_status.last_error[0] = '\0';
	}

	pthread_mutex_unlock(&g_standby_mutex);

	return true;
}

bool ramd_standby_cluster_setup_replication_slot(void)
{
	if (!g_standby_config.use_replication_slots)
		return true;

	ramd_log_info("Setting up replication slot: %s",
	              g_standby_config.slot_name);

	ramd_log_info("Creating replication slot on primary cluster");

	if (!g_standby_config.primary_connection)
	{
		ramd_log_error("No connection to primary cluster");
		return false;
	}

	char slot_query[512];
	snprintf(slot_query, sizeof(slot_query),
	         "SELECT pg_create_physical_replication_slot('%s')",
	         g_standby_config.replication_slot_name);

	PGresult* res = PQexec(g_standby_config.primary_connection, slot_query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		ramd_log_info("Replication slot '%s' created successfully",
		              g_standby_config.replication_slot_name);
		PQclear(res);
		return true;
	}
	else
	{
		ramd_log_error("Failed to create replication slot: %s",
		               PQerrorMessage(g_standby_config.primary_connection));
		PQclear(res);
		return false;
	}
}

bool ramd_standby_cluster_remove_replication_slot(void)
{
	if (!g_standby_config.use_replication_slots)
		return true;

	ramd_log_info("Removing replication slot: %s", g_standby_config.slot_name);

	ramd_log_info("Removing replication slot from primary cluster");

	if (!g_standby_config.primary_connection)
	{
		ramd_log_warning("No connection to primary cluster for slot removal");
		return true;
	}

	char slot_query[512];
	snprintf(slot_query, sizeof(slot_query),
	         "SELECT pg_drop_replication_slot('%s')",
	         g_standby_config.replication_slot_name);

	PGresult* res = PQexec(g_standby_config.primary_connection, slot_query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		ramd_log_info("Replication slot '%s' removed successfully",
		              g_standby_config.replication_slot_name);
		PQclear(res);
		return true;
	}
	else
	{
		ramd_log_warning("Failed to remove replication slot: %s",
		                 PQerrorMessage(g_standby_config.primary_connection));
		PQclear(res);
		return false;
	}
}

void ramd_standby_cluster_config_set_defaults(
    ramd_standby_cluster_config_t* config)
{
	if (!config)
		return;

	memset(config, 0, sizeof(ramd_standby_cluster_config_t));

	config->enabled = false;
	strcpy(config->primary_cluster_name, "primary_cluster");
	config->primary_cluster_endpoint[0] = '\0';
	strcpy(config->standby_cluster_name, "standby_cluster");
	config->sync_interval_seconds = 30;
	config->promotion_timeout_seconds = 300;
	config->auto_promote_on_failure = false;
	config->max_lag_bytes = 16 * 1024 * 1024;
	config->use_replication_slots = true;
	strcpy(config->slot_name, "standby_cluster_slot");
}

bool ramd_standby_cluster_config_validate(ramd_standby_cluster_config_t* config)
{
	if (!config)
		return false;

	if (config->enabled)
	{
		if (strlen(config->primary_cluster_name) == 0)
		{
			ramd_log_error("Primary cluster name cannot be empty");
			return false;
		}

		if (strlen(config->primary_cluster_endpoint) == 0)
		{
			ramd_log_error("Primary cluster endpoint cannot be empty");
			return false;
		}

		if (config->sync_interval_seconds < 1)
		{
			ramd_log_error("Sync interval must be at least 1 second");
			return false;
		}

		if (config->promotion_timeout_seconds < 10)
		{
			ramd_log_error("Promotion timeout must be at least 10 seconds");
			return false;
		}
	}

	return true;
}

bool ramd_standby_cluster_is_promotion_safe(void)
{
	bool safe = true;
	int primary_check_attempts = 3;
	int primary_check_interval = 5;
	int i;
	time_t current_time;
	double uptime_hours;
	long lag_ms;

	if (g_standby_status.connected_nodes <
	    g_standby_config.min_nodes_for_quorum)
	{
		ramd_log_warning(
		    "Insufficient connected nodes for safe promotion: %d/%d",
		    g_standby_status.connected_nodes,
		    g_standby_config.min_nodes_for_quorum);
		return false;
	}

	lag_ms = ramd_standby_cluster_get_replication_lag();
	if (lag_ms > g_standby_config.max_lag_ms)
	{
		ramd_log_warning("Replication lag too high for safe promotion: %ld ms",
		                 lag_ms);
		return false;
	}

	for (i = 0; i < primary_check_attempts; i++)
	{
		if (ramd_standby_cluster_check_primary_health())
		{
			ramd_log_error("Primary appears to be healthy on attempt %d/%d, "
			               "aborting promotion to avoid split-brain",
			               i + 1, primary_check_attempts);
			return false;
		}
		if (i < primary_check_attempts - 1)
		{
			sleep(primary_check_interval);
		}
	}

	pthread_mutex_lock(&g_standby_mutex);

	if (g_standby_status.state != RAMD_STANDBY_STATE_READY)
	{
		ramd_log_warning("Cluster not in ready state: %s",
		                 ramd_standby_state_to_string(g_standby_status.state));
		safe = false;
	}

	if (g_standby_status.lag_bytes > g_standby_config.max_lag_bytes)
	{
		ramd_log_warning("Replication lag exceeds maximum: %ld bytes",
		                 g_standby_status.lag_bytes);
		safe = false;
	}

	if (g_standby_status.maintenance_mode)
	{
		ramd_log_warning("Cannot promote during maintenance mode");
		safe = false;
	}

	if (g_standby_status.disk_usage_percent > 90 ||
	    g_standby_status.memory_usage_percent > 90)
	{
		ramd_log_warning("Insufficient system resources for promotion");
		safe = false;
	}

	current_time = time(NULL);
	uptime_hours = difftime(current_time, g_standby_status.start_time) / 3600.0;
	if (uptime_hours < 1.0)
	{
		ramd_log_warning("System uptime too low for safe promotion: %.1f hours",
		                 uptime_hours);
		safe = false;
	}

	pthread_mutex_unlock(&g_standby_mutex);

	ramd_log_debug("Promotion safety check: %s", safe ? "SAFE" : "NOT SAFE");

	return safe;
}

bool ramd_disaster_recovery_init(ramd_disaster_recovery_plan_t* plan)
{
	if (!plan)
		return false;

	memcpy(&g_dr_plan, plan, sizeof(ramd_disaster_recovery_plan_t));

	ramd_log_info("Disaster recovery plan initialized: %s (RTO=%ds, RPO=%ds)",
	              g_dr_plan.plan_name, g_dr_plan.rto_seconds,
	              g_dr_plan.rpo_seconds);

	return true;
}

bool ramd_disaster_recovery_execute_failover(void)
{
	ramd_log_info("Executing disaster recovery failover plan: %s",
	              g_dr_plan.plan_name);

	ramd_log_error("Executing disaster recovery failover");

	int check_count = 0;
	bool primary_confirmed_down = true;

	for (check_count = 0; check_count < 3; check_count++)
	{
		if (ramd_standby_cluster_check_primary_health())
		{
			primary_confirmed_down = false;
			break;
		}
		sleep(5);
	}

	if (!primary_confirmed_down)
	{
		ramd_log_warning(
		    "Primary cluster appears to be back online, aborting DR failover");
		return false;
	}

	ramd_standby_cluster_stop_sync();

	if (!ramd_standby_cluster_promote())
	{
		ramd_log_error("Failed to promote standby during DR failover");
		return false;
	}

	if (strlen(g_dr_plan.dns_update_script) > 0)
	{
		ramd_log_info("Updating DNS: %s", g_dr_plan.dns_update_script);
		system(g_dr_plan.dns_update_script);
	}

	ramd_log_error("DR failover completed - this cluster is now the primary");

	g_standby_config.is_promoted = true;

	if (strlen(g_dr_plan.failover_script) > 0)
	{
		ramd_log_info("Executing DR failover script: %s",
		              g_dr_plan.failover_script);
		int result = system(g_dr_plan.failover_script);
		if (result != 0)
		{
			ramd_log_error("DR failover script failed: %d", result);
			return false;
		}
	}

	if (!ramd_standby_cluster_promote())
	{
		ramd_log_error("Failed to promote standby during DR failover");
		return false;
	}

	ramd_log_info("Disaster recovery failover completed successfully");
	return true;
}

const char*
ramd_standby_cluster_state_to_string(ramd_standby_cluster_state_t state)
{
	switch (state)
	{
	case RAMD_STANDBY_STATE_UNKNOWN:
		return "unknown";
	case RAMD_STANDBY_STATE_SYNCING:
		return "syncing";
	case RAMD_STANDBY_STATE_SYNCHRONIZED:
		return "synchronized";
	case RAMD_STANDBY_STATE_LAGGED:
		return "lagged";
	case RAMD_STANDBY_STATE_DISCONNECTED:
		return "disconnected";
	case RAMD_STANDBY_STATE_PROMOTING:
		return "promoting";
	case RAMD_STANDBY_STATE_PROMOTED:
		return "promoted";
	case RAMD_STANDBY_STATE_FAILED:
		return "failed";
	default:
		return "invalid";
	}
}

/*
 * Monitor thread for primary cluster health
 */
void* ramd_standby_monitor_thread(void* arg)
{
	(void) arg; /* Suppress unused parameter warning */
	ramd_log_info("Standby cluster monitor thread started");

	while (g_standby_initialized)
	{
		bool primary_healthy = ramd_standby_cluster_check_primary_health();

		if (!primary_healthy)
		{
			ramd_log_warning("Primary cluster health check failed");

			time_t current_time = time(NULL);
			if (current_time - g_standby_config.last_primary_contact >
			    g_dr_plan.failover_timeout)
			{
				ramd_log_error("Primary cluster unavailable for %ld seconds, "
				               "considering DR failover",
				               current_time -
				                   g_standby_config.last_primary_contact);

				if (g_dr_plan.auto_failover_enabled)
				{
					ramd_log_info("Initiating automatic DR failover");
					if (!ramd_standby_cluster_execute_dr_failover())
					{
						ramd_log_error("Automatic DR failover failed");
					}
					break; /* Exit monitoring loop after failover attempt */
				}
			}
		}
		else
		{
			g_standby_config.last_primary_contact = time(NULL);
		}

		long lag_ms = ramd_standby_cluster_get_replication_lag();
		if (lag_ms > g_standby_config.max_lag_ms)
		{
			ramd_log_warning("Replication lag too high: %ld ms", lag_ms);
		}

		/* Sleep before next check */
		sleep((unsigned int) g_standby_config.health_check_interval);
	}

	ramd_log_info("Standby cluster monitor thread stopped");
	return NULL;
}

/*
 * Connect to primary cluster
 */
bool ramd_standby_cluster_connect_to_primary(void)
{
	char conninfo[1024];

	snprintf(conninfo, sizeof(conninfo), "host=%s port=%d dbname=%s user=%s",
	         g_standby_config.primary_host, g_standby_config.primary_port,
	         g_standby_config.primary_database,
	         g_standby_config.replication_user);

	if (strlen(g_standby_config.replication_password) > 0)
	{
		char password_part[256];
		snprintf(password_part, sizeof(password_part), " password=%s",
		         g_standby_config.replication_password);
		strncat(conninfo, password_part,
		        sizeof(conninfo) - strlen(conninfo) - 1);
	}

	g_standby_config.primary_connection = PQconnectdb(conninfo);

	if (PQstatus(g_standby_config.primary_connection) != CONNECTION_OK)
	{
		ramd_log_error("Failed to connect to primary cluster: %s",
		               PQerrorMessage(g_standby_config.primary_connection));
		PQfinish(g_standby_config.primary_connection);
		g_standby_config.primary_connection = NULL;
		return false;
	}

	ramd_log_info("Successfully connected to primary cluster");
	return true;
}

/*
 * Setup replication from primary
 */
bool ramd_standby_cluster_setup_replication(void)
{
	if (!g_standby_config.primary_connection)
	{
		ramd_log_error("No connection to primary cluster");
		return false;
	}

	/* This would involve:
	 * 1. Taking a base backup from primary
	 * 2. Setting up recovery.conf or postgresql.auto.conf
	 * 3. Starting PostgreSQL in standby mode
	 */

	ramd_log_info("Setting up replication from primary cluster");

	/* Execute pg_basebackup to get initial data */
	char basebackup_cmd[1024];
	snprintf(basebackup_cmd, sizeof(basebackup_cmd),
	         "pg_basebackup -h %s -p %d -U %s -D %s -Ft -z -P -W",
	         g_standby_config.primary_host, g_standby_config.primary_port,
	         g_standby_config.replication_user,
	         pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR);

	ramd_log_info("Starting base backup: %s", basebackup_cmd);
	int result = system(basebackup_cmd);
	if (result != 0)
	{
		ramd_log_error("Base backup failed with exit code: %d", result);
		return false;
	}

	/* Create standby.signal file for PostgreSQL 12+ */
	char signal_file[512];
	const char* pgdata = getenv("PGDATA");
	snprintf(signal_file, sizeof(signal_file), "%s/standby.signal",
	         pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR);
	FILE* fp = fopen(signal_file, "w");
	if (fp)
	{
		fclose(fp);
		ramd_log_info("Created standby.signal file");
	}
	else
	{
		ramd_log_warning("Failed to create standby.signal file");
	}

	return true;
}

/*
 * Get replication lag in milliseconds
 */
long ramd_standby_cluster_get_replication_lag(void)
{
	if (!g_standby_config.primary_connection)
		return -1;

	PGresult* res = PQexec((PGconn*) g_standby_config.primary_connection,
	                       "SELECT EXTRACT(EPOCH FROM (now() - "
	                       "pg_last_xact_replay_timestamp())) * 1000");

	long lag_ms = -1;
	if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
	{
		char* lag_str = PQgetvalue(res, 0, 0);
		if (lag_str && strlen(lag_str) > 0)
		{
			lag_ms = (long) atof(lag_str);
		}
	}
	PQclear(res);

	return lag_ms;
}

/*
 * Create replication slot on primary
 */
bool ramd_standby_cluster_create_replication_slot(void)
{
	return ramd_standby_cluster_setup_replication_slot();
}

/*
 * Execute DR failover
 */
bool ramd_standby_cluster_execute_dr_failover(void)
{
	return ramd_disaster_recovery_execute_failover();
}
