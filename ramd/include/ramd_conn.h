/*-------------------------------------------------------------------------
 *
 * ramd_conn.h
 *		PostgreSQL Auto-Failover Daemon - PostgreSQL Connection API
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_CONN_H
#define RAMD_CONN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libpq-fe.h>
#include "ramd.h"

/* Initialize connection subsystem */
extern bool ramd_conn_init(void);

/* Cleanup connection subsystem */
extern void ramd_conn_cleanup(void);

/* Get a connection to PostgreSQL */
extern PGconn* ramd_conn_get(const char* host, int32_t port, const char* dbname,
                             const char* user, const char* password);

/* Get a cached connection to PostgreSQL */
extern PGconn* ramd_conn_get_cached(int32_t node_id, const char* host, int32_t port,
                                    const char* dbname, const char* user, const char* password);

/* Execute a query and return result */
extern PGresult* ramd_conn_exec(PGconn* conn, const char* query);

/* Close a connection */
extern void ramd_conn_close(PGconn* conn);

#ifdef __cplusplus
}
#endif

#endif /* RAMD_CONN_H */
