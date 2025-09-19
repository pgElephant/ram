/*-------------------------------------------------------------------------
 *
 * ramd_pgraft.h
 *		PostgreSQL RAM Daemon - pgraft function declarations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * This file provides C function declarations for calling pgraft extension
 * functions from the ramd daemon. These functions handle the interface
 * between ramd and the pgraft PostgreSQL extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_PGRAFT_H
#define RAMD_PGRAFT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libpq-fe.h"

/* Return codes for pgraft functions */
#define RAMD_PGRAFT_SUCCESS 0
#define RAMD_PGRAFT_ERROR -1
#define RAMD_PGRAFT_NOT_INITIALIZED -2
#define RAMD_PGRAFT_NOT_LEADER -3

/* Core pgraft functions used by ramd */

/*
 * Initialize the pgraft system
 * Returns: RAMD_PGRAFT_SUCCESS on success, error code on failure
 */
extern int ramd_pgraft_init(PGconn* conn, int node_id, const char* hostname, int port);

/*
 * Start the pgraft system
 * Returns: RAMD_PGRAFT_SUCCESS on success, error code on failure
 */
extern int ramd_pgraft_start(PGconn* conn);

/*
 * Stop the pgraft system
 * Returns: RAMD_PGRAFT_SUCCESS on success, error code on failure
 */
extern int ramd_pgraft_stop(PGconn* conn);

/*
 * Get the current state of the pgraft system
 * Returns: state string ("stopped", "running", "error")
 */
extern char* ramd_pgraft_get_state(PGconn* conn);

/*
 * Check if the current node is the Raft leader
 * Returns: 1 if leader, 0 if not leader, -1 on error
 */
extern int ramd_pgraft_is_leader(PGconn* conn);

/*
 * Get the current Raft leader node ID
 * Returns: leader node ID, or -1 on error
 */
extern int ramd_pgraft_get_leader(PGconn* conn);

/*
 * Get the current Raft term
 * Returns: current term, or -1 on error
 */
extern long long ramd_pgraft_get_term(PGconn* conn);

/*
 * Get the list of nodes in the Raft cluster
 * Returns: JSON string with node information, or NULL on error
 * Caller must free the returned string
 */
extern char* ramd_pgraft_get_nodes(PGconn* conn);

/* Cluster management functions */

/*
 * Add a node to the Raft cluster
 * Returns: RAMD_PGRAFT_SUCCESS on success, error code on failure
 */
extern int ramd_pgraft_add_node(PGconn* conn, int node_id, const char* hostname, int port);

/*
 * Remove a node from the Raft cluster
 * Returns: RAMD_PGRAFT_SUCCESS on success, error code on failure
 */
extern int ramd_pgraft_remove_node(PGconn* conn, int node_id);

/*
 * Get cluster health information
 * Returns: JSON string with health information, or NULL on error
 * Caller must free the returned string
 */
extern char* ramd_pgraft_get_cluster_health(PGconn* conn);

/*
 * Check if the cluster is healthy
 * Returns: 1 if healthy, 0 if not healthy, -1 on error
 */
extern int ramd_pgraft_is_cluster_healthy(PGconn* conn);

/*
 * Get cluster performance metrics
 * Returns: JSON string with metrics, or NULL on error
 * Caller must free the returned string
 */
extern char* ramd_pgraft_get_performance_metrics(PGconn* conn);

/* Log and consensus functions */

/*
 * Append a log entry to the Raft log
 * Returns: RAMD_PGRAFT_SUCCESS on success, error code on failure
 */
extern int ramd_pgraft_append_log(PGconn* conn, const char* log_data);

/*
 * Get Raft log entries
 * Returns: JSON string with log entries, or NULL on error
 * Caller must free the returned string
 */
extern char* ramd_pgraft_get_log(PGconn* conn);

/*
 * Get Raft statistics
 * Returns: JSON string with statistics, or NULL on error
 * Caller must free the returned string
 */
extern char* ramd_pgraft_get_stats(PGconn* conn);

/* Utility functions */

/*
 * Get the pgraft extension version
 * Returns: version string, or NULL on error
 * Caller must free the returned string
 */
extern char* ramd_pgraft_get_version(PGconn* conn);

/*
 * Test the pgraft system (for debugging)
 * Returns: test result integer, or -1 on error
 */
extern int ramd_pgraft_test(PGconn* conn);

/*
 * Get the last error message from pgraft operations
 * Returns: error message string, or NULL if no error
 * This is a static string, do not free it
 */
extern const char* ramd_pgraft_get_last_error(void);

/*
 * Check if pgraft extension is available and properly configured
 * Returns: 1 if available, 0 if not available, -1 on error
 */
extern int ramd_pgraft_check_availability(PGconn* conn);

/* Replica setup functions */

/*
 * Set up a new replica node using base backup and add to Raft cluster
 * This function takes a base backup from the current node and sets up
 * a new replica node, then adds it to the Raft cluster
 * 
 * Parameters:
 *   conn - PostgreSQL connection to current node
 *   replica_node_id - ID of the new replica node
 *   replica_hostname - Hostname of the new replica node
 *   replica_port - PostgreSQL port of the new replica node
 *   backup_dir - Directory to store the base backup
 * 
 * Returns: RAMD_PGRAFT_SUCCESS on success, error code on failure
 */
extern int ramd_pgraft_setup_replica(PGconn* conn, int replica_node_id, 
                                    const char* replica_hostname, int replica_port,
                                    const char* backup_dir);

#ifdef __cplusplus
}
#endif

#endif /* RAMD_PGRAFT_H */
