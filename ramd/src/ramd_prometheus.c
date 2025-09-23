/*
 * ramd_prometheus.c - Prometheus metrics endpoint for ramd
 * 
 * This file implements Prometheus-compatible metrics collection
 * for monitoring the Raft cluster, PostgreSQL nodes, and ramd daemon.
 */

#include "ramd_prometheus.h"
#include "ramd_config.h"
#include "ramd_conn.h"
#include "ramd_pgraft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

/* Global metrics storage */
static ramd_prometheus_metrics_t g_metrics = {0};
static time_t g_metrics_last_update = 0;

/* Prometheus metric types */
#define METRIC_TYPE_COUNTER   "counter"
#define METRIC_TYPE_GAUGE     "gauge"
#define METRIC_TYPE_HISTOGRAM "histogram"
#define METRIC_TYPE_SUMMARY   "summary"

/* Metric collection functions */
static void
ramd_prometheus_collect_system_metrics(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    g_metrics.timestamp = tv.tv_sec;
    g_metrics.uptime_seconds = tv.tv_sec - g_metrics.start_time;
    g_metrics.memory_usage_bytes = 0; /* TODO: Get actual memory usage */
    g_metrics.cpu_usage_percent = 0.0; /* TODO: Get actual CPU usage */
}

static void
ramd_prometheus_collect_postgresql_metrics(PGconn* conn)
{
    PGresult* result;
    char query[512];
    
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK)
    {
        g_metrics.postgresql_connected = 0;
        g_metrics.postgresql_connections = 0;
        return;
    }
    
    g_metrics.postgresql_connected = 1;
    
    /* Get connection count */
    snprintf(query, sizeof(query), 
        "SELECT COUNT(*) FROM pg_stat_activity WHERE state = 'active'");
    
    result = PQexec(conn, query);
    if (result && PQresultStatus(result) == PGRES_TUPLES_OK)
    {
        g_metrics.postgresql_connections = atoi(PQgetvalue(result, 0, 0));
    }
    else
    {
        g_metrics.postgresql_connections = 0;
    }
    
    if (result)
    {
        PQclear(result);
    }
}

static void
ramd_prometheus_collect_raft_metrics(PGconn* conn)
{
    PGresult* result;
    char query[512];
    
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK)
    {
        g_metrics.raft_leader = -1;
        g_metrics.raft_term = -1;
        g_metrics.raft_healthy = 0;
        g_metrics.raft_nodes = 0;
        return;
    }
    
    /* Get Raft leader */
    snprintf(query, sizeof(query), "SELECT pgraft_get_leader()");
    result = PQexec(conn, query);
    if (result && PQresultStatus(result) == PGRES_TUPLES_OK)
    {
        g_metrics.raft_leader = atoi(PQgetvalue(result, 0, 0));
    }
    else
    {
        g_metrics.raft_leader = -1;
    }
    
    if (result)
    {
        PQclear(result);
    }
    
    /* Get Raft term */
    snprintf(query, sizeof(query), "SELECT pgraft_get_term()");
    result = PQexec(conn, query);
    if (result && PQresultStatus(result) == PGRES_TUPLES_OK)
    {
        g_metrics.raft_term = atoi(PQgetvalue(result, 0, 0));
    }
    else
    {
        g_metrics.raft_term = -1;
    }
    
    if (result)
    {
        PQclear(result);
    }
    
    /* Get Raft health */
    snprintf(query, sizeof(query), "SELECT pgraft_is_cluster_healthy()");
    result = PQexec(conn, query);
    if (result && PQresultStatus(result) == PGRES_TUPLES_OK)
    {
        g_metrics.raft_healthy = strcmp(PQgetvalue(result, 0, 0), "true") == 0 ? 1 : 0;
    }
    else
    {
        g_metrics.raft_healthy = 0;
    }
    
    if (result)
    {
        PQclear(result);
    }
    
    /* Get Raft node count */
    snprintf(query, sizeof(query), "SELECT pgraft_get_nodes()");
    result = PQexec(conn, query);
    if (result && PQresultStatus(result) == PGRES_TUPLES_OK)
    {
        const char* nodes_json = PQgetvalue(result, 0, 0);
        if (nodes_json && strcmp(nodes_json, "null") != 0)
        {
            /* Simple count of nodes in JSON array */
            g_metrics.raft_nodes = 1; /* TODO: Parse JSON properly */
        }
        else
        {
            g_metrics.raft_nodes = 0;
        }
    }
    else
    {
        g_metrics.raft_nodes = 0;
    }
    
    if (result)
    {
        PQclear(result);
    }
}

static void
ramd_prometheus_collect_http_metrics(void)
{
    /* HTTP request metrics */
    g_metrics.http_requests_total = g_metrics.http_requests_total + 1;
    g_metrics.http_requests_in_flight = 0; /* TODO: Track in-flight requests */
    g_metrics.http_request_duration_seconds = 0.0; /* TODO: Track request duration */
}

/* Public functions */
void
ramd_prometheus_init(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    memset(&g_metrics, 0, sizeof(ramd_prometheus_metrics_t));
    g_metrics.start_time = tv.tv_sec;
    g_metrics_last_update = 0;
    
    ramd_log_info("Prometheus metrics initialized");
}

void
ramd_prometheus_update_metrics(PGconn* conn)
{
    time_t now;
    time(&now);
    
    /* Update metrics every 5 seconds */
    if (now - g_metrics_last_update < 5)
    {
        return;
    }
    
    ramd_prometheus_collect_system_metrics();
    ramd_prometheus_collect_postgresql_metrics(conn);
    ramd_prometheus_collect_raft_metrics(conn);
    ramd_prometheus_collect_http_metrics();
    
    g_metrics_last_update = now;
}

int
ramd_prometheus_handle_request(const char* request, char* response, size_t response_size)
{
    PGconn* conn;
    char metrics_buffer[8192];
    size_t offset = 0;
    
    (void)request; /* Suppress unused parameter warning */
    
    /* Update metrics before serving */
    conn = ramd_conn_get_cached(1, "127.0.0.1", 5432, "postgres", "postgres", "postgres");
    ramd_prometheus_update_metrics(conn);
    
    /* Build Prometheus metrics response */
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_uptime_seconds Total uptime in seconds\n"
        "# TYPE ramd_uptime_seconds %s\n"
        "ramd_uptime_seconds %ld\n\n",
        METRIC_TYPE_GAUGE, g_metrics.uptime_seconds);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_memory_usage_bytes Memory usage in bytes\n"
        "# TYPE ramd_memory_usage_bytes %s\n"
        "ramd_memory_usage_bytes %ld\n\n",
        METRIC_TYPE_GAUGE, g_metrics.memory_usage_bytes);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_cpu_usage_percent CPU usage percentage\n"
        "# TYPE ramd_cpu_usage_percent %s\n"
        "ramd_cpu_usage_percent %.2f\n\n",
        METRIC_TYPE_GAUGE, g_metrics.cpu_usage_percent);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_postgresql_connected PostgreSQL connection status\n"
        "# TYPE ramd_postgresql_connected %s\n"
        "ramd_postgresql_connected %d\n\n",
        METRIC_TYPE_GAUGE, g_metrics.postgresql_connected);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_postgresql_connections Number of active PostgreSQL connections\n"
        "# TYPE ramd_postgresql_connections %s\n"
        "ramd_postgresql_connections %d\n\n",
        METRIC_TYPE_GAUGE, g_metrics.postgresql_connections);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_raft_leader Current Raft leader node ID\n"
        "# TYPE ramd_raft_leader %s\n"
        "ramd_raft_leader %d\n\n",
        METRIC_TYPE_GAUGE, g_metrics.raft_leader);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_raft_term Current Raft term\n"
        "# TYPE ramd_raft_term %s\n"
        "ramd_raft_term %d\n\n",
        METRIC_TYPE_GAUGE, g_metrics.raft_term);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_raft_healthy Raft cluster health status\n"
        "# TYPE ramd_raft_healthy %s\n"
        "ramd_raft_healthy %d\n\n",
        METRIC_TYPE_GAUGE, g_metrics.raft_healthy);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_raft_nodes Number of Raft nodes\n"
        "# TYPE ramd_raft_nodes %s\n"
        "ramd_raft_nodes %d\n\n",
        METRIC_TYPE_GAUGE, g_metrics.raft_nodes);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_http_requests_total Total HTTP requests\n"
        "# TYPE ramd_http_requests_total %s\n"
        "ramd_http_requests_total %ld\n\n",
        METRIC_TYPE_COUNTER, g_metrics.http_requests_total);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_http_requests_in_flight Current in-flight HTTP requests\n"
        "# TYPE ramd_http_requests_in_flight %s\n"
        "ramd_http_requests_in_flight %d\n\n",
        METRIC_TYPE_GAUGE, g_metrics.http_requests_in_flight);
    
    offset += (size_t)snprintf(metrics_buffer + offset, sizeof(metrics_buffer) - offset,
        "# HELP ramd_http_request_duration_seconds HTTP request duration\n"
        "# TYPE ramd_http_request_duration_seconds %s\n"
        "ramd_http_request_duration_seconds %.3f\n\n",
        METRIC_TYPE_GAUGE, g_metrics.http_request_duration_seconds);
    
    /* Copy to response buffer */
    strncpy(response, metrics_buffer, response_size - 1);
    response[response_size - 1] = '\0';
    
    return 0;
}

void
ramd_prometheus_cleanup(void)
{
    ramd_log_info("Prometheus metrics cleanup completed");
}
