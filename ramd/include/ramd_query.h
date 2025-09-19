/*-------------------------------------------------------------------------
 *
 * ramd_query.h
 *		PostgreSQL Auto-Failover Daemon - SQL Query Execution API
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * This file provides a clean API for SQL query execution and result handling.
 * All SQL operations go through this module for consistency and error handling.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_QUERY_H
#define RAMD_QUERY_H

#include <libpq-fe.h>

/* Query result types */
typedef enum
{
	RAMD_QUERY_SUCCESS = 0,
	RAMD_QUERY_ERROR = -1,
	RAMD_QUERY_NO_RESULT = -2,
	RAMD_QUERY_CONNECTION_ERROR = -3
} ramd_query_result_t;

/* Initialize query subsystem */
extern bool ramd_query_init(void);

/* Cleanup query subsystem */
extern void ramd_query_cleanup(void);

/*
 * Execute a simple SQL query on a connection
 * Returns RAMD_QUERY_SUCCESS on success, error code on failure
 */
extern ramd_query_result_t ramd_query_exec(PGconn* conn, const char* sql);

/*
 * Execute a SQL query and return the result
 * Returns PGresult* on success, NULL on error
 * The caller is responsible for calling PQclear on the result
 */
extern PGresult* ramd_query_exec_with_result(PGconn* conn, const char* sql);

/*
 * Execute a parameterized query
 * Returns PGresult* on success, NULL on error
 */
extern PGresult* ramd_query_exec_params(PGconn* conn, const char* sql, 
                                       int nParams, const Oid* paramTypes,
                                       const char* const* paramValues,
                                       const int* paramLengths,
                                       const int* paramFormats, int resultFormat);

/*
 * Check if a query result is successful
 * Returns true if result indicates success, false otherwise
 */
extern bool ramd_query_result_ok(PGresult* result);

/*
 * Get the first row, first column value as string
 * Returns NULL if no result or error
 */
extern const char* ramd_query_get_string_value(PGresult* result, int row, int col);

/*
 * Get the first row, first column value as integer
 * Returns 0 if no result or error
 */
extern int ramd_query_get_int_value(PGresult* result, int row, int col);

/*
 * Get the first row, first column value as boolean
 * Returns false if no result or error
 */
extern bool ramd_query_get_bool_value(PGresult* result, int row, int col);

/*
 * Execute a query and get a single string result
 * Returns the result string on success, NULL on error
 * The returned string is owned by the caller and should be freed
 */
extern char* ramd_query_get_single_string(PGconn* conn, const char* sql);

/*
 * Execute a query and get a single integer result
 * Returns the result integer on success, -1 on error
 */
extern int ramd_query_get_single_int(PGconn* conn, const char* sql);

/*
 * Execute a query and get a single boolean result
 * Returns the result boolean on success, false on error
 */
extern bool ramd_query_get_single_bool(PGconn* conn, const char* sql);

/*
 * Check if PostgreSQL is in recovery mode
 * Returns true if in recovery, false if primary, error on connection failure
 */
extern bool ramd_query_is_in_recovery(PGconn* conn);

/*
 * Check if PostgreSQL is accepting connections
 * Returns true if accepting, false otherwise
 */
extern bool ramd_query_is_accepting_connections(PGconn* conn);

/*
 * Get PostgreSQL version string
 * Returns version string on success, NULL on error
 */
extern char* ramd_query_get_version(PGconn* conn);

/*
 * Get current WAL LSN
 * Returns LSN string on success, NULL on error
 */
extern char* ramd_query_get_current_wal_lsn(PGconn* conn);

/*
 * Get replication lag in seconds
 * Returns lag in seconds, -1 on error
 */
extern double ramd_query_get_replication_lag(PGconn* conn);

/*
 * Check if there are active transactions
 * Returns true if active transactions exist, false otherwise
 */
extern bool ramd_query_has_active_transactions(PGconn* conn);

/*
 * Reload PostgreSQL configuration
 * Returns true on success, false on failure
 */
extern bool ramd_query_reload_config(PGconn* conn);

#endif /* RAMD_QUERY_H */
