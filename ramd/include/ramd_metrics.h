/*-------------------------------------------------------------------------
 *
 * ramd_metrics.h
 *		PostgreSQL Auto-Failover Daemon - Prometheus Metrics
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_METRICS_H
#define RAMD_METRICS_H

#include "ramd.h"
#include <time.h>

/* Metrics collection context */
typedef struct ramd_metrics_t
{
	bool enabled;
	time_t last_collection;
	int32_t collection_interval_ms;
	
	/* Cluster metrics */
	int32_t cluster_total_nodes;
	int32_t cluster_healthy_nodes;
	int32_t cluster_primary_node_id;
	bool cluster_has_quorum;
	bool cluster_is_leader;
	
	/* Node metrics */
	int32_t node_id;
	char node_hostname[256];
	ramd_node_state_t node_state;
	ramd_role_t node_role;
	bool node_is_healthy;
	time_t node_last_seen;
	float node_health_score;
	int64_t node_wal_lsn;
	int32_t node_replication_lag_ms;
	
	/* Performance metrics */
	int64_t total_health_checks;
	int64_t failed_health_checks;
	int64_t total_failovers;
	int64_t total_promotions;
	int64_t total_demotions;
	time_t last_failover_time;
	time_t last_promotion_time;
	time_t last_demotion_time;
	
	/* HTTP API metrics */
	int64_t http_requests_total;
	int64_t http_requests_2xx;
	int64_t http_requests_4xx;
	int64_t http_requests_5xx;
	int64_t http_request_duration_ms;
	
	/* Replication metrics */
	int32_t replication_lag_max_ms;
	int32_t replication_lag_avg_ms;
	int32_t replication_connections_active;
	int32_t replication_connections_total;
	
	/* Memory and resource metrics */
	size_t memory_usage_bytes;
	int32_t cpu_usage_percent;
	int32_t disk_usage_percent;
	
	/* Timestamps */
	time_t daemon_start_time;
	time_t last_metrics_update;
} ramd_metrics_t;

/* Metrics collection functions */
bool ramd_metrics_init(ramd_metrics_t* metrics);
void ramd_metrics_cleanup(ramd_metrics_t* metrics);
bool ramd_metrics_collect(ramd_metrics_t* metrics, const ramd_cluster_t* cluster);
bool ramd_metrics_update_node(ramd_metrics_t* metrics, const ramd_node_t* node);
bool ramd_metrics_update_cluster(ramd_metrics_t* metrics, const ramd_cluster_t* cluster);

/* Prometheus format output */
char* ramd_metrics_to_prometheus(const ramd_metrics_t* metrics);
void ramd_metrics_free_prometheus_output(char* output);

/* Metrics update functions */
void ramd_metrics_increment_health_checks(ramd_metrics_t* metrics, bool success);
void ramd_metrics_increment_failovers(ramd_metrics_t* metrics);
void ramd_metrics_increment_promotions(ramd_metrics_t* metrics);
void ramd_metrics_increment_demotions(ramd_metrics_t* metrics);
void ramd_metrics_update_http_request(ramd_metrics_t* metrics, int status_code, int duration_ms);
void ramd_metrics_update_replication_lag(ramd_metrics_t* metrics, int32_t lag_ms);
void ramd_metrics_update_resource_usage(ramd_metrics_t* metrics, size_t memory_bytes, int32_t cpu_percent, int32_t disk_percent);

/* Global metrics instance */
extern ramd_metrics_t* g_ramd_metrics;

#endif /* RAMD_METRICS_H */
