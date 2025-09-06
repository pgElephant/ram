/*-------------------------------------------------------------------------
 *
 * ramd_cascade.c
 *		Cascading replication support for ramd
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "ramd_defaults.h"

#include "ramd_cascade.h"
#include "ramd_cluster.h"
#include "ramd_config.h"
#include "ramd_logging.h"
#include "ramd_postgresql.h"

/* Global cascade configuration */
static ramd_cascade_config_t g_cascade_config = {0};
static bool g_cascade_initialized = false;

/* Maximum nodes we can track in cascade topology */
#define MAX_CASCADE_NODES 64
static ramd_cascade_node_t g_cascade_nodes[MAX_CASCADE_NODES];
static int32_t g_cascade_node_count = 0;

/* ramd_cluster_get_node_by_id is now implemented in ramd_cluster.c */

static const char* ramd_postgresql_get_data_directory(void)
{
	static char data_dir[512];
	char* pgdata = getenv("PGDATA");

	if (pgdata && strlen(pgdata) > 0)
	{
		strncpy(data_dir, pgdata, sizeof(data_dir) - 1);
		data_dir[sizeof(data_dir) - 1] = '\0';
	}
	else
	{
		strcpy(data_dir, RAMD_DEFAULT_PG_DATA_DIR);
	}

	return data_dir;
}


static bool ramd_postgresql_update_recovery_conf(const char* path,
                                                 const char* conninfo,
                                                 const char* slot)
{
	FILE* f;

	if (!path || !conninfo)
		return false;

	f = fopen(path, "w");
	if (!f)
		return false;

	fprintf(f, "primary_conninfo = '%s'\n", conninfo);
	if (slot)
		fprintf(f, "primary_slot_name = '%s'\n", slot);
	fprintf(f, "standby_mode = 'on'\n");

	fclose(f);
	return true;
}


bool ramd_cascade_init(ramd_cascade_config_t* config)
{
	if (!config)
	{
		ramd_log_error("Cascade initialization failed: NULL config provided");
		return false;
	}

	/* Copy configuration */
	memcpy(&g_cascade_config, config, sizeof(ramd_cascade_config_t));

	/* Validate configuration */
	if (!ramd_cascade_config_validate(&g_cascade_config))
	{
		ramd_log_error("Cascade initialization failed: Invalid configuration");
		return false;
	}

	/* Initialize cascade topology tracking */
	memset(g_cascade_nodes, 0, sizeof(g_cascade_nodes));
	g_cascade_node_count = 0;

	g_cascade_initialized = true;

	if (g_cascade_config.enabled)
	{
		ramd_log_info(
		    "Cascading replication initialized: max_depth=%d, upstream=%d",
		    g_cascade_config.max_cascade_depth,
		    g_cascade_config.upstream_node_id);
	}
	else
	{
		ramd_log_info("Cascading replication disabled");
	}

	return true;
}


void ramd_cascade_cleanup(void)
{
	if (!g_cascade_initialized)
		return;

	ramd_log_info("Cleaning up cascading replication");

	memset(&g_cascade_config, 0, sizeof(g_cascade_config));
	memset(g_cascade_nodes, 0, sizeof(g_cascade_nodes));
	g_cascade_node_count = 0;
	g_cascade_initialized = false;
}


bool ramd_cascade_is_enabled(void)
{
	return g_cascade_initialized && g_cascade_config.enabled;
}


bool ramd_cascade_setup_upstream(int32_t upstream_node_id)
{
	char recovery_conf_path[512];
	char primary_conninfo[1024];
	ramd_node_t upstream_node;

	if (!ramd_cascade_is_enabled())
	{
		ramd_log_warning(
		    "Cascade setup upstream requested but cascading is disabled");
		return false;
	}

	/* Get upstream node information */
	if (!ramd_cluster_get_node_by_id(upstream_node_id, &upstream_node))
	{
		ramd_log_error(
		    "Cascade setup upstream failed: upstream node %d not found",
		    upstream_node_id);
		return false;
	}

	/* Validate cascade depth */
	int32_t upstream_depth = ramd_cascade_get_depth(upstream_node_id);
	if (upstream_depth >= 0 &&
	    upstream_depth >= g_cascade_config.max_cascade_depth)
	{
		ramd_log_error(
		    "Cascade setup upstream failed: would exceed max depth %d",
		    g_cascade_config.max_cascade_depth);
		return false;
	}

	/* Check for loops */
	if (!ramd_cascade_is_loop_free(-1,
	                               upstream_node_id)) /* -1 = current node */
	{
		ramd_log_error(
		    "Cascade setup upstream failed: would create loop with node %d",
		    upstream_node_id);
		return false;
	}

	/* Build primary_conninfo for upstream */
	snprintf(primary_conninfo, sizeof(primary_conninfo),
	         "host=%s port=%d user=replicator application_name='%s'",
	         upstream_node.hostname, upstream_node.postgresql_port,
	         g_cascade_config.cascade_application_name);

	/* Update recovery configuration */
	snprintf(recovery_conf_path, sizeof(recovery_conf_path), "%s/recovery.conf",
	         ramd_postgresql_get_data_directory());

	if (!ramd_postgresql_update_recovery_conf(recovery_conf_path,
	                                          primary_conninfo, NULL))
	{
		ramd_log_error(
		    "Cascade setup upstream failed: could not update recovery.conf");
		return false;
	}

	/* Update configuration */
	g_cascade_config.upstream_node_id = upstream_node_id;

	/* Restart PostgreSQL to apply new configuration */
	if (!ramd_postgresql_restart(NULL))
	{
		ramd_log_error(
		    "Cascade setup upstream failed: could not restart PostgreSQL");
		return false;
	}

	ramd_log_info("Cascade upstream configured: node %d (%s:%d)",
	              upstream_node_id, upstream_node.hostname,
	              upstream_node.postgresql_port);

	return true;
}


bool ramd_cascade_remove_upstream(void)
{
	if (!ramd_cascade_is_enabled())
		return true;

	if (g_cascade_config.upstream_node_id == -1)
	{
		ramd_log_info("Cascade remove upstream: no upstream configured");
		return true;
	}

	ramd_log_info("Removing cascade upstream connection from node %d",
	              g_cascade_config.upstream_node_id);

	/* Reset to connect directly to primary */
	g_cascade_config.upstream_node_id = -1;

	/* This would typically trigger a reconfiguration to connect to primary */
	/* Implementation depends on how primary connection is managed */

	return true;
}


bool ramd_cascade_validate_topology(void)
{
	int32_t i, j;

	if (!ramd_cascade_is_enabled())
		return true;

	/* Check for loops in cascade topology */
	for (i = 0; i < g_cascade_node_count; i++)
	{
		ramd_cascade_node_t* node = &g_cascade_nodes[i];
		int32_t visited[MAX_CASCADE_NODES];
		int32_t visited_count = 0;
		int32_t current_node = node->node_id;

		/* Follow upstream chain */
		while (current_node != -1 && visited_count < MAX_CASCADE_NODES)
		{
			/* Check if we've seen this node before (loop detection) */
			for (j = 0; j < visited_count; j++)
			{
				if (visited[j] == current_node)
				{
					ramd_log_error("Cascade topology validation failed: loop "
					               "detected involving node %d",
					               current_node);
					return false;
				}
			}

			visited[visited_count++] = current_node;

			/* Find upstream of current node */
			int32_t upstream = -1;
			for (j = 0; j < g_cascade_node_count; j++)
			{
				if (g_cascade_nodes[j].node_id == current_node)
				{
					upstream = g_cascade_nodes[j].upstream_node_id;
					break;
				}
			}
			current_node = upstream;
		}

		/* Check cascade depth */
		if (visited_count >
		    g_cascade_config.max_cascade_depth + 1) /* +1 for primary */
		{
			ramd_log_error("Cascade topology validation failed: depth %d "
			               "exceeds maximum %d for node %d",
			               visited_count - 1,
			               g_cascade_config.max_cascade_depth, node->node_id);
			return false;
		}
	}

	ramd_log_debug("Cascade topology validation passed");
	return true;
}


int32_t ramd_cascade_find_best_upstream(int32_t for_node_id)
{
	int32_t i;
	int32_t best_upstream = -1;              /* -1 means primary */
	int64_t min_lag = 9223372036854775807LL; /* LLONG_MAX */
	int32_t min_depth = 2147483647;          /* INT_MAX */

	if (!ramd_cascade_is_enabled())
		return -1; /* Connect to primary */

	/* Find best upstream candidate */
	for (i = 0; i < g_cascade_node_count; i++)
	{
		ramd_cascade_node_t* candidate = &g_cascade_nodes[i];

		/* Skip self */
		if (candidate->node_id == for_node_id)
			continue;

		/* Skip if not cascade eligible */
		if (!candidate->is_cascade_eligible)
			continue;

		/* Skip if would exceed max depth */
		if (candidate->cascade_depth >= g_cascade_config.max_cascade_depth)
			continue;

		/* Skip if lag is too high */
		if (candidate->cascade_lag_bytes >
		    g_cascade_config.cascade_lag_threshold)
			continue;

		/* Check if would create loop */
		if (!ramd_cascade_is_loop_free(for_node_id, candidate->node_id))
			continue;

		/* Prefer lower lag, then lower depth */
		if (candidate->cascade_lag_bytes < min_lag ||
		    (candidate->cascade_lag_bytes == min_lag &&
		     candidate->cascade_depth < min_depth))
		{
			best_upstream = candidate->node_id;
			min_lag = candidate->cascade_lag_bytes;
			min_depth = candidate->cascade_depth;
		}
	}

	if (best_upstream != -1)
	{
		ramd_log_info(
		    "Cascade best upstream for node %d: node %d (lag=%lld, depth=%d)",
		    for_node_id, best_upstream, (long long) min_lag, min_depth);
	}
	else
	{
		ramd_log_info("Cascade best upstream for node %d: primary (no suitable "
		              "cascade upstream)",
		              for_node_id);
	}

	return best_upstream;
}


bool ramd_cascade_reconfigure_on_failover(int32_t failed_node_id,
                                          int32_t new_primary_id)
{
	int32_t i;
	bool reconfigured = false;

	if (!ramd_cascade_is_enabled())
		return true;

	ramd_log_info(
	    "Cascade reconfiguration on failover: failed=%d, new_primary=%d",
	    failed_node_id, new_primary_id);

	/* Find all nodes that were cascading from the failed node */
	for (i = 0; i < g_cascade_node_count; i++)
	{
		ramd_cascade_node_t* node = &g_cascade_nodes[i];

		if (node->upstream_node_id == failed_node_id)
		{
			/* This node needs a new upstream */
			int32_t new_upstream =
			    ramd_cascade_find_best_upstream(node->node_id);

			ramd_log_info("Cascade reconfiguring node %d: old_upstream=%d -> "
			              "new_upstream=%d",
			              node->node_id, failed_node_id, new_upstream);

			/* Update topology */
			node->upstream_node_id = new_upstream;
			if (new_upstream == -1)
				node->cascade_depth = 0; /* Connected to primary */
			else
				node->cascade_depth = ramd_cascade_get_depth(new_upstream) + 1;

			reconfigured = true;
		}
	}

	/* Update primary node in topology */
	for (i = 0; i < g_cascade_node_count; i++)
	{
		if (g_cascade_nodes[i].node_id == new_primary_id)
		{
			g_cascade_nodes[i].upstream_node_id = -1;
			g_cascade_nodes[i].cascade_depth = 0;
			break;
		}
	}

	if (reconfigured)
	{
		/* Validate new topology */
		if (!ramd_cascade_validate_topology())
		{
			ramd_log_error("Cascade reconfiguration on failover resulted in "
			               "invalid topology");
			return false;
		}
	}

	return true;
}


int32_t ramd_cascade_get_depth(int32_t node_id)
{
	int32_t i;

	if (node_id == -1)
		return 0; /* Primary has depth 0 */

	for (i = 0; i < g_cascade_node_count; i++)
	{
		if (g_cascade_nodes[i].node_id == node_id)
			return g_cascade_nodes[i].cascade_depth;
	}

	return -1; /* Node not found */
}


bool ramd_cascade_is_loop_free(int32_t node_id, int32_t proposed_upstream_id)
{
	int32_t current = proposed_upstream_id;
	int32_t visited_count = 0;

	/* Follow upstream chain from proposed upstream */
	while (current != -1 && visited_count < MAX_CASCADE_NODES)
	{
		if (current == node_id)
			return false; /* Loop detected */

		/* Find upstream of current node */
		int32_t i;
		int32_t upstream = -1;
		for (i = 0; i < g_cascade_node_count; i++)
		{
			if (g_cascade_nodes[i].node_id == current)
			{
				upstream = g_cascade_nodes[i].upstream_node_id;
				break;
			}
		}
		current = upstream;
		visited_count++;
	}

	return true; /* No loop detected */
}


void ramd_cascade_config_set_defaults(ramd_cascade_config_t* config)
{
	if (!config)
		return;

	memset(config, 0, sizeof(ramd_cascade_config_t));

	config->enabled = false;
	config->upstream_node_id = -1;
	config->allow_cascading = true;
	config->max_cascade_depth = 3;
	config->cascade_lag_threshold = 16 * 1024 * 1024; /* 16MB */
	snprintf(config->cascade_application_name,
	         sizeof(config->cascade_application_name), "ramd_cascade");
}


bool ramd_cascade_config_validate(ramd_cascade_config_t* config)
{
	if (!config)
		return false;

	if (config->max_cascade_depth < 1 || config->max_cascade_depth > 10)
	{
		ramd_log_error(
		    "Cascade config validation failed: max_cascade_depth must be 1-10");
		return false;
	}

	if (config->cascade_lag_threshold < 0)
	{
		ramd_log_error("Cascade config validation failed: "
		               "cascade_lag_threshold must be >= 0");
		return false;
	}

	if (strlen(config->cascade_application_name) == 0)
	{
		ramd_log_warning(
		    "Cascade config: empty application_name, using default");
		snprintf(config->cascade_application_name,
		         sizeof(config->cascade_application_name), "ramd_cascade");
	}

	return true;
}


bool ramd_cascade_get_node_info(int32_t node_id, ramd_cascade_node_t* info)
{
	int32_t i;

	if (!info)
		return false;

	for (i = 0; i < g_cascade_node_count; i++)
	{
		if (g_cascade_nodes[i].node_id == node_id)
		{
			memcpy(info, &g_cascade_nodes[i], sizeof(ramd_cascade_node_t));
			return true;
		}
	}

	return false;
}
