/*-------------------------------------------------------------------------
 *
 * ramd_pgraft.c
 *		PostgreSQL RAM Daemon - pgraft function implementations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * This file implements the interface between ramd and the pgraft PostgreSQL
 * extension. It provides C functions that call the pgraft SQL functions
 * to enable ramd to use Raft consensus for cluster management decisions.
 *
 *-------------------------------------------------------------------------
 */

#include "libpq-fe.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "ramd_pgraft.h"
#include "ramd_logging.h"
#include "ramd_basebackup.h"
#include "ramd_conn.h"
#include "ramd_query.h"

static char g_last_error[512] = {0};

/* Enhanced integration helper functions */
static void
ramd_pgraft_update_cluster_state(PGconn* conn)
{
	/* Update cluster health status */
	ramd_pgraft_is_cluster_healthy(conn);
	
	/* Update leader information */
	ramd_pgraft_get_leader(conn);
	
	/* Update node list */
	ramd_pgraft_get_nodes(conn);
	
	ramd_log_debug("Updated cluster state from pgraft");
}

static void
set_last_error(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(g_last_error, sizeof(g_last_error), format, args);
	va_end(args);
}

static char*
execute_simple_query(PGconn* conn, const char* query)
{
	PGresult* result;
	char* value = NULL;

	if (!conn)
	{
		set_last_error("Database connection is NULL");
		return NULL;
	}

	result = ramd_query_exec_with_result(conn, query);
	if (!result)
	{
		set_last_error("Failed to execute query: %s", PQerrorMessage(conn));
		return NULL;
	}

	if (PQresultStatus(result) != PGRES_TUPLES_OK)
	{
		set_last_error("Query failed: %s", PQresultErrorMessage(result));
		PQclear(result);
		return NULL;
	}

	if (PQntuples(result) > 0 && !PQgetisnull(result, 0, 0))
	{
		char* result_value = PQgetvalue(result, 0, 0);
		value = strdup(result_value);
	}

	PQclear(result);
	return value;
}

static int
execute_boolean_query(PGconn* conn, const char* query)
{
	char* result = execute_simple_query(conn, query);
	if (!result)
		return -1;

	int value = (strcmp(result, "t") == 0) ? 1 : 0;
	free(result);
	return value;
}

static long long
execute_int_query(PGconn* conn, const char* query)
{
	char* result = execute_simple_query(conn, query);
	if (!result)
		return -1;

	long long value = atoll(result);
	free(result);
	return value;
}

int
ramd_pgraft_init(PGconn* conn, int node_id, const char* hostname, int port)
{
	char query[512];
	PGresult* result;

	if (!conn)
	{
		set_last_error("Database connection is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	if (!hostname)
	{
		set_last_error("Hostname is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	snprintf(query, sizeof(query),
	         "SELECT pgraft_init(%d, '%s', %d)", node_id, hostname, port);

	result = ramd_query_exec_with_result(conn, query);
	if (!result)
	{
		set_last_error("Failed to execute pgraft_init: %s", PQerrorMessage(conn));
		return RAMD_PGRAFT_ERROR;
	}

	if (PQresultStatus(result) != PGRES_TUPLES_OK)
	{
		set_last_error("pgraft_init failed: %s", PQresultErrorMessage(result));
		PQclear(result);
		return RAMD_PGRAFT_ERROR;
	}

	PQclear(result);
	ramd_log_info("Successfully initialized pgraft system for node %d at %s:%d",
	              node_id, hostname, port);
	return RAMD_PGRAFT_SUCCESS;
}

int
ramd_pgraft_start(PGconn* conn)
{
	char* result = execute_simple_query(conn, "SELECT pgraft_start()");
	if (!result)
		return RAMD_PGRAFT_ERROR;

	int success = (strcmp(result, "t") == 0) ? RAMD_PGRAFT_SUCCESS : RAMD_PGRAFT_ERROR;
	free(result);

	if (success == RAMD_PGRAFT_SUCCESS)
	{
		ramd_log_info("Successfully started pgraft system");
	}
	else
	{
		set_last_error("pgraft_start returned false");
	}

	return success;
}

int
ramd_pgraft_stop(PGconn* conn)
{
	char* result = execute_simple_query(conn, "SELECT pgraft_stop()");
	if (!result)
		return RAMD_PGRAFT_ERROR;

	int success = (strcmp(result, "t") == 0) ? RAMD_PGRAFT_SUCCESS : RAMD_PGRAFT_ERROR;
	free(result);

	if (success == RAMD_PGRAFT_SUCCESS)
	{
		ramd_log_info("Successfully stopped pgraft system");
	}
	else
	{
		set_last_error("pgraft_stop returned false");
	}

	return success;
}

char*
ramd_pgraft_get_state(PGconn* conn)
{
	return execute_simple_query(conn, "SELECT pgraft_get_state()");
}

int
ramd_pgraft_is_leader(PGconn* conn)
{
	return execute_boolean_query(conn, "SELECT pgraft_is_leader()");
}

int
ramd_pgraft_get_leader(PGconn* conn)
{
	long long leader = execute_int_query(conn, "SELECT pgraft_get_leader()");
	return (int)leader;
}

long long
ramd_pgraft_get_term(PGconn* conn)
{
	return execute_int_query(conn, "SELECT pgraft_get_term()");
}

char*
ramd_pgraft_get_nodes(PGconn* conn)
{
	return execute_simple_query(conn, "SELECT pgraft_get_nodes()");
}

int
ramd_pgraft_add_node(PGconn* conn, int node_id, const char* hostname, int port)
{
	char query[512];
	PGresult* result;

	if (!conn)
	{
		set_last_error("Database connection is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	if (!hostname)
	{
		set_last_error("Hostname is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	snprintf(query, sizeof(query),
	         "SELECT pgraft_add_node(%d, '%s', %d)", node_id, hostname, port);

	result = ramd_query_exec_with_result(conn, query);
	if (!result)
	{
		set_last_error("Failed to execute pgraft_add_node: %s", PQerrorMessage(conn));
		return RAMD_PGRAFT_ERROR;
	}

	if (PQresultStatus(result) != PGRES_TUPLES_OK)
	{
		set_last_error("pgraft_add_node failed: %s", PQresultErrorMessage(result));
		PQclear(result);
		return RAMD_PGRAFT_ERROR;
	}

	PQclear(result);
	ramd_log_info("Successfully added node %d at %s:%d to Raft cluster",
	              node_id, hostname, port);
	
	/* Enhanced integration: Update cluster state */
	ramd_pgraft_update_cluster_state(conn);
	
	return RAMD_PGRAFT_SUCCESS;
}

int
ramd_pgraft_remove_node(PGconn* conn, int node_id)
{
	char query[256];
	PGresult* result;

	if (!conn)
	{
		set_last_error("Database connection is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	snprintf(query, sizeof(query), "SELECT pgraft_remove_node(%d)", node_id);

	result = ramd_query_exec_with_result(conn, query);
	if (!result)
	{
		set_last_error("Failed to execute pgraft_remove_node: %s", PQerrorMessage(conn));
		return RAMD_PGRAFT_ERROR;
	}

	if (PQresultStatus(result) != PGRES_TUPLES_OK)
	{
		set_last_error("pgraft_remove_node failed: %s", PQresultErrorMessage(result));
		PQclear(result);
		return RAMD_PGRAFT_ERROR;
	}

	PQclear(result);
	ramd_log_info("Successfully removed node %d from Raft cluster", node_id);
	return RAMD_PGRAFT_SUCCESS;
}

char*
ramd_pgraft_get_cluster_health(PGconn* conn)
{
	return execute_simple_query(conn, "SELECT pgraft_get_cluster_health()");
}

int
ramd_pgraft_is_cluster_healthy(PGconn* conn)
{
	return execute_boolean_query(conn, "SELECT pgraft_is_cluster_healthy()");
}

char*
ramd_pgraft_get_performance_metrics(PGconn* conn)
{
	return execute_simple_query(conn, "SELECT pgraft_get_performance_metrics()");
}

int
ramd_pgraft_append_log(PGconn* conn, const char* log_data)
{
	char query[1024];
	PGresult* result;

	if (!conn)
	{
		set_last_error("Database connection is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	if (!log_data)
	{
		set_last_error("Log data is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	char escaped_data[512];
	size_t j = 0;
	for (int i = 0; log_data[i] && j < sizeof(escaped_data) - 2; i++)
	{
		if (log_data[i] == '\'')
		{
			escaped_data[j++] = '\'';
			escaped_data[j++] = '\'';
		}
		else
		{
			escaped_data[j++] = log_data[i];
		}
	}
	escaped_data[j] = '\0';

	snprintf(query, sizeof(query), "SELECT pgraft_append_log('%s')", escaped_data);

	result = ramd_query_exec_with_result(conn, query);
	if (!result)
	{
		set_last_error("Failed to execute pgraft_append_log: %s", PQerrorMessage(conn));
		return RAMD_PGRAFT_ERROR;
	}

	if (PQresultStatus(result) != PGRES_TUPLES_OK)
	{
		set_last_error("pgraft_append_log failed: %s", PQresultErrorMessage(result));
		PQclear(result);
		return RAMD_PGRAFT_ERROR;
	}

	PQclear(result);
	return RAMD_PGRAFT_SUCCESS;
}

char*
ramd_pgraft_get_log(PGconn* conn)
{
	return execute_simple_query(conn, "SELECT pgraft_get_log()");
}

char*
ramd_pgraft_get_stats(PGconn* conn)
{
	return execute_simple_query(conn, "SELECT pgraft_get_stats()");
}

char*
ramd_pgraft_get_version(PGconn* conn)
{
	return execute_simple_query(conn, "SELECT pgraft_version()");
}

int
ramd_pgraft_test(PGconn* conn)
{
	long long result = execute_int_query(conn, "SELECT pgraft_test()");
	return (int)result;
}

const char*
ramd_pgraft_get_last_error(void)
{
	return g_last_error[0] ? g_last_error : NULL;
}

int
ramd_pgraft_check_availability(PGconn* conn)
{
	char* result = execute_simple_query(conn, "SELECT pgraft_version()");
	if (!result)
		return 0;

	free(result);
	return 1;
}

int
ramd_pgraft_setup_replica(PGconn* conn, int replica_node_id, 
                         const char* replica_hostname, int replica_port,
                         const char* backup_dir)
{
	char backup_label[256];
	char replica_backup_dir[512];
	int result;

	if (!conn)
	{
		set_last_error("Database connection is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	if (!replica_hostname || !backup_dir)
	{
		set_last_error("Replica hostname or backup directory is NULL");
		return RAMD_PGRAFT_ERROR;
	}

	snprintf(backup_label, sizeof(backup_label), "replica_%d_setup", replica_node_id);

	snprintf(replica_backup_dir, sizeof(replica_backup_dir), "%s/replica_%d", backup_dir, replica_node_id);

	ramd_log_info("Setting up replica node %d at %s:%d", replica_node_id, replica_hostname, replica_port);
	ramd_log_info("Taking base backup to directory: %s", replica_backup_dir);

	result = ramd_take_basebackup(conn, replica_backup_dir, backup_label);
	if (result != 0)
	{
		set_last_error("Failed to take base backup for replica %d", replica_node_id);
		return RAMD_PGRAFT_ERROR;
	}

	ramd_log_info("Base backup completed successfully for replica %d", replica_node_id);

	result = ramd_pgraft_add_node(conn, replica_node_id, replica_hostname, replica_port);
	if (result != RAMD_PGRAFT_SUCCESS)
	{
		set_last_error("Failed to add replica %d to Raft cluster", replica_node_id);
		return result;
	}

	ramd_log_info("Successfully set up replica node %d and added to Raft cluster", replica_node_id);
	return RAMD_PGRAFT_SUCCESS;
}
