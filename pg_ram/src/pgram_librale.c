/*-------------------------------------------------------------------------
 *
 * pgram_librale.c
 *		librale integration implementation for pg_ram
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/elog.h"
#include "pgram_librale.h"
#include "pgram_guc.h"

/* Include real librale headers with path from the actual rale project */
#include "librale.h"
#include "cluster.h"
#include "node.h"
#include "dstore.h"

pg_ram_librale_config_t* pg_ram_librale_config = NULL;

#define DEFAULT_RALE_PORT 7400
#define DEFAULT_DSTORE_PORT 7401
#define DEFAULT_NODE_ID 1

bool pgram_librale_init(void)
{
	librale_config_t* librale_config;
	librale_status_t result;

	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
		return true;

	pg_ram_librale_config =
	    (pg_ram_librale_config_t*) palloc0(sizeof(pg_ram_librale_config_t));
	if (pg_ram_librale_config == NULL)
	{
		elog(ERROR, "pg_ram: Failed to allocate librale configuration");
		return false;
	}

	pg_ram_librale_config->node_id =
	    (pgram_node_id > 0) ? pgram_node_id : DEFAULT_NODE_ID;

	if (pgram_node_name && pgram_node_name[0] != '\0')
		snprintf(pg_ram_librale_config->node_name,
		         sizeof(pg_ram_librale_config->node_name), "%s",
		         pgram_node_name);
	else
		snprintf(pg_ram_librale_config->node_name,
		         sizeof(pg_ram_librale_config->node_name), "pg_ram_node_%d",
		         pg_ram_librale_config->node_id);

	if (pgram_node_ip && pgram_node_ip[0] != '\0')
		snprintf(pg_ram_librale_config->node_ip,
		         sizeof(pg_ram_librale_config->node_ip), "%s", pgram_node_ip);
	else
		snprintf(pg_ram_librale_config->node_ip,
		         sizeof(pg_ram_librale_config->node_ip), pgram_node_ip);

	pg_ram_librale_config->rale_port =
	    (uint16) ((pgram_rale_port >= 1 && pgram_rale_port <= 65535)
	                  ? pgram_rale_port
	                  : DEFAULT_RALE_PORT);
	pg_ram_librale_config->dstore_port =
	    (uint16) ((pgram_dstore_port >= 1 && pgram_dstore_port <= 65535)
	                  ? pgram_dstore_port
	                  : DEFAULT_DSTORE_PORT);

	if (pgram_db_path && pgram_db_path[0] != '\0')
	{
		snprintf(pg_ram_librale_config->db_path,
		         sizeof(pg_ram_librale_config->db_path), "%s", pgram_db_path);
		snprintf(pg_ram_librale_config->log_directory,
		         sizeof(pg_ram_librale_config->log_directory), "%s",
		         pgram_db_path);
	}
	else
	{
		snprintf(pg_ram_librale_config->db_path,
		         sizeof(pg_ram_librale_config->db_path), "%s/pg_ram_librale",
		         DataDir);
		snprintf(pg_ram_librale_config->log_directory,
		         sizeof(pg_ram_librale_config->log_directory),
		         "%s/pg_ram_librale", DataDir);
	}

	librale_config = librale_config_create();
	if (librale_config == NULL)
	{
		elog(ERROR, "pg_ram: Failed to create librale configuration");
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	result = librale_config_set_node_id(librale_config,
	                                    pg_ram_librale_config->node_id);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR, "pg_ram: Failed to set librale node ID: %d", result);
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	result = librale_config_set_node_name(librale_config,
	                                      pg_ram_librale_config->node_name);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR, "pg_ram: Failed to set librale node name: %d", result);
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	result = librale_config_set_node_ip(librale_config,
	                                    pg_ram_librale_config->node_ip);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR, "pg_ram: Failed to set librale node IP: %d", result);
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	result = librale_config_set_rale_port(librale_config,
	                                      pg_ram_librale_config->rale_port);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR, "pg_ram: Failed to set librale RALE port: %d", result);
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	result = librale_config_set_dstore_port(librale_config,
	                                        pg_ram_librale_config->dstore_port);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR, "pg_ram: Failed to set librale DStore port: %d", result);
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	result = librale_config_set_db_path(librale_config,
	                                    pg_ram_librale_config->db_path);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR, "pg_ram: Failed to set librale database path: %d", result);
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	result = librale_config_set_log_directory(
	    librale_config, pg_ram_librale_config->log_directory);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR, "pg_ram: Failed to set librale log directory: %d", result);
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	pg_ram_librale_config->librale_config = librale_config;

	result = librale_rale_init(librale_config);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR, "pg_ram: Failed to initialize librale: %d", result);
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	result =
	    librale_dstore_init(pg_ram_librale_config->dstore_port, librale_config);
	if (result != RALE_SUCCESS)
	{
		elog(ERROR,
		     "pg_ram: Failed to initialize librale distributed store: %d",
		     result);
		librale_rale_finit();
		librale_config_destroy(librale_config);
		pfree(pg_ram_librale_config);
		pg_ram_librale_config = NULL;
		return false;
	}

	/* Set the node ID in librale for cluster formation */
	librale_set_node_id(pg_ram_librale_config->node_id);

	pg_ram_librale_config->initialized = true;

	elog(
	    LOG,
	    "pg_ram: librale initialized successfully for node %d (%s) at %s:%d/%d",
	    pg_ram_librale_config->node_id, pg_ram_librale_config->node_name,
	    pg_ram_librale_config->node_ip, pg_ram_librale_config->rale_port,
	    pg_ram_librale_config->dstore_port);

	return true;
}

void pgram_librale_cleanup(void)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return;

	char errbuf[256];
	librale_dstore_finit(errbuf, sizeof(errbuf));
	librale_rale_finit();

	if (pg_ram_librale_config->librale_config != NULL)
		librale_config_destroy(pg_ram_librale_config->librale_config);

	pfree(pg_ram_librale_config);
	pg_ram_librale_config = NULL;

	elog(LOG, "pg_ram: librale cleanup completed");
}

bool pgram_librale_is_leader(void)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return false;

	return dstore_is_current_leader() != 0;
}

int32 pgram_librale_get_leader_id(void)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return -1;

	return dstore_get_current_leader();
}

uint32 pgram_librale_get_node_count(void)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return 0;

	return librale_cluster_get_node_count();
}

librale_status_t pgram_librale_add_node(int32 node_id, const char* name,
                                        const char* ip, uint16 rale_port,
                                        uint16 dstore_port)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return RALE_ERROR_GENERAL;

	if (!pgram_librale_is_leader())
	{
		elog(WARNING, "pg_ram: Only leader can add nodes to cluster");
		return RALE_ERROR_GENERAL;
	}

	return cluster_add_node(node_id, name, ip, rale_port, dstore_port);
}

librale_status_t pgram_librale_remove_node(int32 node_id)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return RALE_ERROR_GENERAL;

	if (!pgram_librale_is_leader())
	{
		elog(WARNING, "pg_ram: Only leader can remove nodes from cluster");
		return RALE_ERROR_GENERAL;
	}

	return cluster_remove_node(node_id);
}

bool pgram_librale_has_quorum(void)
{
	uint32 node_count = pgram_librale_get_node_count();

	return node_count >= 2;
}

librale_status_t pgram_librale_process_consensus(void)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return RALE_ERROR_GENERAL;

	return librale_rale_tick();
}

int32 pgram_librale_get_current_role(void)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return -1;

	return librale_get_current_role();
}

int32 pgram_librale_get_current_term(void)
{
	return 0;
}

bool pgram_librale_is_node_healthy(int32 node_id)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return false;

	return dstore_is_node_connected(node_id) != 0;
}

librale_status_t pgram_librale_get_cluster_status(char* status,
                                                  size_t status_size)
{
	if (pg_ram_librale_config == NULL || !pg_ram_librale_config->initialized)
		return RALE_ERROR_GENERAL;

	if (status == NULL || status_size == 0)
		return RALE_ERROR_GENERAL;

	snprintf(status, status_size,
	         "Node ID: %d, Role: %d, Leader: %d, Nodes: %u, Quorum: %s",
	         pg_ram_librale_config->node_id, pgram_librale_get_current_role(),
	         pgram_librale_get_leader_id(), pgram_librale_get_node_count(),
	         pgram_librale_has_quorum() ? "Yes" : "No");

	return RALE_SUCCESS;
}
