/*-------------------------------------------------------------------------
 *
 * ramd_cluster.c
 *		PostgreSQL Auto-Failover Daemon - Cluster Management
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ramd_cluster.h"
#include "ramd_logging.h"
#include "ramd_pgraft.h"
#include "ramd_conn.h"
#include "ramd_query.h"
#include "ramd_daemon.h"
#include <libpq-fe.h>

extern ramd_daemon_t* g_ramd_daemon;
extern PGconn* g_conn;

bool
ramd_cluster_init(ramd_cluster_t* cluster, const ramd_config_t* config)
{
	if (!cluster || !config)
		return false;

	memset(cluster, 0, sizeof(ramd_cluster_t));

	strncpy(cluster->cluster_name, config->cluster_name,
	        sizeof(cluster->cluster_name) - 1);
	cluster->local_node_id = config->node_id;
	cluster->primary_node_id = -1;
	cluster->leader_node_id = -1;
	cluster->last_topology_change = time(NULL);

	if (!g_conn)
	{
		ramd_log_error("Failed to connect to PostgreSQL for pgraft integration");
		return false;
	}

	ramd_log_info("PostgreSQL connection established for pgraft integration");
		
	if (ramd_pgraft_check_availability(g_conn) == 1)
		ramd_log_info("pgraft extension is available and ready");
	else
		ramd_log_warning("pgraft extension not available - using fallback logic");

	ramd_log_info("Cluster initialized: %s (local_node_id=%d)",
	              cluster->cluster_name, cluster->local_node_id);

	return true;
}

bool
ramd_cluster_bootstrap_primary(ramd_cluster_t* cluster, const ramd_config_t* config)
{
	ramd_node_t* local_node;

	if (!cluster || !config)
		return false;

	if (cluster->node_count > 0)
	{
		ramd_log_error("Cannot bootstrap primary: cluster has %d nodes",
		               cluster->node_count);
		return false;
	}

	ramd_log_info("Bootstrapping primary node for cluster '%s'",
	              cluster->cluster_name);

	if (!ramd_cluster_add_node(cluster, config->node_id, config->hostname,
	                           config->postgresql_port, config->rale_port,
	                           config->dstore_port))
	{
		ramd_log_error("Failed to add local node during bootstrap");
		return false;
	}

	local_node = ramd_cluster_find_node(cluster, config->node_id);
	if (local_node)
	{
		local_node->is_healthy = true;
		local_node->state = RAMD_NODE_STATE_PRIMARY;
		local_node->role = RAMD_ROLE_PRIMARY;
		cluster->primary_node_id = config->node_id;
		cluster->leader_node_id = config->node_id;
	}

	ramd_log_info("Primary node bootstrap completed successfully");
	return true;
}

void
ramd_cluster_cleanup(ramd_cluster_t* cluster)
{
	if (!cluster)
		return;

	ramd_log_info("Cleaning up cluster: %s", cluster->cluster_name);
	memset(cluster, 0, sizeof(ramd_cluster_t));
}

bool
ramd_cluster_add_node(ramd_cluster_t* cluster, int32_t node_id,
                      const char* hostname, int32_t pg_port,
                      int32_t rale_port, int32_t dstore_port)
{
	ramd_node_t* node;

	if (!cluster || node_id <= 0 || !hostname)
		return false;

	if (cluster->node_count >= RAMD_MAX_NODES)
		return false;

	node = &cluster->nodes[cluster->node_count];
	node->node_id = node_id;
	strncpy(node->hostname, hostname, sizeof(node->hostname) - 1);
	node->postgresql_port = pg_port;
	node->rale_port = rale_port;
	node->dstore_port = dstore_port;
	node->state = RAMD_NODE_STATE_UNKNOWN;
	node->role = RAMD_ROLE_UNKNOWN;
	node->last_seen = time(NULL);
	node->state_changed_at = time(NULL);

	cluster->node_count++;

	ramd_log_info("Added node %d (%s:%d) to cluster", node_id, hostname,
	              pg_port);
	return true;
}

ramd_node_t*
ramd_cluster_find_node(ramd_cluster_t* cluster, int32_t node_id)
{
	int i;

	if (!cluster)
		return NULL;

	for (i = 0; i < cluster->node_count; i++)
	{
		if (cluster->nodes[i].node_id == node_id)
			return &cluster->nodes[i];
	}

	return NULL;
}

ramd_node_t*
ramd_cluster_get_local_node(ramd_cluster_t* cluster)
{
	if (!cluster)
		return NULL;

	return ramd_cluster_find_node(cluster, cluster->local_node_id);
}

bool
ramd_cluster_has_quorum(const ramd_cluster_t* cluster)
{
	int leader;
	int is_healthy;
	int healthy_nodes;

	if (!cluster || !g_conn)
		return false;

	leader = ramd_pgraft_get_leader(g_conn);
	if (leader > 0)
	{
		ramd_log_debug("ramd_cluster_has_quorum: pgraft leader is %d", leader);
		return true;
	}

	ramd_log_debug("ramd_cluster_has_quorum: no pgraft leader, checking health");
	is_healthy = ramd_pgraft_is_cluster_healthy(g_conn);
	if (is_healthy > 0)
		return true;

	ramd_log_debug("ramd_cluster_has_quorum: using simple quorum logic");
	healthy_nodes = ramd_cluster_count_healthy_nodes(cluster);
	return healthy_nodes > (cluster->node_count / 2);
}

int32_t
ramd_cluster_count_healthy_nodes(const ramd_cluster_t* cluster)
{
	int i;
	int count = 0;

	if (!cluster)
		return 0;

	for (i = 0; i < cluster->node_count; i++)
	{
		if (cluster->nodes[i].is_healthy)
			count++;
	}

	return count;
}

bool
ramd_cluster_remove_node(ramd_cluster_t* cluster, int32_t node_id)
{
	int32_t i;
	int32_t j;

	if (!cluster || node_id <= 0 || node_id > cluster->node_count)
		return false;

	for (i = 0; i < cluster->node_count; i++)
	{
		if (cluster->nodes[i].node_id == node_id)
		{
			for (j = i; j < cluster->node_count - 1; j++)
				cluster->nodes[j] = cluster->nodes[j + 1];

			cluster->node_count--;
			ramd_log_info("Removed node %d from cluster", node_id);
			return true;
		}
	}
	return false;
}

ramd_node_t*
ramd_cluster_get_primary_node(ramd_cluster_t* cluster)
{
	int32_t i;

	if (!cluster || cluster->primary_node_id <= 0)
		return NULL;

	for (i = 0; i < cluster->node_count; i++)
	{
		if (cluster->nodes[i].node_id == cluster->primary_node_id)
			return &cluster->nodes[i];
	}
	return NULL;
}

ramd_node_t*
ramd_cluster_get_leader_node(ramd_cluster_t* cluster)
{
	int32_t i;

	if (!cluster || cluster->leader_node_id <= 0)
		return NULL;

	for (i = 0; i < cluster->node_count; i++)
	{
		if (cluster->nodes[i].node_id == cluster->leader_node_id)
			return &cluster->nodes[i];
	}
	return NULL;
}

bool
ramd_cluster_update_node_state(ramd_cluster_t* cluster, int32_t node_id,
                               ramd_node_state_t new_state)
{
	ramd_node_t* node;

	if (!cluster || node_id <= 0)
		return false;

	node = ramd_cluster_find_node(cluster, node_id);
	if (!node)
		return false;

	node->state = new_state;
	node->state_changed_at = time(NULL);
	ramd_log_info("Updated node %d state to %d", node_id, new_state);
	return true;
}

bool
ramd_cluster_update_node_role(ramd_cluster_t* cluster, int32_t node_id,
                              ramd_role_t new_role)
{
	ramd_node_t* node;

	if (!cluster || node_id <= 0)
		return false;

	node = ramd_cluster_find_node(cluster, node_id);
	if (!node)
		return false;

	node->role = new_role;
	ramd_log_info("Updated node %d role to %d", node_id, new_role);
	return true;
}

bool
ramd_cluster_update_node_health(ramd_cluster_t* cluster, int32_t node_id,
                                float health_score)
{
	ramd_node_t* node;

	if (!cluster || node_id <= 0 || health_score < 0.0f ||
	    health_score > 100.0f)
		return false;

	node = ramd_cluster_find_node(cluster, node_id);
	if (!node)
		return false;

	node->health_score = health_score;
	node->last_seen = time(NULL);
	node->is_healthy = (health_score >= RAMD_HEALTH_SCORE_THRESHOLD);
	return true;
}

bool
ramd_cluster_has_primary(const ramd_cluster_t* cluster)
{
	if (!cluster)
		return false;

	return (cluster->primary_node_id > 0);
}

bool
ramd_cluster_has_leader(const ramd_cluster_t* cluster)
{
	if (!cluster)
		return false;

	return (cluster->leader_node_id > 0);
}

int32_t
ramd_cluster_count_standby_nodes(const ramd_cluster_t* cluster)
{
	int32_t i;
	int32_t count = 0;

	if (!cluster)
		return 0;

	for (i = 0; i < cluster->node_count; i++)
	{
		if (cluster->nodes[i].role == RAMD_ROLE_STANDBY)
			count++;
	}
	return count;
}

bool
ramd_cluster_detect_topology_change(ramd_cluster_t* cluster)
{
	int32_t i;
	time_t   now;
	ramd_node_t* node;

	if (!cluster)
		return false;

	now = time(NULL);
	for (i = 0; i < cluster->node_count; i++)
	{
		node = &cluster->nodes[i];
		if (difftime(now, node->last_seen) > RAMD_NODE_TIMEOUT_SECONDS)
		{
			if (node->is_healthy)
			{
				node->is_healthy = false;
				ramd_log_warning("Node %d detected as unhealthy due to timeout",
				                 node->node_id);
				return true;
			}
		}
	}
	return false;
}

void
ramd_cluster_update_topology(ramd_cluster_t* cluster)
{
	int32_t i;

	if (!cluster)
		return;

	cluster->node_count = 0;
	for (i = 0; i < RAMD_MAX_NODES; i++)
	{
		if (cluster->nodes[i].node_id > 0)
			cluster->node_count++;
	}

	ramd_log_debug("Updated cluster topology: %d nodes", cluster->node_count);
}

void
ramd_cluster_print_topology(const ramd_cluster_t* cluster)
{
	int32_t i;
	const ramd_node_t* node;

	if (!cluster)
		return;

	ramd_log_info("Cluster Topology:");
	ramd_log_info("  Name: %s", cluster->cluster_name);
	ramd_log_info("  Primary Node ID: %d", cluster->primary_node_id);
	ramd_log_info("  Leader Node ID: %d", cluster->leader_node_id);
	ramd_log_info("  Total Nodes: %d", cluster->node_count);

	for (i = 0; i < cluster->node_count; i++)
	{
		node = &cluster->nodes[i];
		ramd_log_info("    Node %d: %s:%d (%s, %s, health: %.1f)",
		              node->node_id, node->hostname, node->postgresql_port,
		              node->state == RAMD_NODE_STATE_PRIMARY ? "PRIMARY" :
		              node->state == RAMD_NODE_STATE_STANDBY ? "STANDBY" :
		                                                       "UNKNOWN",
		              node->is_healthy ? "healthy" : "unhealthy",
		              node->health_score);
	}
}

bool
ramd_cluster_get_node_by_id(int32_t node_id, ramd_node_t* node)
{
	PGresult* res;
	int32_t   leader_id;
	int32_t   current_state;
	int       node_count;

	if (!node || node_id < 0 || !g_conn)
	{
		ramd_log_error("Invalid parameters for ramd_cluster_get_node_by_id");
		return false;
	}

	res = ramd_query_exec_with_result(g_conn, "SELECT pgraft_get_leader_id()");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		ramd_log_error("Failed to get leader ID: %s", PQerrorMessage(g_conn));
		PQclear(res);
		return false;
	}

	leader_id = 0;
	if (PQntuples(res) > 0)
		leader_id = atoi(PQgetvalue(res, 0, 0));
	PQclear(res);

	res = ramd_query_exec_with_result(g_conn, "SELECT pgraft_get_current_state()");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		ramd_log_error("Failed to get current state: %s", PQerrorMessage(g_conn));
		PQclear(res);
		return false;
	}

	current_state = 0;
	if (PQntuples(res) > 0)
		current_state = atoi(PQgetvalue(res, 0, 0));
	PQclear(res);

	res = ramd_query_exec_with_result(g_conn, "SELECT pgraft_get_cluster_nodes()");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		ramd_log_error("Failed to get cluster nodes: %s", PQerrorMessage(g_conn));
		PQclear(res);
		return false;
	}

	node_count = PQntuples(res);
	PQclear(res);

	memset(node, 0, sizeof(ramd_node_t));
	node->node_id = node_id;
	node->leader_id = leader_id;
	node->state = (ramd_node_state_t)current_state;
	node->cluster_size = node_count;
	node->postgresql_port = g_ramd_daemon->config.postgresql_port;
	node->rale_port = g_ramd_daemon->config.rale_port;
	node->dstore_port = g_ramd_daemon->config.dstore_port;
	node->is_healthy = true;
	node->last_seen = time(NULL);
	node->health_score = 1.0f;

	ramd_log_debug("Retrieved node %d info", node_id);

	return true;
}
