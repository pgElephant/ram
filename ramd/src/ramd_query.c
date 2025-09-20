/*-------------------------------------------------------------------------
 *
 * ramd_query.c
 *		PostgreSQL Auto-Failover Daemon - SQL Query Execution Implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * This module provides a clean API for SQL query execution and result handling.
 * All SQL operations go through this module for consistency and error handling.
 *
 *-------------------------------------------------------------------------
 */

#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ramd_query.h"
#include "ramd_logging.h"

static bool g_query_initialized = false;

bool ramd_query_init(void)
{
	if (g_query_initialized)
		return true;
	
	ramd_log_info("Initializing query subsystem");
	g_query_initialized = true;
	return true;
}

void ramd_query_cleanup(void)
{
	if (!g_query_initialized)
		return;
	
	ramd_log_info("Cleaning up query subsystem");
	g_query_initialized = false;
}

ramd_query_result_t ramd_query_exec(PGconn* conn, const char* sql)
{
	PGresult* result;
	
	if (!conn || !sql)
	{
		ramd_log_error("Invalid parameters for query execution");
		return RAMD_QUERY_ERROR;
	}
	
	if (PQstatus(conn) != CONNECTION_OK)
	{
		ramd_log_error("Database connection is not OK: %s", PQerrorMessage(conn));
		return RAMD_QUERY_CONNECTION_ERROR;
	}
	
	result = PQexec(conn, sql);
	if (!result)
	{
		ramd_log_error("Failed to execute query: %s", PQerrorMessage(conn));
		return RAMD_QUERY_ERROR;
	}
	
	if (PQresultStatus(result) == PGRES_COMMAND_OK || 
	    PQresultStatus(result) == PGRES_TUPLES_OK)
	{
		PQclear(result);
		return RAMD_QUERY_SUCCESS;
	}
	
	ramd_log_error("Query execution failed: %s", PQresultErrorMessage(result));
	PQclear(result);
	return RAMD_QUERY_ERROR;
}

PGresult* ramd_query_exec_with_result(PGconn* conn, const char* sql)
{
	PGresult* result;
	
	if (!conn || !sql)
	{
		ramd_log_error("Invalid parameters for query execution");
		return NULL;
	}
	
	if (PQstatus(conn) != CONNECTION_OK)
	{
		ramd_log_error("Database connection is not OK: %s", PQerrorMessage(conn));
		return NULL;
	}
	
	result = PQexec(conn, sql);
	if (!result)
	{
		ramd_log_error("Failed to execute query: %s", PQerrorMessage(conn));
		return NULL;
	}
	
	if (PQresultStatus(result) != PGRES_TUPLES_OK && 
	    PQresultStatus(result) != PGRES_COMMAND_OK)
	{
		ramd_log_error("Query execution failed: %s", PQresultErrorMessage(result));
		PQclear(result);
		return NULL;
	}
	
	return result;
}

PGresult* ramd_query_exec_params(PGconn* conn, const char* sql, 
                                int nParams, const Oid* paramTypes,
                                const char* const* paramValues,
                                const int* paramLengths,
                                const int* paramFormats, int resultFormat)
{
	PGresult* result;
	
	if (!conn || !sql)
	{
		ramd_log_error("Invalid parameters for parameterized query execution");
		return NULL;
	}
	
	if (PQstatus(conn) != CONNECTION_OK)
	{
		ramd_log_error("Database connection is not OK: %s", PQerrorMessage(conn));
		return NULL;
	}
	
	result = PQexecParams(conn, sql, nParams, paramTypes, paramValues,
	                     paramLengths, paramFormats, resultFormat);
	if (!result)
	{
		ramd_log_error("Failed to execute parameterized query: %s", PQerrorMessage(conn));
		return NULL;
	}
	
	if (PQresultStatus(result) != PGRES_TUPLES_OK && 
	    PQresultStatus(result) != PGRES_COMMAND_OK)
	{
		ramd_log_error("Parameterized query execution failed: %s", PQresultErrorMessage(result));
		PQclear(result);
		return NULL;
	}
	
	return result;
}

bool ramd_query_result_ok(PGresult* result)
{
	if (!result)
		return false;
	
	return (PQresultStatus(result) == PGRES_COMMAND_OK || 
	        PQresultStatus(result) == PGRES_TUPLES_OK);
}

const char* ramd_query_get_string_value(PGresult* result, int row, int col)
{
	if (!result || row < 0 || col < 0 || 
	    row >= PQntuples(result) || col >= PQnfields(result))
		return NULL;
	
	return PQgetvalue(result, row, col);
}

int ramd_query_get_int_value(PGresult* result, int row, int col)
{
	const char* value = ramd_query_get_string_value(result, row, col);
	if (!value)
		return 0;
	
	return atoi(value);
}

bool ramd_query_get_bool_value(PGresult* result, int row, int col)
{
	const char* value = ramd_query_get_string_value(result, row, col);
	if (!value)
		return false;
	
	return (strcmp(value, "t") == 0 || strcmp(value, "true") == 0 || 
	        strcmp(value, "1") == 0 || strcmp(value, "yes") == 0);
}

char* ramd_query_get_single_string(PGconn* conn, const char* sql)
{
	PGresult* result;
	char* value = NULL;
	
	result = ramd_query_exec_with_result(conn, sql);
	if (result && PQntuples(result) > 0)
	{
		const char* str_value = PQgetvalue(result, 0, 0);
		if (str_value)
		{
			value = malloc(strlen(str_value) + 1);
			if (value)
				strcpy(value, str_value);
		}
	}
	
	if (result)
		PQclear(result);
	
	return value;
}

int ramd_query_get_single_int(PGconn* conn, const char* sql)
{
	PGresult* result;
	int value = -1;
	
	result = ramd_query_exec_with_result(conn, sql);
	if (result && PQntuples(result) > 0)
	{
		value = ramd_query_get_int_value(result, 0, 0);
	}
	
	if (result)
		PQclear(result);
	
	return value;
}

bool ramd_query_get_single_bool(PGconn* conn, const char* sql)
{
	PGresult* result;
	bool value = false;
	
	result = ramd_query_exec_with_result(conn, sql);
	if (result && PQntuples(result) > 0)
	{
		value = ramd_query_get_bool_value(result, 0, 0);
	}
	
	if (result)
		PQclear(result);
	
	return value;
}

bool ramd_query_is_in_recovery(PGconn* conn)
{
	if (!conn)
		return false;
	
	return ramd_query_get_single_bool(conn, "SELECT pg_is_in_recovery()");
}

bool ramd_query_is_accepting_connections(PGconn* conn)
{
	if (!conn)
		return false;
	
	PGresult* result = ramd_query_exec_with_result(conn, "SELECT 1");
	if (result)
	{
		bool success = ramd_query_result_ok(result);
		PQclear(result);
		return success;
	}
	
	return false;
}

char* ramd_query_get_version(PGconn* conn)
{
	if (!conn)
		return NULL;
	
	return ramd_query_get_single_string(conn, "SELECT version()");
}

char* ramd_query_get_current_wal_lsn(PGconn* conn)
{
	if (!conn)
		return NULL;
	
	return ramd_query_get_single_string(conn, "SELECT pg_current_wal_lsn()");
}

double ramd_query_get_replication_lag(PGconn* conn)
{
	PGresult* result;
	double lag = -1.0;
	
	if (!conn)
		return lag;
	
	result = ramd_query_exec_with_result(conn, 
		"SELECT EXTRACT(EPOCH FROM (now() - pg_last_xact_replay_timestamp()))");
	
	if (result && PQntuples(result) > 0)
	{
		const char* lag_str = PQgetvalue(result, 0, 0);
		if (lag_str)
		{
			lag = atof(lag_str);
		}
	}
	
	if (result)
		PQclear(result);
	
	return lag;
}

bool ramd_query_has_active_transactions(PGconn* conn)
{
	PGresult* result;
	bool has_active = false;
	
	if (!conn)
		return false;
	
	result = ramd_query_exec_with_result(conn, 
		"SELECT count(*) FROM pg_stat_activity WHERE state = 'active' AND pid != pg_backend_pid()");
	
	if (result && PQntuples(result) > 0)
	{
		int active_count = ramd_query_get_int_value(result, 0, 0);
		has_active = (active_count > 0);
	}
	
	if (result)
		PQclear(result);
	
	return has_active;
}

bool ramd_query_reload_config(PGconn* conn)
{
	if (!conn)
		return false;
	
	return (ramd_query_exec(conn, "SELECT pg_reload_conf()") == RAMD_QUERY_SUCCESS);
}
