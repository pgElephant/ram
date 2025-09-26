/*-------------------------------------------------------------------------
 *
 * metrics.c
 *		Metrics and monitoring for pgraft extension
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

/* Function declarations */
PG_FUNCTION_INFO_V1(pgraft_get_cluster_health);
PG_FUNCTION_INFO_V1(pgraft_get_performance_metrics);
PG_FUNCTION_INFO_V1(pgraft_is_cluster_healthy);
PG_FUNCTION_INFO_V1(pgraft_get_system_stats);
PG_FUNCTION_INFO_V1(pgraft_get_quorum_status);


/* Metrics storage structure */
typedef struct
{
    int total_requests;
    int successful_requests;
    int failed_requests;
    int64 total_response_time_us;
    int current_connections;
    int max_connections;
    int heartbeat_count;
    int election_count;
    int log_entries_appended;
    int log_entries_committed;
    TimestampTz last_reset;
    TimestampTz last_activity;
} pgraft_metrics_data_t;

/* Health check data */
typedef struct
{
    pgraft_health_status_t overall_status;
    int healthy_nodes;
    int total_nodes;
    int failed_checks;
    int warnings_count;
    int errors_count;
    TimestampTz last_check;
    TimestampTz last_healthy;
} pgraft_health_data_t;

/* Global metrics data */
static pgraft_metrics_data_t g_metrics = {0};
static pgraft_health_data_t g_health = {0};
static bool metrics_initialized = false;

/* Forward declarations */
static void pgraft_metrics_reset(void);
static void pgraft_health_check_internal(void);

/*
 * Initialize metrics system
 */
void
pgraft_metrics_init(void)
{
    if (metrics_initialized)
        return;
    
    memset(&g_metrics, 0, sizeof(g_metrics));
    memset(&g_health, 0, sizeof(g_health));
    
    g_metrics.last_reset = GetCurrentTimestamp();
    g_health.last_check = GetCurrentTimestamp();
    g_health.last_healthy = GetCurrentTimestamp();
    
    metrics_initialized = true;
    elog(INFO, "pgraft: metrics system initialized");
}

/*
 * Reset metrics data
 */
static void
pgraft_metrics_reset(void)
{
    memset(&g_metrics, 0, sizeof(g_metrics));
    g_metrics.last_reset = GetCurrentTimestamp();
    elog(INFO, "pgraft: metrics reset");
}

/*
 * Record a request
 */
void
pgraft_metrics_record_request(bool success, int response_time_ms)
{
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    g_metrics.total_requests++;
    
    if (success)
        g_metrics.successful_requests++;
    else
        g_metrics.failed_requests++;
    
    g_metrics.total_response_time_us += response_time_ms * 1000;
    g_metrics.last_activity = GetCurrentTimestamp();
}

/*
 * Record a connection
 */
void
pgraft_metrics_record_connection(bool connected)
{
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    if (connected)
    {
        g_metrics.current_connections++;
        if (g_metrics.current_connections > g_metrics.max_connections)
            g_metrics.max_connections = g_metrics.current_connections;
    }
    else
    {
        if (g_metrics.current_connections > 0)
            g_metrics.current_connections--;
    }
    
    g_metrics.last_activity = GetCurrentTimestamp();
}

/*
 * Record a heartbeat
 */
void
pgraft_metrics_record_heartbeat(void)
{
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    g_metrics.heartbeat_count++;
    g_metrics.last_activity = GetCurrentTimestamp();
}

/*
 * Record an election
 */
void
pgraft_metrics_record_election(void)
{
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    g_metrics.election_count++;
    g_metrics.last_activity = GetCurrentTimestamp();
}

/*
 * Record log operations
 */
void
pgraft_metrics_record_log_append(void)
{
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    g_metrics.log_entries_appended++;
    g_metrics.last_activity = GetCurrentTimestamp();
}

void
pgraft_metrics_record_log_commit(void)
{
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    g_metrics.log_entries_committed++;
    g_metrics.last_activity = GetCurrentTimestamp();
}

/*
 * Perform health check
 */
static void
pgraft_health_check_internal(void)
{
    TimestampTz now;
    int time_since_last_activity;
    
    now = GetCurrentTimestamp();
    time_since_last_activity = (int)((now - g_metrics.last_activity) / 1000);
    
    /* Reset health status */
    g_health.overall_status = PGRAFT_HEALTH_OK;
    g_health.failed_checks = 0;
    g_health.warnings_count = 0;
    g_health.errors_count = 0;
    
    /* Check if system is responsive */
    if (time_since_last_activity > 30000) /* 30 seconds */
    {
        g_health.overall_status = PGRAFT_HEALTH_ERROR;
        g_health.errors_count++;
    }
    else if (time_since_last_activity > 10000) /* 10 seconds */
    {
        g_health.overall_status = PGRAFT_HEALTH_WARNING;
        g_health.warnings_count++;
    }
    
    /* Check request success rate */
    if (g_metrics.total_requests > 0)
    {
        float success_rate = (float)g_metrics.successful_requests / g_metrics.total_requests;
        if (success_rate < 0.5) /* Less than 50% success rate */
        {
            g_health.overall_status = PGRAFT_HEALTH_CRITICAL;
            g_health.errors_count++;
        }
        else if (success_rate < 0.8) /* Less than 80% success rate */
        {
            if (g_health.overall_status == PGRAFT_HEALTH_OK)
                g_health.overall_status = PGRAFT_HEALTH_WARNING;
            g_health.warnings_count++;
        }
    }
    
    g_health.last_check = now;
    
    if (g_health.overall_status == PGRAFT_HEALTH_OK)
        g_health.last_healthy = now;
}

/*
 * Get performance metrics
 */
Datum
pgraft_get_performance_metrics(PG_FUNCTION_ARGS)
{
    StringInfoData buf;
    char *metrics_json;
    
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    initStringInfo(&buf);
    appendStringInfo(&buf, 
        "{"
        "\"total_requests\":%d,"
        "\"successful_requests\":%d,"
        "\"failed_requests\":%d,"
        "\"success_rate\":%.2f,"
        "\"current_connections\":%d,"
        "\"max_connections\":%d,"
        "\"heartbeat_count\":%d,"
        "\"election_count\":%d,"
        "\"log_entries_appended\":%d,"
        "\"log_entries_committed\":%d,"
        "\"last_activity\":\"%s\","
        "\"uptime_seconds\":%d"
        "}",
        g_metrics.total_requests,
        g_metrics.successful_requests,
        g_metrics.failed_requests,
        g_metrics.total_requests > 0 ? (float)g_metrics.successful_requests / g_metrics.total_requests : 0.0,
        g_metrics.current_connections,
        g_metrics.max_connections,
        g_metrics.heartbeat_count,
        g_metrics.election_count,
        g_metrics.log_entries_appended,
        g_metrics.log_entries_committed,
        timestamptz_to_str(g_metrics.last_activity),
        (int)((GetCurrentTimestamp() - g_metrics.last_reset) / 1000000)
    );
    
    metrics_json = buf.data;
    PG_RETURN_TEXT_P(cstring_to_text(metrics_json));
}

/*
 * Get cluster health status
 */
Datum
pgraft_get_cluster_health(PG_FUNCTION_ARGS)
{
    StringInfoData buf;
    char *health_json;
    
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    /* Perform health check */
    pgraft_health_check_internal();
    
    initStringInfo(&buf);
    appendStringInfo(&buf,
        "{"
        "\"overall_status\":\"%s\","
        "\"healthy_nodes\":%d,"
        "\"total_nodes\":%d,"
        "\"failed_checks\":%d,"
        "\"warnings_count\":%d,"
        "\"errors_count\":%d,"
        "\"last_check\":\"%s\","
        "\"last_healthy\":\"%s\","
        "\"uptime_seconds\":%d"
        "}",
        g_health.overall_status == PGRAFT_HEALTH_OK ? "ok" :
        g_health.overall_status == PGRAFT_HEALTH_WARNING ? "warning" :
        g_health.overall_status == PGRAFT_HEALTH_ERROR ? "error" : "critical",
        g_health.healthy_nodes,
        g_health.total_nodes,
        g_health.failed_checks,
        g_health.warnings_count,
        g_health.errors_count,
        timestamptz_to_str(g_health.last_check),
        timestamptz_to_str(g_health.last_healthy),
        (int)((GetCurrentTimestamp() - g_metrics.last_reset) / 1000000)
    );
    
    health_json = buf.data;
    PG_RETURN_TEXT_P(cstring_to_text(health_json));
}

/*
 * Check if cluster is healthy
 */
Datum
pgraft_is_cluster_healthy(PG_FUNCTION_ARGS)
{
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    /* Perform health check */
    pgraft_health_check_internal();
    
    PG_RETURN_BOOL(g_health.overall_status == PGRAFT_HEALTH_OK);
}

/*
 * Get system statistics
 */
Datum
pgraft_get_system_stats(PG_FUNCTION_ARGS)
{
    StringInfoData buf;
    char *stats_json;
    
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    initStringInfo(&buf);
    appendStringInfo(&buf,
        "{"
        "\"metrics_initialized\":%s,"
        "\"total_requests\":%d,"
        "\"current_connections\":%d,"
        "\"max_connections\":%d,"
        "\"last_activity\":\"%s\","
        "\"uptime_seconds\":%d"
        "}",
        metrics_initialized ? "true" : "false",
        g_metrics.total_requests,
        g_metrics.current_connections,
        g_metrics.max_connections,
        timestamptz_to_str(g_metrics.last_activity),
        (int)((GetCurrentTimestamp() - g_metrics.last_reset) / 1000000)
    );
    
    stats_json = buf.data;
    PG_RETURN_TEXT_P(cstring_to_text(stats_json));
}

/*
 * Get quorum status
 */
Datum
pgraft_get_quorum_status(PG_FUNCTION_ARGS)
{
    StringInfoData buf;
    char *quorum_json;
    int total_nodes;
    int healthy_nodes;
    bool has_quorum;
    
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    /* Perform health check */
    pgraft_health_check_internal();
    
    total_nodes = g_health.total_nodes > 0 ? g_health.total_nodes : 1;
    healthy_nodes = g_health.healthy_nodes > 0 ? g_health.healthy_nodes : 1;
    has_quorum = healthy_nodes > (total_nodes / 2);
    
    initStringInfo(&buf);
    appendStringInfo(&buf,
        "{"
        "\"has_quorum\":%s,"
        "\"total_nodes\":%d,"
        "\"healthy_nodes\":%d,"
        "\"quorum_threshold\":%d,"
        "\"quorum_percentage\":%.1f"
        "}",
        has_quorum ? "true" : "false",
        total_nodes,
        healthy_nodes,
        (total_nodes / 2) + 1,
        (float)healthy_nodes / total_nodes * 100.0
    );
    
    quorum_json = buf.data;
    PG_RETURN_TEXT_P(cstring_to_text(quorum_json));
}

/*
 * Reset metrics
 */
Datum
pgraft_reset_metrics(PG_FUNCTION_ARGS)
{
    if (!metrics_initialized)
        pgraft_metrics_init();
    
    pgraft_metrics_reset();
    
    PG_RETURN_BOOL(true);
}