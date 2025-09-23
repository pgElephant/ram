/*
 * ramd_prometheus.h - Prometheus metrics definitions for ramd
 * 
 * This header defines the Prometheus metrics structure and functions
 * for monitoring the Raft cluster, PostgreSQL nodes, and ramd daemon.
 */

#ifndef RAMD_PROMETHEUS_H
#define RAMD_PROMETHEUS_H

#include <time.h>
#include <libpq-fe.h>

/* Prometheus metrics structure */
typedef struct ramd_prometheus_metrics_t
{
    /* System metrics */
    time_t timestamp;
    time_t start_time;
    long uptime_seconds;
    long memory_usage_bytes;
    double cpu_usage_percent;
    
    /* PostgreSQL metrics */
    int postgresql_connected;
    int postgresql_connections;
    
    /* Raft metrics */
    int raft_leader;
    int raft_term;
    int raft_healthy;
    int raft_nodes;
    
    /* HTTP metrics */
    long http_requests_total;
    int http_requests_in_flight;
    double http_request_duration_seconds;
    
} ramd_prometheus_metrics_t;

/* Function declarations */
void ramd_prometheus_init(void);
void ramd_prometheus_update_metrics(PGconn* conn);
int ramd_prometheus_handle_request(const char* request, char* response, size_t response_size);
void ramd_prometheus_cleanup(void);

#endif /* RAMD_PROMETHEUS_H */
