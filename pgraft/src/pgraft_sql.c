/*-------------------------------------------------------------------------
 *
 * pgraft_sql.c
 *      SQL interface functions for pgraft
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include "utils/builtins.h"

#include "../include/pgraft_sql.h"
#include "../include/pgraft_core.h"
#include "../include/pgraft_go.h"
#include "../include/pgraft_state.h"
#include "../include/pgraft_log.h"
#include "../include/pgraft_guc.h"

/* Function info macros for core functions */
PG_FUNCTION_INFO_V1(pgraft_init);
PG_FUNCTION_INFO_V1(pgraft_init_guc);
PG_FUNCTION_INFO_V1(pgraft_start);
PG_FUNCTION_INFO_V1(pgraft_add_node);
PG_FUNCTION_INFO_V1(pgraft_remove_node);
PG_FUNCTION_INFO_V1(pgraft_get_cluster_status);
PG_FUNCTION_INFO_V1(pgraft_get_leader);
PG_FUNCTION_INFO_V1(pgraft_get_term);
PG_FUNCTION_INFO_V1(pgraft_is_leader);
PG_FUNCTION_INFO_V1(pgraft_get_nodes);
PG_FUNCTION_INFO_V1(pgraft_get_version);
PG_FUNCTION_INFO_V1(pgraft_test);
PG_FUNCTION_INFO_V1(pgraft_set_debug);

/* Function info macros for log functions */
PG_FUNCTION_INFO_V1(pgraft_log_append);
PG_FUNCTION_INFO_V1(pgraft_log_commit);
PG_FUNCTION_INFO_V1(pgraft_log_apply);
PG_FUNCTION_INFO_V1(pgraft_log_get_entry_sql);
PG_FUNCTION_INFO_V1(pgraft_log_get_stats);
PG_FUNCTION_INFO_V1(pgraft_log_get_replication_status_sql);
PG_FUNCTION_INFO_V1(pgraft_log_sync_with_leader_sql);

/*
 * Initialize pgraft node
 */
Datum
pgraft_init_guc(PG_FUNCTION_ARGS)
{
	int32_t		node_id;
	int32_t		port;
	char	   *address;
	char	   *cluster_id;
	pgraft_go_init_func init_func;
	
	/* Get configuration from GUC variables */
	node_id = pgraft_node_id;
	port = pgraft_port;
	address = pgraft_address;
	cluster_id = pgraft_cluster_name;
	
	elog(INFO, "pgraft: Initializing node %d at %s:%d (cluster: %s)", 
		 node_id, address, port, cluster_id);
	
	/* Initialize core system */
	if (pgraft_core_init(node_id, address, port) != 0)
	{
		elog(ERROR, "pgraft: Failed to initialize core system");
		PG_RETURN_BOOL(false);
	}
	
	/* Load and initialize Go library */
	if (pgraft_go_load_library() != 0)
	{
		elog(ERROR, "pgraft: Failed to load Go library");
		PG_RETURN_BOOL(false);
	}
	
	init_func = pgraft_go_get_init_func();
	if (!init_func)
	{
		elog(ERROR, "pgraft: Failed to get Go init function");
		PG_RETURN_BOOL(false);
	}
	
	/* Initialize Go Raft library */
	if (init_func(node_id, address, port) != 0)
	{
		elog(ERROR, "pgraft: Failed to initialize Go Raft library");
		PG_RETURN_BOOL(false);
	}
	
	elog(INFO, "pgraft: Node %d initialized successfully", node_id);
	PG_RETURN_BOOL(true);
}

/*
 * Start pgraft consensus process
 */
Datum
pgraft_start(PG_FUNCTION_ARGS)
{
	pgraft_go_start_func start_func;
	
	elog(INFO, "pgraft: Starting consensus process");
	
	/* Check if Go library is loaded */
	if (!pgraft_go_is_loaded())
	{
		elog(ERROR, "pgraft: Go library not loaded - call pgraft_init() first");
		PG_RETURN_BOOL(false);
	}
	
	/* Get start function */
	start_func = pgraft_go_get_start_func();
	if (!start_func)
	{
		elog(ERROR, "pgraft: Failed to get Go start function");
		PG_RETURN_BOOL(false);
	}
	
	/* Start the Raft consensus process */
	if (start_func() != 0)
	{
		elog(ERROR, "pgraft: Failed to start Raft consensus process");
		PG_RETURN_BOOL(false);
	}
	
	elog(INFO, "pgraft: Consensus process started successfully");
	PG_RETURN_BOOL(true);
}

Datum
pgraft_init(PG_FUNCTION_ARGS)
{
	int32_t		node_id;
	text	   *address_text;
	int32_t		port;
	char	   *address;
	pgraft_go_init_func init_func;
	
	node_id = PG_GETARG_INT32(0);
	address_text = PG_GETARG_TEXT_PP(1);
	port = PG_GETARG_INT32(2);
	
	address = text_to_cstring(address_text);
	
	elog(INFO, "pgraft: Initializing node %d at %s:%d", node_id, address, port);
	
	/* Initialize core system */
	if (pgraft_core_init(node_id, address, port) != 0)
	{
		elog(ERROR, "pgraft: Failed to initialize core system");
		PG_RETURN_BOOL(false);
	}
	elog(INFO, "pgraft: Core system initialized successfully");
	
	/* Load Go library dynamically (not in _PG_init) */
	if (pgraft_go_load_library() != 0)
	{
		elog(ERROR, "pgraft: Failed to load Go library");
		PG_RETURN_BOOL(false);
	}
	elog(INFO, "pgraft: Go library loaded successfully");
	
	/* Initialize Go library */
	init_func = pgraft_go_get_init_func();
	if (init_func && init_func(node_id, address, port) != 0)
	{
		elog(ERROR, "pgraft: Failed to initialize Go library");
		PG_RETURN_BOOL(false);
	}
	elog(INFO, "pgraft: Go library initialized successfully");
	
	/* Save state to shared memory */
	pgraft_state_save_node_config(node_id, address, port);
	pgraft_state_save_go_library_state();
	pgraft_state_set_go_initialized(true);
	elog(INFO, "pgraft: State saved to shared memory");
	
	elog(INFO, "pgraft: Node initialization completed successfully");
	PG_RETURN_BOOL(true);
}

/*
 * Add node to cluster
 */
Datum
pgraft_add_node(PG_FUNCTION_ARGS)
{
	int32_t		node_id;
	text	   *address_text;
	int32_t		port;
	char	   *address;
	pgraft_go_add_peer_func add_peer_func;
	
	node_id = PG_GETARG_INT32(0);
	address_text = PG_GETARG_TEXT_PP(1);
	port = PG_GETARG_INT32(2);
	
	address = text_to_cstring(address_text);
	
	elog(INFO, "pgraft: Adding node %d at %s:%d", node_id, address, port);
	
	/* Add to core system */
	if (pgraft_core_add_node(node_id, address, port) != 0)
	{
		elog(ERROR, "pgraft: Failed to add node %d to core system - core system not initialized or cluster not available", node_id);
		PG_RETURN_BOOL(false);
	}
	elog(INFO, "pgraft: Node added to core system successfully");
	
	/* Add to Go library if loaded */
	if (pgraft_go_is_loaded())
	{
		add_peer_func = pgraft_go_get_add_peer_func();
		if (add_peer_func && add_peer_func(node_id, address, port) != 0)
		{
			elog(ERROR, "pgraft: Failed to add node to Go library");
			PG_RETURN_BOOL(false);
		}
		elog(INFO, "pgraft: Node added to Go library successfully");
	}
	else
	{
		elog(WARNING, "pgraft: Go library not loaded, skipping Go library addition");
	}
	
	elog(INFO, "pgraft: Node %d added successfully to cluster", node_id);
	PG_RETURN_BOOL(true);
}

/*
 * Remove node from cluster
 */
Datum
pgraft_remove_node(PG_FUNCTION_ARGS)
{
    int32_t node_id = PG_GETARG_INT32(0);
    
    elog(INFO, "pgraft: Removing node %d", node_id);
    
    /* Remove from core system */
    if (pgraft_core_remove_node(node_id) != 0) {
        elog(ERROR, "pgraft: Failed to remove node from core system");
        PG_RETURN_BOOL(false);
    }
    
    /* Remove from Go library if loaded */
    if (pgraft_go_is_loaded()) {
        pgraft_go_remove_peer_func remove_peer_func = pgraft_go_get_remove_peer_func();
        if (remove_peer_func && remove_peer_func(node_id) != 0) {
            elog(ERROR, "pgraft: Failed to remove node from Go library");
            PG_RETURN_BOOL(false);
        }
    }
    
    elog(INFO, "pgraft: Node removed successfully");
    PG_RETURN_BOOL(true);
}

/*
 * Get cluster status
 */
Datum
pgraft_get_cluster_status(PG_FUNCTION_ARGS)
{
    pgraft_cluster_t cluster;
    StringInfoData result;
    
    if (pgraft_core_get_cluster_state(&cluster) != 0) {
        elog(ERROR, "pgraft: Failed to get cluster state");
        PG_RETURN_NULL();
    }
    
    /* Create result text */
    initStringInfo(&result);
    
    appendStringInfo(&result, "Node ID: %d\n", cluster.node_id);
    appendStringInfo(&result, "Current Term: %d\n", cluster.current_term);
    appendStringInfo(&result, "Leader ID: %lld\n", cluster.leader_id);
    appendStringInfo(&result, "State: %s\n", cluster.state);
    appendStringInfo(&result, "Number of Nodes: %d\n", cluster.num_nodes);
    appendStringInfo(&result, "Messages Processed: %lld\n", cluster.messages_processed);
    appendStringInfo(&result, "Heartbeats Sent: %lld\n", cluster.heartbeats_sent);
    appendStringInfo(&result, "Elections Triggered: %lld\n", cluster.elections_triggered);
    
    PG_RETURN_TEXT_P(cstring_to_text(result.data));
}

/*
 * Get current leader
 */
Datum
pgraft_get_leader(PG_FUNCTION_ARGS)
{
    int64_t leader_id = pgraft_core_get_leader_id();
    PG_RETURN_INT64(leader_id);
}

/*
 * Get current term
 */
Datum
pgraft_get_term(PG_FUNCTION_ARGS)
{
    int32_t term = pgraft_core_get_current_term();
    PG_RETURN_INT32(term);
}

/*
 * Check if current node is leader
 */
Datum
pgraft_is_leader(PG_FUNCTION_ARGS)
{
    bool is_leader = pgraft_core_is_leader();
    PG_RETURN_BOOL(is_leader);
}

/*
 * Get cluster nodes
 */
Datum
pgraft_get_nodes(PG_FUNCTION_ARGS)
{
    pgraft_cluster_t cluster;
    StringInfoData result;
    
    if (pgraft_core_get_cluster_state(&cluster) != 0) {
        elog(ERROR, "pgraft: Failed to get cluster state");
        PG_RETURN_NULL();
    }
    
    initStringInfo(&result);
    
    for (int i = 0; i < cluster.num_nodes; i++) {
        appendStringInfo(&result, "Node %d: %s:%d%s\n", 
                        cluster.nodes[i].id,
                        cluster.nodes[i].address,
                        cluster.nodes[i].port,
                        cluster.nodes[i].is_leader ? " (leader)" : "");
    }
    
    PG_RETURN_TEXT_P(cstring_to_text(result.data));
}

/*
 * Get pgraft version
 */
Datum
pgraft_get_version(PG_FUNCTION_ARGS)
{
    if (pgraft_go_is_loaded()) {
        pgraft_go_version_func version_func = pgraft_go_get_version_func();
        if (version_func) {
            char *version = version_func();
            if (version) {
                text *result = cstring_to_text(version);
                pgraft_go_free_string_func free_func = pgraft_go_get_free_string_func();
                if (free_func) {
                    free_func(version);
                }
                PG_RETURN_TEXT_P(result);
            }
        }
    }
    
    PG_RETURN_TEXT_P(cstring_to_text("pgraft-1.0.0"));
}

/*
 * Test pgraft functionality
 */
Datum
pgraft_test(PG_FUNCTION_ARGS)
{
    if (pgraft_go_is_loaded()) {
        pgraft_go_test_func test_func = pgraft_go_get_test_func();
        if (test_func && test_func() == 0) {
            PG_RETURN_BOOL(true);
        }
    }
    
    PG_RETURN_BOOL(false);
}

/*
 * Set debug mode
 */
Datum
pgraft_set_debug(PG_FUNCTION_ARGS)
{
    bool debug_enabled = PG_GETARG_BOOL(0);
    
    if (pgraft_go_is_loaded()) {
        pgraft_go_set_debug_func set_debug_func = pgraft_go_get_set_debug_func();
        if (set_debug_func) {
            set_debug_func(debug_enabled ? 1 : 0);
        }
    }
    
    elog(INFO, "pgraft: Debug mode %s", debug_enabled ? "enabled" : "disabled");
    PG_RETURN_BOOL(true);
}

/*
 * Log replication functions
 */

/*
 * Append log entry
 */
Datum
pgraft_log_append(PG_FUNCTION_ARGS)
{
    int64_t term = PG_GETARG_INT64(0);
    text *data_text = PG_GETARG_TEXT_PP(1);
    
    char *data = text_to_cstring(data_text);
    int32_t data_size = strlen(data);
    
    if (pgraft_log_append_entry(term, data, data_size) != 0) {
        elog(ERROR, "pgraft: Failed to append log entry");
        PG_RETURN_BOOL(false);
    }
    
    elog(DEBUG1, "pgraft: Appended log entry with term %lld", term);
    PG_RETURN_BOOL(true);
}

/*
 * Commit log entry
 */
Datum
pgraft_log_commit(PG_FUNCTION_ARGS)
{
    int64_t index = PG_GETARG_INT64(0);
    
    if (pgraft_log_commit_entry(index) != 0) {
        elog(ERROR, "pgraft: Failed to commit log entry %lld", index);
        PG_RETURN_BOOL(false);
    }
    
    elog(DEBUG1, "pgraft: Committed log entry %lld", index);
    PG_RETURN_BOOL(true);
}

/*
 * Apply log entry
 */
Datum
pgraft_log_apply(PG_FUNCTION_ARGS)
{
    int64_t index = PG_GETARG_INT64(0);
    
    if (pgraft_log_apply_entry(index) != 0) {
        elog(ERROR, "pgraft: Failed to apply log entry %lld", index);
        PG_RETURN_BOOL(false);
    }
    
    elog(DEBUG1, "pgraft: Applied log entry %lld", index);
    PG_RETURN_BOOL(true);
}

/*
 * Get log entry
 */
Datum
pgraft_log_get_entry_sql(PG_FUNCTION_ARGS)
{
    int64_t index = PG_GETARG_INT64(0);
    pgraft_log_entry_t entry;
    StringInfoData result;
    
    if (pgraft_log_get_entry(index, &entry) != 0) {
        elog(ERROR, "pgraft: Failed to get log entry %lld", index);
        PG_RETURN_NULL();
    }
    initStringInfo(&result);
    
    appendStringInfo(&result, "Index: %lld, Term: %lld, Timestamp: %lld, Data: %s, Committed: %s, Applied: %s",
                    entry.index, entry.term, entry.timestamp, entry.data,
                    entry.committed ? "yes" : "no", entry.applied ? "yes" : "no");
    
    PG_RETURN_TEXT_P(cstring_to_text(result.data));
}

/*
 * Get log statistics
 */
Datum
pgraft_log_get_stats(PG_FUNCTION_ARGS)
{
    pgraft_log_state_t stats;
    StringInfoData result;
    
    if (pgraft_log_get_statistics(&stats) != 0) {
        elog(ERROR, "pgraft: Failed to get log statistics");
        PG_RETURN_NULL();
    }
    initStringInfo(&result);
    
    appendStringInfo(&result, "Log Size: %d, Last Index: %lld, Commit Index: %lld, Last Applied: %lld, "
                    "Replicated: %lld, Committed: %lld, Applied: %lld, Errors: %lld",
                    stats.log_size, stats.last_index, stats.commit_index, stats.last_applied,
                    stats.entries_replicated, stats.entries_committed, 
                    stats.entries_applied, stats.replication_errors);
    
    PG_RETURN_TEXT_P(cstring_to_text(result.data));
}

/*
 * Get replication status
 */
Datum
pgraft_log_get_replication_status_sql(PG_FUNCTION_ARGS)
{
    char status[1024];
    
    if (pgraft_log_get_replication_status(status, sizeof(status)) != 0) {
        elog(ERROR, "pgraft: Failed to get replication status");
        PG_RETURN_NULL();
    }
    
    PG_RETURN_TEXT_P(cstring_to_text(status));
}

/*
 * Sync with leader
 */
Datum
pgraft_log_sync_with_leader_sql(PG_FUNCTION_ARGS)
{
    if (pgraft_log_sync_with_leader() != 0) {
        elog(ERROR, "pgraft: Failed to sync with leader");
        PG_RETURN_BOOL(false);
    }
    
    elog(INFO, "pgraft: Synced with leader successfully");
    PG_RETURN_BOOL(true);
}
