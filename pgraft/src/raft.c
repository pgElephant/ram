/*-------------------------------------------------------------------------
 *
 * raft.c
 *		Raft consensus algorithm implementation for pgraft extension
 *		Uses etcd-io/raft library via Go wrapper
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "../include/pgraft.h"
#include <dlfcn.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Dynamic loading of Go Raft library */
static void *go_lib_handle = NULL;
static bool go_lib_loaded = false;

/* Function pointers for Go Raft functions */
typedef int (*pgraft_go_init_func)(int nodeID, char* address, int port);
typedef int (*pgraft_go_start_func)(void);
typedef int (*pgraft_go_stop_func)(void);
typedef int (*pgraft_go_add_peer_func)(int nodeID, char* address, int port);
typedef int (*pgraft_go_remove_peer_func)(int nodeID);
typedef char* (*pgraft_go_get_state_func)(void);
typedef int (*pgraft_go_get_leader_func)(void);
typedef long (*pgraft_go_get_term_func)(void);
typedef int (*pgraft_go_append_log_func)(char* data, int length);
typedef char* (*pgraft_go_get_stats_func)(void);
typedef char* (*pgraft_go_get_nodes_func)(void);
typedef char* (*pgraft_go_get_logs_func)(void);
typedef int (*pgraft_go_commit_log_func)(long index);
typedef int (*pgraft_go_step_message_func)(char* data, int length);
typedef char* (*pgraft_go_get_network_status_func)(void);
typedef void (*pgraft_go_free_string_func)(char* str);

/* Replication function pointers */
typedef int (*pgraft_go_replicate_log_entry_func)(char* data, int dataLen);
typedef char* (*pgraft_go_get_replication_status_func)(void);
typedef char* (*pgraft_go_create_snapshot_func)(void);
typedef int (*pgraft_go_apply_snapshot_func)(char* snapshotData);
typedef int (*pgraft_go_replicate_to_node_func)(unsigned long long nodeID, char* data, int dataLen);
typedef double (*pgraft_go_get_replication_lag_func)(void);
typedef int (*pgraft_go_sync_replication_func)(void);

static pgraft_go_init_func pgraft_go_init_ptr = NULL;
static pgraft_go_start_func pgraft_go_start_ptr = NULL;
static pgraft_go_stop_func pgraft_go_stop_ptr = NULL;
static pgraft_go_add_peer_func pgraft_go_add_peer_ptr = NULL;
static pgraft_go_remove_peer_func pgraft_go_remove_peer_ptr = NULL;
static pgraft_go_get_state_func pgraft_go_get_state_ptr = NULL;
static pgraft_go_get_leader_func pgraft_go_get_leader_ptr = NULL;
static pgraft_go_get_term_func pgraft_go_get_term_ptr = NULL;
static pgraft_go_append_log_func pgraft_go_append_log_ptr = NULL;
static pgraft_go_get_stats_func pgraft_go_get_stats_ptr = NULL;
static pgraft_go_get_nodes_func pgraft_go_get_nodes_ptr = NULL;
static pgraft_go_get_logs_func pgraft_go_get_logs_ptr = NULL;
static pgraft_go_commit_log_func pgraft_go_commit_log_ptr = NULL;
static pgraft_go_step_message_func pgraft_go_step_message_ptr = NULL;
static pgraft_go_get_network_status_func pgraft_go_get_network_status_ptr = NULL;
static pgraft_go_free_string_func pgraft_go_free_string_ptr = NULL;

/* Replication function pointer variables */
static pgraft_go_replicate_log_entry_func pgraft_go_replicate_log_entry_ptr = NULL;
static pgraft_go_get_replication_status_func pgraft_go_get_replication_status_ptr = NULL;
static pgraft_go_create_snapshot_func pgraft_go_create_snapshot_ptr = NULL;
static pgraft_go_apply_snapshot_func pgraft_go_apply_snapshot_ptr = NULL;
static pgraft_go_replicate_to_node_func pgraft_go_replicate_to_node_ptr = NULL;
static pgraft_go_get_replication_lag_func pgraft_go_get_replication_lag_ptr = NULL;
static pgraft_go_sync_replication_func pgraft_go_sync_replication_ptr = NULL;

static pgraft_raft_state_t raft_state;

/*
 * Load Go Raft library
 */
static int
load_go_library(void)
{
    int retry_count = 0;
    const int max_retries = 3;
    
    if (go_lib_loaded)
        return 0;
    
	/* Try to load the Go library with retries */
	while (retry_count < max_retries)
	{
		char		lib_path[MAXPGPATH];
		const char *pg_libdir;

		/* Get PostgreSQL library directory */
		pg_libdir = pkglib_path;
		snprintf(lib_path, sizeof(lib_path), "%s/pgraft_go.dylib", pg_libdir);

		go_lib_handle = dlopen(lib_path, RTLD_LAZY);
		if (go_lib_handle)
			break;
            
        retry_count++;
        elog(WARNING, "pgraft_raft: attempt %d failed to load Go library: %s", 
             retry_count, dlerror());
        
        if (retry_count < max_retries)
        {
            /* Wait before retry with exponential backoff */
            pg_usleep(100000 * retry_count); /* 100ms * retry_count */
        }
    }
    
    if (!go_lib_handle)
    {
        elog(ERROR, "pgraft_raft: failed to load Go library after %d attempts", max_retries);
        return -1;
    }
    
    /* Load function pointers */
    pgraft_go_init_ptr = (pgraft_go_init_func) dlsym(go_lib_handle, "pgraft_go_init");
    pgraft_go_start_ptr = (pgraft_go_start_func) dlsym(go_lib_handle, "pgraft_go_start");
    pgraft_go_stop_ptr = (pgraft_go_stop_func) dlsym(go_lib_handle, "pgraft_go_stop");
    pgraft_go_add_peer_ptr = (pgraft_go_add_peer_func) dlsym(go_lib_handle, "pgraft_go_add_peer");
    pgraft_go_remove_peer_ptr = (pgraft_go_remove_peer_func) dlsym(go_lib_handle, "pgraft_go_remove_peer");
    pgraft_go_get_state_ptr = (pgraft_go_get_state_func) dlsym(go_lib_handle, "pgraft_go_get_state");
    pgraft_go_get_leader_ptr = (pgraft_go_get_leader_func) dlsym(go_lib_handle, "pgraft_go_get_leader");
    pgraft_go_get_term_ptr = (pgraft_go_get_term_func) dlsym(go_lib_handle, "pgraft_go_get_term");
    pgraft_go_append_log_ptr = (pgraft_go_append_log_func) dlsym(go_lib_handle, "pgraft_go_append_log");
    pgraft_go_get_stats_ptr = (pgraft_go_get_stats_func) dlsym(go_lib_handle, "pgraft_go_get_stats");
    pgraft_go_get_nodes_ptr = (pgraft_go_get_nodes_func) dlsym(go_lib_handle, "pgraft_go_get_nodes");
    pgraft_go_get_logs_ptr = (pgraft_go_get_logs_func) dlsym(go_lib_handle, "pgraft_go_get_logs");
    pgraft_go_commit_log_ptr = (pgraft_go_commit_log_func) dlsym(go_lib_handle, "pgraft_go_commit_log");
    pgraft_go_step_message_ptr = (pgraft_go_step_message_func) dlsym(go_lib_handle, "pgraft_go_step_message");
    pgraft_go_get_network_status_ptr = (pgraft_go_get_network_status_func) dlsym(go_lib_handle, "pgraft_go_get_network_status");
    pgraft_go_free_string_ptr = (pgraft_go_free_string_func) dlsym(go_lib_handle, "pgraft_go_free_string");
    
    /* Load replication function pointers */
    pgraft_go_replicate_log_entry_ptr = (pgraft_go_replicate_log_entry_func) dlsym(go_lib_handle, "pgraft_go_replicate_log_entry");
    pgraft_go_get_replication_status_ptr = (pgraft_go_get_replication_status_func) dlsym(go_lib_handle, "pgraft_go_get_replication_status");
    pgraft_go_create_snapshot_ptr = (pgraft_go_create_snapshot_func) dlsym(go_lib_handle, "pgraft_go_create_snapshot");
    pgraft_go_apply_snapshot_ptr = (pgraft_go_apply_snapshot_func) dlsym(go_lib_handle, "pgraft_go_apply_snapshot");
    pgraft_go_replicate_to_node_ptr = (pgraft_go_replicate_to_node_func) dlsym(go_lib_handle, "pgraft_go_replicate_to_node");
    pgraft_go_get_replication_lag_ptr = (pgraft_go_get_replication_lag_func) dlsym(go_lib_handle, "pgraft_go_get_replication_lag");
    pgraft_go_sync_replication_ptr = (pgraft_go_sync_replication_func) dlsym(go_lib_handle, "pgraft_go_sync_replication");
    
    /* Check if all critical functions were loaded */
    if (!pgraft_go_init_ptr || !pgraft_go_start_ptr || !pgraft_go_stop_ptr)
    {
        elog(ERROR, "pgraft_raft: failed to load critical Go functions");
        dlclose(go_lib_handle);
        go_lib_handle = NULL;
        return -1;
    }
    
    go_lib_loaded = true;
    elog(INFO, "pgraft_raft: Go Raft library loaded successfully");
    return 0;
}

/*
 * Unload Go Raft library
 */
static void
unload_go_library(void)
{
    if (go_lib_handle)
    {
        dlclose(go_lib_handle);
        go_lib_handle = NULL;
    }
    
    /* Reset function pointers */
    pgraft_go_init_ptr = NULL;
    pgraft_go_start_ptr = NULL;
    pgraft_go_stop_ptr = NULL;
    pgraft_go_add_peer_ptr = NULL;
    pgraft_go_remove_peer_ptr = NULL;
    pgraft_go_get_state_ptr = NULL;
    pgraft_go_get_leader_ptr = NULL;
    pgraft_go_get_term_ptr = NULL;
    pgraft_go_append_log_ptr = NULL;
    pgraft_go_get_stats_ptr = NULL;
    pgraft_go_get_nodes_ptr = NULL;
    pgraft_go_get_logs_ptr = NULL;
    pgraft_go_commit_log_ptr = NULL;
    pgraft_go_step_message_ptr = NULL;
    pgraft_go_get_network_status_ptr = NULL;
    pgraft_go_free_string_ptr = NULL;
    
    go_lib_loaded = false;
    elog(INFO, "pgraft_raft: Go Raft library unloaded");
}

void
pgraft_raft_init(void)
{
    memset(&raft_state, 0, sizeof(raft_state));
    
    /* Initialize state */
    raft_state.state = PGRAFT_STATE_FOLLOWER;
    raft_state.current_term = 0;
    raft_state.voted_for = -1;
    raft_state.last_log_index = 0;
    raft_state.last_log_term = 0;
    raft_state.commit_index = 0;
    raft_state.last_applied = 0;
    raft_state.leader_id = -1;
    raft_state.last_heartbeat = GetCurrentTimestamp();
    raft_state.is_initialized = true;
    
    /* Load Go Raft library */
    if (load_go_library() != 0)
    {
        elog(ERROR, "pgraft_raft: failed to load Go Raft library");
        return;
    }
    
    elog(INFO, "pgraft_raft: Raft state initialized with etcd-io/raft");
}

void
pgraft_raft_start(void)
{
    int result;
    
    if (!go_lib_loaded || !pgraft_go_start_ptr)
    {
        elog(ERROR, "pgraft_raft: Go Raft library not loaded");
        return;
    }
    
    /* Start Go Raft system */
    result = pgraft_go_start_ptr();
    if (result != 0)
    {
        elog(ERROR, "pgraft_raft: failed to start Go Raft system: %d", result);
        return;
    }
    
    raft_state.is_running = true;
    elog(INFO, "pgraft_raft: Raft system started with etcd-io/raft");
}

void
pgraft_raft_stop(void)
{
    int result;
    
    if (!go_lib_loaded || !pgraft_go_stop_ptr)
    {
        elog(WARNING, "pgraft_raft: Go Raft library not loaded");
        return;
    }
    
    /* Stop Go Raft system */
    result = pgraft_go_stop_ptr();
    if (result != 0)
    {
        elog(WARNING, "pgraft_raft: failed to stop Go Raft system: %d", result);
    }
    
    raft_state.is_running = false;
    elog(INFO, "pgraft_raft: Raft system stopped");
}

void
pgraft_raft_cleanup(void)
{
    /* Stop the system first */
    pgraft_raft_stop();
    
    /* Unload Go library */
    unload_go_library();
    
    /* Reset state */
    memset(&raft_state, 0, sizeof(raft_state));
    
    elog(INFO, "pgraft_raft: Raft system cleaned up");
}

int
pgraft_raft_add_node(uint64_t node_id, const char *address, int port)
{
    int result;
    
    if (!go_lib_loaded || !pgraft_go_add_peer_ptr)
    {
        elog(ERROR, "pgraft_raft: Go Raft library not loaded");
        return -1;
    }
    
    /* Add peer to Go Raft system */
    result = pgraft_go_add_peer_ptr((int)node_id, (char*)address, port);
    if (result != 0)
    {
        elog(ERROR, "pgraft_raft: failed to add node %llu: %d", (unsigned long long)node_id, result);
        return -1;
    }
    
    elog(INFO, "pgraft_raft: added node %llu at %s:%d", (unsigned long long)node_id, address, port);
    return 0;
}

int
pgraft_raft_remove_node(uint64_t node_id)
{
    int result;
    
    if (!go_lib_loaded || !pgraft_go_remove_peer_ptr)
    {
        elog(ERROR, "pgraft_raft: Go Raft library not loaded");
        return -1;
    }
    
    /* Remove peer from Go Raft system */
    result = pgraft_go_remove_peer_ptr((int)node_id);
    if (result != 0)
    {
        elog(ERROR, "pgraft_raft: failed to remove node %llu: %d", (unsigned long long)node_id, result);
        return -1;
    }
    
    elog(INFO, "pgraft_raft: removed node %llu", (unsigned long long)node_id);
    return 0;
}

pgraft_raft_state_t*
pgraft_raft_get_state(void)
{
    char *state_str;
    int leader_id;
    long term;
    
    if (!go_lib_loaded || !pgraft_go_get_state_ptr || !pgraft_go_get_leader_ptr || !pgraft_go_get_term_ptr)
    {
        return &raft_state;
    }
    
    /* Get state from Go Raft system */
    state_str = pgraft_go_get_state_ptr();
    if (state_str)
    {
        if (strcmp(state_str, "leader") == 0)
            raft_state.state = PGRAFT_STATE_LEADER;
        else if (strcmp(state_str, "candidate") == 0)
            raft_state.state = PGRAFT_STATE_CANDIDATE;
        else if (strcmp(state_str, "follower") == 0)
            raft_state.state = PGRAFT_STATE_FOLLOWER;
        else
            raft_state.state = PGRAFT_STATE_UNKNOWN;
        
        pgraft_go_free_string_ptr(state_str);
    }
    
    /* Get leader ID */
    leader_id = pgraft_go_get_leader_ptr();
    if (leader_id >= 0)
        raft_state.leader_id = leader_id;
    
    /* Get current term */
    term = pgraft_go_get_term_ptr();
    if (term >= 0)
        raft_state.current_term = term;
    
    return &raft_state;
}

int
pgraft_raft_append_log(const char *data, size_t data_len)
{
    int result;
    
    if (!go_lib_loaded || !pgraft_go_append_log_ptr)
    {
        elog(ERROR, "pgraft_raft: Go Raft library not loaded");
        return -1;
    }
    
    /* Append log entry to Go Raft system */
    result = pgraft_go_append_log_ptr((char*)data, (int)data_len);
    if (result != 0)
    {
        elog(ERROR, "pgraft_raft: failed to append log entry: %d", result);
        return -1;
    }
    
    return 0;
}

int
pgraft_raft_commit_log(long index)
{
    int result;
    
    if (!go_lib_loaded || !pgraft_go_commit_log_ptr)
    {
        elog(ERROR, "pgraft_raft: Go Raft library not loaded");
        return -1;
    }
    
    /* Commit log entry in Go Raft system */
    result = pgraft_go_commit_log_ptr(index);
    if (result != 0)
    {
        elog(ERROR, "pgraft_raft: failed to commit log entry: %d", result);
        return -1;
    }
    
    return 0;
}

int
pgraft_raft_step_message(const char *data, size_t data_len)
{
    int result;
    
    if (!go_lib_loaded || !pgraft_go_step_message_ptr)
    {
        elog(ERROR, "pgraft_raft: Go Raft library not loaded");
        return -1;
    }
    
    /* Step message in Go Raft system */
    result = pgraft_go_step_message_ptr((char*)data, (int)data_len);
    if (result != 0)
    {
        elog(ERROR, "pgraft_raft: failed to step message: %d", result);
        return -1;
    }
    
    return 0;
}

char*
pgraft_raft_get_stats(void)
{
    if (!go_lib_loaded || !pgraft_go_get_stats_ptr)
    {
        return NULL;
    }
    
    return pgraft_go_get_stats_ptr();
}

char*
pgraft_raft_get_nodes(void)
{
    if (!go_lib_loaded || !pgraft_go_get_nodes_ptr)
    {
        return NULL;
    }
    
    return pgraft_go_get_nodes_ptr();
}

char*
pgraft_raft_get_logs(void)
{
    if (!go_lib_loaded || !pgraft_go_get_logs_ptr)
    {
        return NULL;
    }
    
    return pgraft_go_get_logs_ptr();
}

char*
pgraft_raft_get_network_status(void)
{
    if (!go_lib_loaded || !pgraft_go_get_network_status_ptr)
    {
        return NULL;
    }
    
    return pgraft_go_get_network_status_ptr();
}

void
pgraft_raft_free_string(char *str)
{
    if (go_lib_loaded && pgraft_go_free_string_ptr && str)
    {
        pgraft_go_free_string_ptr(str);
    }
}

/* ============================================================================
 * REPLICATION FUNCTIONS - C wrappers for Go etcd-io/raft replication
 * ============================================================================ */

/*
 * Replicate log entry to all nodes in the cluster
 */
int
pgraft_replicate_log_entry(const char *data, size_t data_len)
{
    if (!go_lib_loaded || !pgraft_go_replicate_log_entry_ptr)
    {
        elog(ERROR, "pgraft_replication: Go library not loaded");
        return 0;
    }
    
    if (!data || data_len == 0)
    {
        elog(ERROR, "pgraft_replication: invalid data parameters");
        return 0;
    }
    
    return pgraft_go_replicate_log_entry_ptr((char *)data, (int)data_len);
}

/*
 * Get replication status information
 */
char *
pgraft_get_replication_info(void)
{
    if (!go_lib_loaded || !pgraft_go_get_replication_status_ptr)
    {
        elog(ERROR, "pgraft_replication: Go library not loaded");
        return NULL;
    }
    
    return pgraft_go_get_replication_status_ptr();
}

/*
 * Create a snapshot for replication
 */
char *
pgraft_create_replication_snapshot(void)
{
    if (!go_lib_loaded || !pgraft_go_create_snapshot_ptr)
    {
        elog(ERROR, "pgraft_replication: Go library not loaded");
        return NULL;
    }
    
    return pgraft_go_create_snapshot_ptr();
}

/*
 * Apply a snapshot for replication
 */
int
pgraft_apply_replication_snapshot(const char *snapshot_data)
{
    if (!go_lib_loaded || !pgraft_go_apply_snapshot_ptr)
    {
        elog(ERROR, "pgraft_replication: Go library not loaded");
        return 0;
    }
    
    if (!snapshot_data)
    {
        elog(ERROR, "pgraft_replication: invalid snapshot data");
        return 0;
    }
    
    return pgraft_go_apply_snapshot_ptr((char *)snapshot_data);
}

/*
 * Replicate data to a specific node
 */
int
pgraft_replicate_to_node(uint64_t node_id, const char *data, size_t data_len)
{
    if (!go_lib_loaded || !pgraft_go_replicate_to_node_ptr)
    {
        elog(ERROR, "pgraft_replication: Go library not loaded");
        return 0;
    }
    
    if (!data || data_len == 0)
    {
        elog(ERROR, "pgraft_replication: invalid data parameters");
        return 0;
    }
    
    return pgraft_go_replicate_to_node_ptr((uint64_t)node_id, (char *)data, (int)data_len);
}

/*
 * Get replication lag in milliseconds
 */
double
pgraft_get_replication_lag(void)
{
    if (!go_lib_loaded || !pgraft_go_get_replication_lag_ptr)
    {
        elog(ERROR, "pgraft_replication: Go library not loaded");
        return 0.0;
    }
    
    return pgraft_go_get_replication_lag_ptr();
}

/*
 * Force synchronization of replication
 */
int
pgraft_sync_replication(void)
{
    if (!go_lib_loaded || !pgraft_go_sync_replication_ptr)
    {
        elog(ERROR, "pgraft_replication: Go library not loaded");
        return 0;
    }
    
    return pgraft_go_sync_replication_ptr();
}

/*
 * Free string returned by replication functions
 */
void
pgraft_free_replication_string(char *str)
{
    if (go_lib_loaded && pgraft_go_free_string_ptr && str)
    {
        pgraft_go_free_string_ptr(str);
    }
}