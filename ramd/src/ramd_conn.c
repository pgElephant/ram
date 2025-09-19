/*-------------------------------------------------------------------------
 *
 * ramd_conn.c
 *		PostgreSQL Auto-Failover Daemon - PostgreSQL Connection API
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <libpq-fe.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "ramd_conn.h"
#include "ramd_logging.h"

/* Connection cache to avoid repeated connections */
static PGconn* g_conn_cache[RAMD_MAX_NODES] = {NULL};
static pthread_mutex_t g_conn_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Initialize connection subsystem */
bool
ramd_conn_init(void)
{
	pthread_mutex_lock(&g_conn_mutex);
	memset(g_conn_cache, 0, sizeof(g_conn_cache));
	pthread_mutex_unlock(&g_conn_mutex);
	
	ramd_log_info("Connection subsystem initialized");
	return true;
}

/* Cleanup connection subsystem */
void
ramd_conn_cleanup(void)
{
	pthread_mutex_lock(&g_conn_mutex);
	for (int i = 0; i < RAMD_MAX_NODES; i++)
	{
		if (g_conn_cache[i])
		{
			PQfinish(g_conn_cache[i]);
			g_conn_cache[i] = NULL;
		}
	}
	pthread_mutex_unlock(&g_conn_mutex);
	
	ramd_log_info("Connection subsystem cleaned up");
}

/* Get a connection to PostgreSQL */
PGconn*
ramd_conn_get(const char* host, int32_t port, const char* dbname,
              const char* user, const char* password)
{
	char conninfo[512];
	PGconn* conn;
	
	/* Build connection string */
	snprintf(conninfo, sizeof(conninfo),
	         "host=%s port=%d dbname=%s user=%s password=%s",
	         host, port, dbname ? dbname : "postgres",
	         user ? user : "postgres",
	         password ? password : "");
	
	/* Attempt connection */
	conn = PQconnectdb(conninfo);
	if (PQstatus(conn) != CONNECTION_OK)
	{
		ramd_log_error("Failed to connect to PostgreSQL: %s",
		               PQerrorMessage(conn));
		PQfinish(conn);
		return NULL;
	}
	
	return conn;
}

/* Get a cached connection to PostgreSQL */
PGconn*
ramd_conn_get_cached(int32_t node_id, const char* host, int32_t port,
                     const char* dbname, const char* user, const char* password)
{
	PGconn* conn = NULL;
	
	if (node_id <= 0 || node_id > RAMD_MAX_NODES)
		return NULL;
	
	pthread_mutex_lock(&g_conn_mutex);
	
	/* Check if we have a cached connection */
	if (g_conn_cache[node_id - 1])
	{
		/* Check if connection is still valid */
		if (PQstatus(g_conn_cache[node_id - 1]) == CONNECTION_OK)
		{
			conn = g_conn_cache[node_id - 1];
			pthread_mutex_unlock(&g_conn_mutex);
			return conn;
		}
		
		/* Connection is bad, close it */
		PQfinish(g_conn_cache[node_id - 1]);
		g_conn_cache[node_id - 1] = NULL;
	}
	
	/* Get new connection */
	conn = ramd_conn_get(host, port, dbname, user, password);
	if (conn)
		g_conn_cache[node_id - 1] = conn;
	
	pthread_mutex_unlock(&g_conn_mutex);
	return conn;
}

/* Execute a query and return result */
PGresult*
ramd_conn_exec(PGconn* conn, const char* query)
{
	PGresult* res;
	
	if (!conn || !query)
		return NULL;
	
	res = PQexec(conn, query);
	if (PQresultStatus(res) != PGRES_TUPLES_OK &&
	    PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		ramd_log_error("Query failed: %s", PQerrorMessage(conn));
		PQclear(res);
		return NULL;
	}
	
	return res;
}

/* Close a connection */
void
ramd_conn_close(PGconn* conn)
{
	if (conn)
		PQfinish(conn);
}
