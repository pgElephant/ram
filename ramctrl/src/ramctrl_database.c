/*-------------------------------------------------------------------------
 *
 * ramctrl_database.c
 *		PostgreSQL RAM Control Utility - Database Operations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <libpq-fe.h>
#include <time.h>

#include "ramctrl.h"
#include "ramctrl_database.h"

/* SQL queries for pgraft extension */
static const char* sql_cluster_status =
    "SELECT cluster_id, cluster_name, total_nodes, active_nodes, "
    "primary_node_id, cluster_state, last_update FROM ram.cluster_status();";

static const char* sql_nodes_status =
    "SELECT node_id, hostname, port, state, role, health_status, "
    "is_primary, last_seen, replication_lag FROM ram.node_status();";

static const char* sql_promote_node = "SELECT ram.promote_node($1);";

static const char* sql_demote_node = "SELECT ram.demote_node($1);";

static const char* sql_add_node = "SELECT ram.add_node($1, $2, $3);";

static const char* sql_remove_node = "SELECT ram.remove_node($1);";

static const char* sql_set_maintenance =
    "SELECT ram.set_maintenance_mode($1, $2);";

static const char* sql_trigger_failover = "SELECT ram.trigger_failover($1);";

static PGconn* ramctrl_connect_database(ramctrl_context_t* ctx)
{
	char conninfo[1024];
	PGconn* conn;

	if (!ctx)
		return NULL;

	/* Build connection string */
	snprintf(conninfo, sizeof(conninfo), "host=%s port=%d dbname=%s user=%s",
	         ctx->hostname, ctx->port, ctx->database, ctx->user);

	if (strlen(ctx->password) > 0)
	{
		char temp[1024];
		snprintf(temp, sizeof(temp), "%s password=%s", conninfo, ctx->password);
		strncpy(conninfo, temp, sizeof(conninfo) - 1);
		conninfo[sizeof(conninfo) - 1] = '\0';
	}

	/* Connect to database */
	conn = PQconnectdb(conninfo);

	if (PQstatus(conn) != CONNECTION_OK)
	{
		if (ctx->verbose)
			fprintf(stderr, "ramctrl: Connection failed: %s\n",
			        PQerrorMessage(conn));
		PQfinish(conn);
		return NULL;
	}

	/* Verify pgraft extension is available */
	PGresult* res =
	    PQexec(conn, "SELECT 1 FROM pg_extension WHERE extname = 'pgraft';");
	if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
	{
		if (ctx->verbose)
			fprintf(stderr, "ramctrl: pgraft extension not found\n");
		PQclear(res);
		PQfinish(conn);
		return NULL;
	}
	PQclear(res);

	return conn;
}


bool ramctrl_get_cluster_info(ramctrl_context_t* ctx,
                              ramctrl_cluster_info_t* cluster_info)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;

	if (!ctx || !cluster_info)
		return false;

	memset(cluster_info, 0, sizeof(ramctrl_cluster_info_t));

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	res = PQexec(conn, sql_cluster_status);
	if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
	{
		strncpy(cluster_info->cluster_name, PQgetvalue(res, 0, 1),
		        sizeof(cluster_info->cluster_name) - 1);
		cluster_info->total_nodes = atoi(PQgetvalue(res, 0, 2));
		cluster_info->primary_node_id = atoi(PQgetvalue(res, 0, 4));

		/* Parse cluster state */
		const char* state_str = PQgetvalue(res, 0, 5);
		if (strcmp(state_str, "healthy") == 0)
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_HEALTHY;
		else if (strcmp(state_str, "degraded") == 0)
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_DEGRADED;
		else if (strcmp(state_str, "failed") == 0)
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_FAILED;
		else if (strcmp(state_str, "maintenance") == 0)
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_MAINTENANCE;
		else
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_UNKNOWN;

		/* Parse last update timestamp */
		const char* timestamp_str = PQgetvalue(res, 0, 6);
		struct tm tm;
		if (strptime(timestamp_str, "%Y-%m-%d %H:%M:%S", &tm))
			cluster_info->last_update = mktime(&tm);

		result = true;
	}
	else if (ctx->verbose)
	{
		fprintf(stderr, "ramctrl: Query failed: %s\n", PQerrorMessage(conn));
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}


bool ramctrl_get_nodes_info(ramctrl_context_t* ctx, ramctrl_node_info_t* nodes,
                            int32_t* node_count, int32_t max_nodes)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;

	if (!ctx || !nodes || !node_count)
		return false;

	*node_count = 0;

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	res = PQexec(conn, sql_nodes_status);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		int ntuples = PQntuples(res);
		if (ntuples > 0)
		{
			int count = (ntuples < max_nodes) ? ntuples : max_nodes;
			*node_count = count;

			for (int i = 0; i < count; i++)
			{
				ramctrl_node_info_t* node = &nodes[i];

				node->node_id = atoi(PQgetvalue(res, i, 0));
				strncpy(node->hostname, PQgetvalue(res, i, 1),
				        sizeof(node->hostname) - 1);
				node->port = atoi(PQgetvalue(res, i, 2));

				/* Parse node state */
				const char* state_str = PQgetvalue(res, i, 3);
				if (strcmp(state_str, "running") == 0)
					node->status = RAMCTRL_NODE_STATUS_RUNNING;
				else if (strcmp(state_str, "stopped") == 0)
					node->status = RAMCTRL_NODE_STATUS_STOPPED;
				else if (strcmp(state_str, "failed") == 0)
					node->status = RAMCTRL_NODE_STATUS_FAILED;
				else if (strcmp(state_str, "maintenance") == 0)
					node->status = RAMCTRL_NODE_STATUS_MAINTENANCE;
				else
					node->status = RAMCTRL_NODE_STATUS_UNKNOWN;

				node->is_primary = (strcmp(PQgetvalue(res, i, 6), "t") == 0);

				/* Parse last seen timestamp */
				const char* timestamp_str = PQgetvalue(res, i, 7);
				struct tm tm;
				if (strptime(timestamp_str, "%Y-%m-%d %H:%M:%S", &tm))
					node->last_seen = mktime(&tm);

				/* Parse replication lag */
				const char* lag_str = PQgetvalue(res, i, 8);
				if (lag_str && strlen(lag_str) > 0)
					node->replication_lag_ms = atoi(lag_str);
			}

			result = true;
		}
		else
		{
			/* No nodes found, but query succeeded */
			result = true;
		}
	}
	else if (ctx->verbose)
	{
		fprintf(stderr, "ramctrl: Query failed: %s\n", PQerrorMessage(conn));
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}


bool ramctrl_promote_node(ramctrl_context_t* ctx, int32_t node_id)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;
	const char* params[1];
	char node_id_str[16];

	if (!ctx)
		return false;

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	snprintf(node_id_str, sizeof(node_id_str), "%d", node_id);
	params[0] = node_id_str;

	res = PQexecParams(conn, sql_promote_node, 1, NULL, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (ctx->verbose)
			printf("ramctrl: Node %d promotion initiated\n", node_id);
		result = true;
	}
	else if (ctx->verbose)
	{
		fprintf(stderr, "ramctrl: Promote failed: %s\n", PQerrorMessage(conn));
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}


bool ramctrl_demote_node(ramctrl_context_t* ctx, int32_t node_id)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;
	const char* params[1];
	char node_id_str[16];

	if (!ctx)
		return false;

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	snprintf(node_id_str, sizeof(node_id_str), "%d", node_id);
	params[0] = node_id_str;

	res = PQexecParams(conn, sql_demote_node, 1, NULL, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (ctx->verbose)
			printf("ramctrl: Node %d demotion initiated\n", node_id);
		result = true;
	}
	else if (ctx->verbose)
	{
		fprintf(stderr, "ramctrl: Demote failed: %s\n", PQerrorMessage(conn));
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}


bool ramctrl_add_node(ramctrl_context_t* ctx, int32_t node_id,
                      const char* hostname, int32_t port)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;
	const char* params[3];
	char node_id_str[16];
	char port_str[16];

	if (!ctx || !hostname)
		return false;

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	snprintf(node_id_str, sizeof(node_id_str), "%d", node_id);
	snprintf(port_str, sizeof(port_str), "%d", port);
	params[0] = node_id_str;
	params[1] = hostname;
	params[2] = port_str;

	res = PQexecParams(conn, sql_add_node, 3, NULL, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (ctx->verbose)
			printf("ramctrl: Node %d (%s:%d) added to cluster\n", node_id,
			       hostname, port);
		result = true;
	}
	else if (ctx->verbose)
	{
		fprintf(stderr, "ramctrl: Add node failed: %s\n", PQerrorMessage(conn));
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}


bool ramctrl_remove_node(ramctrl_context_t* ctx, int32_t node_id)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;
	const char* params[1];
	char node_id_str[16];

	if (!ctx)
		return false;

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	snprintf(node_id_str, sizeof(node_id_str), "%d", node_id);
	params[0] = node_id_str;

	res = PQexecParams(conn, sql_remove_node, 1, NULL, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (ctx->verbose)
			printf("ramctrl: Node %d removed from cluster\n", node_id);
		result = true;
	}
	else if (ctx->verbose)
	{
		fprintf(stderr, "ramctrl: Remove node failed: %s\n",
		        PQerrorMessage(conn));
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}


bool ramctrl_set_maintenance_mode(ramctrl_context_t* ctx, int32_t node_id,
                                  bool enable)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;
	const char* params[2];
	char node_id_str[16];
	const char* enable_str = enable ? "true" : "false";

	if (!ctx)
		return false;

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	snprintf(node_id_str, sizeof(node_id_str), "%d", node_id);
	params[0] = node_id_str;
	params[1] = enable_str;

	res =
	    PQexecParams(conn, sql_set_maintenance, 2, NULL, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (ctx->verbose)
			printf("ramctrl: Node %d maintenance mode %s\n", node_id,
			       enable ? "enabled" : "disabled");
		result = true;
	}
	else if (ctx->verbose)
	{
		fprintf(stderr, "ramctrl: Set maintenance mode failed: %s\n",
		        PQerrorMessage(conn));
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}


bool ramctrl_trigger_failover(ramctrl_context_t* ctx, int32_t target_node_id)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;
	const char* params[1];
	char node_id_str[16];

	if (!ctx)
		return false;

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	if (target_node_id > 0)
	{
		snprintf(node_id_str, sizeof(node_id_str), "%d", target_node_id);
		params[0] = node_id_str;
		res = PQexecParams(conn, sql_trigger_failover, 1, NULL, params, NULL,
		                   NULL, 0);
	}
	else
	{
		/* Automatic failover - let pgraft choose best candidate */
		params[0] = NULL;
		res = PQexecParams(conn, sql_trigger_failover, 1, NULL, params, NULL,
		                   NULL, 0);
	}

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (ctx->verbose)
		{
			if (target_node_id > 0)
				printf("ramctrl: Failover to node %d initiated\n",
				       target_node_id);
			else
				printf("ramctrl: Automatic failover initiated\n");
		}
		result = true;
	}
	else if (ctx->verbose)
	{
		fprintf(stderr, "ramctrl: Failover failed: %s\n", PQerrorMessage(conn));
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}

/* Get node information from pgraft extension */
bool ramctrl_get_node_info(ramctrl_context_t* ctx, ramctrl_node_info_t* nodes,
                           int32_t* node_count)
{
	PGconn* conn;
	PGresult* res;
	bool result = false;

	if (!ctx || !nodes || !node_count)
		return false;

	*node_count = 0;

	conn = ramctrl_connect_database(ctx);
	if (!conn)
		return false;

	res = PQexec(conn, sql_nodes_status);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		int ntuples = PQntuples(res);
		if (ntuples > 0)
		{
			int count =
			    (ntuples < RAMCTRL_MAX_NODES) ? ntuples : RAMCTRL_MAX_NODES;
			*node_count = count;

			for (int i = 0; i < count; i++)
			{
				ramctrl_node_info_t* node = &nodes[i];

				node->node_id = atoi(PQgetvalue(res, i, 0));
				strncpy(node->hostname, PQgetvalue(res, i, 1),
				        sizeof(node->hostname) - 1);
				node->port = atoi(PQgetvalue(res, i, 2));

				/* Parse node state */
				const char* state_str = PQgetvalue(res, i, 3);
				if (strcmp(state_str, "running") == 0)
					node->status = RAMCTRL_NODE_STATUS_RUNNING;
				else if (strcmp(state_str, "stopped") == 0)
					node->status = RAMCTRL_NODE_STATUS_STOPPED;
				else if (strcmp(state_str, "failed") == 0)
					node->status = RAMCTRL_NODE_STATUS_FAILED;
				else if (strcmp(state_str, "maintenance") == 0)
					node->status = RAMCTRL_NODE_STATUS_MAINTENANCE;
				else
					node->status = RAMCTRL_NODE_STATUS_UNKNOWN;

				node->is_primary = (strcmp(PQgetvalue(res, i, 6), "t") == 0);

				/* Parse last seen timestamp */
				const char* timestamp_str = PQgetvalue(res, i, 7);
				struct tm tm;
				if (strptime(timestamp_str, "%Y-%m-%d %H:%M:%S", &tm))
					node->last_seen = mktime(&tm);

				result = true;
			}
		}
	}

	PQclear(res);
	PQfinish(conn);
	return result;
}

/* Additional wrapper functions */
bool ramctrl_enable_maintenance_mode(ramctrl_context_t* ctx, int32_t node_id)
{
	ramctrl_database_connection_t conn;
	bool result;

	if (!ctx)
		return false;

	if (!ramctrl_database_connect(&conn, ctx->hostname, ctx->port,
	                              ctx->database, ctx->user, ctx->password))
		return false;

	result = ramctrl_database_enable_maintenance(&conn, node_id);
	ramctrl_database_disconnect(&conn);
	return result;
}


bool ramctrl_disable_maintenance_mode(ramctrl_context_t* ctx, int32_t node_id)
{
	ramctrl_database_connection_t conn;
	bool result;

	if (!ctx)
		return false;

	if (!ramctrl_database_connect(&conn, ctx->hostname, ctx->port,
	                              ctx->database, ctx->user, ctx->password))
		return false;

	result = ramctrl_database_disable_maintenance(&conn, node_id);
	ramctrl_database_disconnect(&conn);
	return result;
}


bool ramctrl_get_all_nodes(ramctrl_context_t* ctx, ramctrl_node_info_t** nodes,
                           int* node_count)
{
	ramctrl_database_connection_t conn;
	ramctrl_node_info_t static_nodes[32]; /* Static buffer for nodes */
	int32_t count = 0;
	bool result;

	if (!ctx || !nodes || !node_count)
		return false;

	if (!ramctrl_database_connect(&conn, ctx->hostname, ctx->port,
	                              ctx->database, ctx->user, ctx->password))
		return false;

	result = ramctrl_database_get_all_nodes(&conn, static_nodes, &count, 32);

	if (result && count > 0)
	{
		*nodes = malloc((size_t) count * sizeof(ramctrl_node_info_t));
		if (*nodes)
		{
			memcpy(*nodes, static_nodes,
			       (size_t) count * sizeof(ramctrl_node_info_t));
			*node_count = count;
		}
		else
		{
			result = false;
		}
	}
	else
	{
		*nodes = NULL;
		*node_count = 0;
	}

	ramctrl_database_disconnect(&conn);
	return result;
}

/* Low-level database operations */
bool ramctrl_database_connect(ramctrl_database_connection_t* conn,
                              const char* host, int32_t port,
                              const char* database, const char* user,
                              const char* password)
{
	char conninfo[512];
	PGconn* pgconn;

	if (!conn || !host || !database || !user)
		return false;

	/* Store connection parameters */
	strncpy(conn->host, host, sizeof(conn->host) - 1);
	conn->port = port;
	strncpy(conn->database, database, sizeof(conn->database) - 1);
	strncpy(conn->user, user, sizeof(conn->user) - 1);
	if (password)
		strncpy(conn->password, password, sizeof(conn->password) - 1);
	else
		conn->password[0] = '\0';

	/* Build connection string */
	snprintf(conninfo, sizeof(conninfo), "host=%s port=%d dbname=%s user=%s",
	         host, port, database, user);

	if (password && strlen(password) > 0)
	{
		strncat(conninfo,
		        " password=", sizeof(conninfo) - strlen(conninfo) - 1);
		strncat(conninfo, password, sizeof(conninfo) - strlen(conninfo) - 1);
	}

	/* Connect to database */
	pgconn = PQconnectdb(conninfo);

	if (PQstatus(pgconn) != CONNECTION_OK)
	{
		PQfinish(pgconn);
		return false;
	}

	conn->connection = pgconn;
	conn->is_connected = true;
	conn->last_activity = time(NULL);

	return true;
}


void ramctrl_database_disconnect(ramctrl_database_connection_t* conn)
{
	if (!conn)
		return;

	if (conn->connection)
	{
		PQfinish((PGconn*) conn->connection);
		conn->connection = NULL;
	}

	conn->is_connected = false;
}


bool ramctrl_database_is_connected(const ramctrl_database_connection_t* conn)
{
	if (!conn || !conn->connection)
		return false;

	return (PQstatus((PGconn*) conn->connection) == CONNECTION_OK);
}


bool ramctrl_database_reconnect(ramctrl_database_connection_t* conn)
{
	if (!conn)
		return false;

	ramctrl_database_disconnect(conn);
	return ramctrl_database_connect(conn, conn->host, conn->port,
	                                conn->database, conn->user, conn->password);
}


bool ramctrl_database_get_cluster_info(ramctrl_database_connection_t* conn,
                                       ramctrl_cluster_info_t* cluster_info)
{
	PGresult* res;
	bool result = false;

	if (!conn || !cluster_info || !conn->is_connected)
		return false;

	res = PQexec((PGconn*) conn->connection, sql_cluster_status);

	if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
	{
		cluster_info->cluster_id = atoi(PQgetvalue(res, 0, 0));
		strncpy(cluster_info->cluster_name, PQgetvalue(res, 0, 1),
		        sizeof(cluster_info->cluster_name) - 1);
		cluster_info->total_nodes = atoi(PQgetvalue(res, 0, 2));
		cluster_info->active_nodes = atoi(PQgetvalue(res, 0, 3));
		cluster_info->primary_node_id = atoi(PQgetvalue(res, 0, 4));

		const char* state_str = PQgetvalue(res, 0, 5);
		if (strcmp(state_str, "healthy") == 0)
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_HEALTHY;
		else if (strcmp(state_str, "degraded") == 0)
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_DEGRADED;
		else if (strcmp(state_str, "failed") == 0)
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_FAILED;
		else if (strcmp(state_str, "maintenance") == 0)
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_MAINTENANCE;
		else
			cluster_info->status = RAMCTRL_CLUSTER_STATUS_UNKNOWN;

		cluster_info->last_update = time(NULL);
		result = true;
	}

	PQclear(res);
	return result;
}


bool ramctrl_database_get_all_nodes(ramctrl_database_connection_t* conn,
                                    ramctrl_node_info_t nodes[],
                                    int32_t* node_count, int32_t max_nodes)
{
	PGresult* res;
	int num_rows;
	int i;
	bool result = false;

	if (!conn || !nodes || !node_count || !conn->is_connected)
		return false;

	res = PQexec((PGconn*) conn->connection, sql_nodes_status);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		num_rows = PQntuples(res);
		*node_count = (num_rows > max_nodes) ? max_nodes : num_rows;

		for (i = 0; i < *node_count; i++)
		{
			ramctrl_node_info_t* node = &nodes[i];

			node->node_id = atoi(PQgetvalue(res, i, 0));
			strncpy(node->hostname, PQgetvalue(res, i, 1),
			        sizeof(node->hostname) - 1);
			node->port = atoi(PQgetvalue(res, i, 2));

			const char* state_str = PQgetvalue(res, i, 3);
			if (strcmp(state_str, "running") == 0)
				node->status = RAMCTRL_NODE_STATUS_RUNNING;
			else if (strcmp(state_str, "stopped") == 0)
				node->status = RAMCTRL_NODE_STATUS_STOPPED;
			else if (strcmp(state_str, "failed") == 0)
				node->status = RAMCTRL_NODE_STATUS_FAILED;
			else if (strcmp(state_str, "maintenance") == 0)
				node->status = RAMCTRL_NODE_STATUS_MAINTENANCE;
			else
				node->status = RAMCTRL_NODE_STATUS_UNKNOWN;

			node->is_primary = (strcmp(PQgetvalue(res, i, 6), "t") == 0);
			node->last_seen = time(NULL);
		}

		result = true;
	}

	PQclear(res);
	return result;
}


bool ramctrl_database_promote_node(ramctrl_database_connection_t* conn,
                                   int32_t node_id)
{
	char query[256];
	PGresult* res;
	bool result = false;

	if (!conn || !conn->is_connected)
		return false;

	snprintf(query, sizeof(query), "SELECT ram.promote_node(%d)", node_id);
	res = PQexec((PGconn*) conn->connection, query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
		result = true;

	PQclear(res);
	return result;
}


bool ramctrl_database_demote_node(ramctrl_database_connection_t* conn,
                                  int32_t node_id)
{
	char query[256];
	PGresult* res;
	bool result = false;

	if (!conn || !conn->is_connected)
		return false;

	snprintf(query, sizeof(query), "SELECT ram.demote_node(%d)", node_id);
	res = PQexec((PGconn*) conn->connection, query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
		result = true;

	PQclear(res);
	return result;
}


bool ramctrl_database_trigger_failover(ramctrl_database_connection_t* conn,
                                       int32_t target_node_id)
{
	char query[256];
	PGresult* res;
	bool result = false;

	if (!conn || !conn->is_connected)
		return false;

	snprintf(query, sizeof(query), "SELECT ram.trigger_failover(%d)",
	         target_node_id);
	res = PQexec((PGconn*) conn->connection, query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
		result = true;

	PQclear(res);
	return result;
}


bool ramctrl_database_add_node(ramctrl_database_connection_t* conn,
                               int32_t node_id, const char* hostname,
                               int32_t port)
{
	char query[512];
	PGresult* res;
	bool result = false;

	if (!conn || !hostname || !conn->is_connected)
		return false;

	snprintf(query, sizeof(query), "SELECT ram.add_node(%d, '%s', %d)", node_id,
	         hostname, port);
	res = PQexec((PGconn*) conn->connection, query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
		result = true;

	PQclear(res);
	return result;
}


bool ramctrl_database_remove_node(ramctrl_database_connection_t* conn,
                                  int32_t node_id)
{
	char query[256];
	PGresult* res;
	bool result = false;

	if (!conn || !conn->is_connected)
		return false;

	snprintf(query, sizeof(query), "SELECT ram.remove_node(%d)", node_id);
	res = PQexec((PGconn*) conn->connection, query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
		result = true;

	PQclear(res);
	return result;
}


bool ramctrl_database_enable_maintenance(ramctrl_database_connection_t* conn,
                                         int32_t node_id)
{
	char query[256];
	PGresult* res;
	bool result = false;

	if (!conn || !conn->is_connected)
		return false;

	snprintf(query, sizeof(query), "SELECT ram.set_maintenance_mode(%d, true)",
	         node_id);
	res = PQexec((PGconn*) conn->connection, query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
		result = true;

	PQclear(res);
	return result;
}


bool ramctrl_database_disable_maintenance(ramctrl_database_connection_t* conn,
                                          int32_t node_id)
{
	char query[256];
	PGresult* res;
	bool result = false;

	if (!conn || !conn->is_connected)
		return false;

	snprintf(query, sizeof(query), "SELECT ram.set_maintenance_mode(%d, false)",
	         node_id);
	res = PQexec((PGconn*) conn->connection, query);

	if (PQresultStatus(res) == PGRES_TUPLES_OK)
		result = true;

	PQclear(res);
	return result;
}
