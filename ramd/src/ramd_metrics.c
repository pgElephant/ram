/*-------------------------------------------------------------------------
 *
 * ramd_metrics.c
 *		PostgreSQL Auto-Failover Daemon - Prometheus Metrics Implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ramd_metrics.h"
#include "ramd_logging.h"
#include "ramd_cluster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

/* Global metrics instance */
ramd_metrics_t* g_ramd_metrics = NULL;

bool ramd_metrics_init(ramd_metrics_t* metrics)
{
	if (!metrics)
		return false;

	memset(metrics, 0, sizeof(ramd_metrics_t));
	
	metrics->enabled = true;
	metrics->collection_interval_ms = 5000; /* 5 seconds */
	metrics->daemon_start_time = time(NULL);
	metrics->last_metrics_update = time(NULL);
	
	/* Initialize counters */
	metrics->total_health_checks = 0;
	metrics->failed_health_checks = 0;
	metrics->total_failovers = 0;
	metrics->total_promotions = 0;
	metrics->total_demotions = 0;
	metrics->http_requests_total = 0;
	metrics->http_requests_2xx = 0;
	metrics->http_requests_4xx = 0;
	metrics->http_requests_5xx = 0;
	
	ramd_log_info("Metrics collection subsystem initialized");
	return true;
}

void ramd_metrics_cleanup(ramd_metrics_t* metrics)
{
	if (!metrics)
		return;
		
	ramd_log_info("Cleaning up metrics collection");
	memset(metrics, 0, sizeof(ramd_metrics_t));
}

bool ramd_metrics_collect(ramd_metrics_t* metrics, const ramd_cluster_t* cluster)
{
	if (!metrics || !cluster)
		return false;
		
	time_t now = time(NULL);
	
	/* Update cluster-level metrics */
	metrics->cluster_total_nodes = cluster->node_count;
	metrics->cluster_healthy_nodes = ramd_cluster_count_healthy_nodes(cluster);
	metrics->cluster_primary_node_id = cluster->primary_node_id;
	metrics->cluster_has_quorum = ramd_cluster_has_quorum(cluster);
	metrics->cluster_is_leader = (cluster->leader_node_id == cluster->local_node_id);
	
	/* Update local node metrics */
	ramd_node_t* local_node = ramd_cluster_get_local_node((ramd_cluster_t*)cluster);
	if (local_node)
	{
		metrics->node_id = local_node->node_id;
		strncpy(metrics->node_hostname, local_node->hostname, sizeof(metrics->node_hostname) - 1);
		metrics->node_state = local_node->state;
		metrics->node_role = local_node->role;
		metrics->node_is_healthy = local_node->is_healthy;
		metrics->node_last_seen = local_node->last_seen;
		metrics->node_health_score = local_node->health_score;
		metrics->node_wal_lsn = local_node->wal_lsn;
		metrics->node_replication_lag_ms = local_node->replication_lag_ms;
	}
	
	/* Update resource usage */
	struct rusage usage;
	if (getrusage(RUSAGE_SELF, &usage) == 0)
	{
		metrics->memory_usage_bytes = (size_t)(usage.ru_maxrss * 1024); /* Convert to bytes */
	}
	
	/* Update replication metrics */
	metrics->replication_lag_max_ms = 0;
	metrics->replication_lag_avg_ms = 0;
	metrics->replication_connections_active = 0;
	metrics->replication_connections_total = cluster->node_count - 1; /* All except primary */
	
	for (int i = 0; i < cluster->node_count; i++)
	{
		if (cluster->nodes[i].node_id != cluster->primary_node_id)
		{
			if (cluster->nodes[i].is_healthy)
				metrics->replication_connections_active++;
				
			if (cluster->nodes[i].replication_lag_ms > metrics->replication_lag_max_ms)
				metrics->replication_lag_max_ms = cluster->nodes[i].replication_lag_ms;
				
			metrics->replication_lag_avg_ms += cluster->nodes[i].replication_lag_ms;
		}
	}
	
	if (metrics->replication_connections_total > 0)
		metrics->replication_lag_avg_ms /= metrics->replication_connections_total;
	
	metrics->last_collection = now;
	metrics->last_metrics_update = now;
	
	return true;
}

bool ramd_metrics_update_node(ramd_metrics_t* metrics, const ramd_node_t* node)
{
	if (!metrics || !node)
		return false;
		
	/* Update node-specific metrics if this is the local node */
	if (node->node_id == metrics->node_id)
	{
		metrics->node_state = node->state;
		metrics->node_role = node->role;
		metrics->node_is_healthy = node->is_healthy;
		metrics->node_last_seen = node->last_seen;
		metrics->node_health_score = node->health_score;
		metrics->node_wal_lsn = node->wal_lsn;
		metrics->node_replication_lag_ms = node->replication_lag_ms;
	}
	
	return true;
}

bool ramd_metrics_update_cluster(ramd_metrics_t* metrics, const ramd_cluster_t* cluster)
{
	if (!metrics || !cluster)
		return false;
		
	metrics->cluster_total_nodes = cluster->node_count;
	metrics->cluster_healthy_nodes = ramd_cluster_count_healthy_nodes(cluster);
	metrics->cluster_primary_node_id = cluster->primary_node_id;
	metrics->cluster_has_quorum = ramd_cluster_has_quorum(cluster);
	metrics->cluster_is_leader = (cluster->leader_node_id == cluster->local_node_id);
	
	return true;
}

char* ramd_metrics_to_prometheus(const ramd_metrics_t* metrics)
{
	if (!metrics)
		return NULL;
		
	char* output = malloc(8192); /* Allocate buffer for Prometheus output */
	if (!output)
		return NULL;
		
	char* ptr = output;
	size_t remaining = 8192;
	
	/* Cluster metrics */
	ptr += snprintf(ptr, remaining,
		"# HELP ramd_cluster_nodes_total Total number of nodes in cluster\n"
		"# TYPE ramd_cluster_nodes_total gauge\n"
		"ramd_cluster_nodes_total %d\n"
		"# HELP ramd_cluster_nodes_healthy Number of healthy nodes in cluster\n"
		"# TYPE ramd_cluster_nodes_healthy gauge\n"
		"ramd_cluster_nodes_healthy %d\n"
		"# HELP ramd_cluster_has_quorum Whether cluster has quorum (1=yes, 0=no)\n"
		"# TYPE ramd_cluster_has_quorum gauge\n"
		"ramd_cluster_has_quorum %d\n"
		"# HELP ramd_cluster_is_leader Whether this node is the leader (1=yes, 0=no)\n"
		"# TYPE ramd_cluster_is_leader gauge\n"
		"ramd_cluster_is_leader %d\n"
		"# HELP ramd_cluster_primary_node_id ID of the primary node\n"
		"# TYPE ramd_cluster_primary_node_id gauge\n"
		"ramd_cluster_primary_node_id %d\n",
		metrics->cluster_total_nodes,
		metrics->cluster_healthy_nodes,
		metrics->cluster_has_quorum ? 1 : 0,
		metrics->cluster_is_leader ? 1 : 0,
		metrics->cluster_primary_node_id);
	
	remaining -= (size_t)(ptr - output);
	
	/* Node metrics */
	ptr += snprintf(ptr, remaining,
		"# HELP ramd_node_healthy Whether this node is healthy (1=yes, 0=no)\n"
		"# TYPE ramd_node_healthy gauge\n"
		"ramd_node_healthy{node_id=\"%d\",hostname=\"%s\"} %d\n"
		"# HELP ramd_node_health_score Health score of this node (0.0-1.0)\n"
		"# TYPE ramd_node_health_score gauge\n"
		"ramd_node_health_score{node_id=\"%d\",hostname=\"%s\"} %.2f\n"
		"# HELP ramd_node_replication_lag_ms Replication lag in milliseconds\n"
		"# TYPE ramd_node_replication_lag_ms gauge\n"
		"ramd_node_replication_lag_ms{node_id=\"%d\",hostname=\"%s\"} %d\n"
		"# HELP ramd_node_wal_lsn Current WAL LSN position\n"
		"# TYPE ramd_node_wal_lsn gauge\n"
		"ramd_node_wal_lsn{node_id=\"%d\",hostname=\"%s\"} %lld\n",
		metrics->node_id, metrics->node_hostname, metrics->node_is_healthy ? 1 : 0,
		metrics->node_id, metrics->node_hostname, metrics->node_health_score,
		metrics->node_id, metrics->node_hostname, metrics->node_replication_lag_ms,
		metrics->node_id, metrics->node_hostname, metrics->node_wal_lsn);
	
	remaining -= (size_t)(ptr - output);
	
	/* Performance metrics */
	ptr += snprintf(ptr, remaining,
		"# HELP ramd_health_checks_total Total number of health checks performed\n"
		"# TYPE ramd_health_checks_total counter\n"
		"ramd_health_checks_total %lld\n"
		"# HELP ramd_health_checks_failed Total number of failed health checks\n"
		"# TYPE ramd_health_checks_failed counter\n"
		"ramd_health_checks_failed %lld\n"
		"# HELP ramd_failovers_total Total number of failovers performed\n"
		"# TYPE ramd_failovers_total counter\n"
		"ramd_failovers_total %lld\n"
		"# HELP ramd_promotions_total Total number of node promotions\n"
		"# TYPE ramd_promotions_total counter\n"
		"ramd_promotions_total %lld\n"
		"# HELP ramd_demotions_total Total number of node demotions\n"
		"# TYPE ramd_demotions_total counter\n"
		"ramd_demotions_total %lld\n",
		metrics->total_health_checks,
		metrics->failed_health_checks,
		metrics->total_failovers,
		metrics->total_promotions,
		metrics->total_demotions);
	
	remaining -= (size_t)(ptr - output);
	
	/* HTTP API metrics */
	ptr += snprintf(ptr, remaining,
		"# HELP ramd_http_requests_total Total number of HTTP requests\n"
		"# TYPE ramd_http_requests_total counter\n"
		"ramd_http_requests_total %lld\n"
		"# HELP ramd_http_requests_2xx Total number of 2xx HTTP responses\n"
		"# TYPE ramd_http_requests_2xx counter\n"
		"ramd_http_requests_2xx %lld\n"
		"# HELP ramd_http_requests_4xx Total number of 4xx HTTP responses\n"
		"# TYPE ramd_http_requests_4xx counter\n"
		"ramd_http_requests_4xx %lld\n"
		"# HELP ramd_http_requests_5xx Total number of 5xx HTTP responses\n"
		"# TYPE ramd_http_requests_5xx counter\n"
		"ramd_http_requests_5xx %lld\n",
		metrics->http_requests_total,
		metrics->http_requests_2xx,
		metrics->http_requests_4xx,
		metrics->http_requests_5xx);
	
	remaining -= (size_t)(ptr - output);
	
	/* Replication metrics */
	ptr += snprintf(ptr, remaining,
		"# HELP ramd_replication_lag_max_ms Maximum replication lag in milliseconds\n"
		"# TYPE ramd_replication_lag_max_ms gauge\n"
		"ramd_replication_lag_max_ms %d\n"
		"# HELP ramd_replication_lag_avg_ms Average replication lag in milliseconds\n"
		"# TYPE ramd_replication_lag_avg_ms gauge\n"
		"ramd_replication_lag_avg_ms %d\n"
		"# HELP ramd_replication_connections_active Number of active replication connections\n"
		"# TYPE ramd_replication_connections_active gauge\n"
		"ramd_replication_connections_active %d\n"
		"# HELP ramd_replication_connections_total Total number of replication connections\n"
		"# TYPE ramd_replication_connections_total gauge\n"
		"ramd_replication_connections_total %d\n",
		metrics->replication_lag_max_ms,
		metrics->replication_lag_avg_ms,
		metrics->replication_connections_active,
		metrics->replication_connections_total);
	
	remaining -= (size_t)(ptr - output);
	
	/* Resource metrics */
	ptr += snprintf(ptr, remaining,
		"# HELP ramd_memory_usage_bytes Memory usage in bytes\n"
		"# TYPE ramd_memory_usage_bytes gauge\n"
		"ramd_memory_usage_bytes %zu\n"
		"# HELP ramd_daemon_uptime_seconds Daemon uptime in seconds\n"
		"# TYPE ramd_daemon_uptime_seconds gauge\n"
		"ramd_daemon_uptime_seconds %ld\n",
		metrics->memory_usage_bytes,
		time(NULL) - metrics->daemon_start_time);
	
	/* Add final newline */
	*ptr = '\n';
	ptr++;
	*ptr = '\0';
	
	return output;
}

void ramd_metrics_free_prometheus_output(char* output)
{
	if (output)
		free(output);
}

/* Metrics update functions */
void ramd_metrics_increment_health_checks(ramd_metrics_t* metrics, bool success)
{
	if (!metrics)
		return;
		
	metrics->total_health_checks++;
	if (!success)
		metrics->failed_health_checks++;
}

void ramd_metrics_increment_failovers(ramd_metrics_t* metrics)
{
	if (!metrics)
		return;
		
	metrics->total_failovers++;
	metrics->last_failover_time = time(NULL);
}

void ramd_metrics_increment_promotions(ramd_metrics_t* metrics)
{
	if (!metrics)
		return;
		
	metrics->total_promotions++;
	metrics->last_promotion_time = time(NULL);
}

void ramd_metrics_increment_demotions(ramd_metrics_t* metrics)
{
	if (!metrics)
		return;
		
	metrics->total_demotions++;
	metrics->last_demotion_time = time(NULL);
}

void ramd_metrics_update_http_request(ramd_metrics_t* metrics, int status_code, int duration_ms)
{
	if (!metrics)
		return;
		
	metrics->http_requests_total++;
	metrics->http_request_duration_ms = duration_ms;
	
	if (status_code >= 200 && status_code < 300)
		metrics->http_requests_2xx++;
	else if (status_code >= 400 && status_code < 500)
		metrics->http_requests_4xx++;
	else if (status_code >= 500)
		metrics->http_requests_5xx++;
}

void ramd_metrics_update_replication_lag(ramd_metrics_t* metrics, int32_t lag_ms)
{
	if (!metrics)
		return;
		
	metrics->node_replication_lag_ms = lag_ms;
}

void ramd_metrics_update_resource_usage(ramd_metrics_t* metrics, size_t memory_bytes, int32_t cpu_percent, int32_t disk_percent)
{
	if (!metrics)
		return;
		
	metrics->memory_usage_bytes = memory_bytes;
	metrics->cpu_usage_percent = cpu_percent;
	metrics->disk_usage_percent = disk_percent;
}
