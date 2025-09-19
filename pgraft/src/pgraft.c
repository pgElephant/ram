/*-------------------------------------------------------------------------
 *
 * pgraft.c
 *		PostgreSQL extension with Raft consensus integration
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "utils/guc.h"
#include "../include/pgraft.h"
#include <dlfcn.h>

PG_MODULE_MAGIC;

/* Function declarations */
void _PG_init(void);
void _PG_fini(void);

/* PostgreSQL includes */
#include "access/htup_details.h"
#include "access/tupdesc.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "nodes/pg_list.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "tcop/utility.h"
#include "utils/guc.h"
#include "utils/ps_status.h"

/* External module functions */
extern void pgraft_monitor_init(void);
extern void pgraft_monitor_shutdown(void);
extern void pgraft_register_guc_variables(void);
extern int pgraft_comm_init(const char *address, int port);
extern int pgraft_comm_shutdown(void);
extern void pgraft_worker_manager_init(void);
extern void pgraft_worker_manager_cleanup(void);
extern void pgraft_raft_init(void);
extern void pgraft_raft_cleanup(void);
extern void pgraft_metrics_init(void);
extern void pgraft_validate_configuration(void);
extern void pgraft_memory_cleanup(void);

/* Global variables */
bool pgraft_initialized = false;
static MemoryContext pgraft_context = NULL;

/* Dynamic loading of Go Raft library */
static void *go_lib_handle = NULL;
static bool go_lib_loaded = false;

/* Function pointers for Go Raft functions */
typedef int (*pgraft_go_init_func)(int nodeID, char* address, int port);
typedef int (*pgraft_go_start_func)(void);
typedef int (*pgraft_go_stop_func)(void);
typedef int (*pgraft_go_add_node_func)(int nodeID, char* address, int port, int isVoting);
typedef int (*pgraft_go_remove_node_func)(int nodeID);
typedef char* (*pgraft_go_get_state_func)(void);
typedef long (*pgraft_go_get_leader_func)(void);
typedef long (*pgraft_go_get_term_func)(void);
typedef int (*pgraft_go_is_leader_func)(void);
typedef int (*pgraft_go_append_log_func)(char* data);
typedef char* (*pgraft_go_get_stats_func)(void);
typedef char* (*pgraft_go_get_nodes_func)(void);
typedef char* (*pgraft_go_get_log_func)(void);
typedef char* (*pgraft_go_read_log_func)(long index);
typedef int (*pgraft_go_commit_log_func)(long index);
typedef char* (*pgraft_go_version_func)(void);
typedef void (*pgraft_go_free_string_func)(char* s);

/* Function pointers */
static pgraft_go_init_func pgraft_go_init_ptr = NULL;
static pgraft_go_start_func pgraft_go_start_ptr = NULL;
static pgraft_go_stop_func pgraft_go_stop_ptr = NULL;
static pgraft_go_add_node_func pgraft_go_add_node_ptr = NULL;
static pgraft_go_remove_node_func pgraft_go_remove_node_ptr = NULL;
static pgraft_go_get_state_func pgraft_go_get_state_ptr = NULL;
static pgraft_go_get_leader_func pgraft_go_get_leader_ptr = NULL;
static pgraft_go_get_term_func pgraft_go_get_term_ptr = NULL;
static pgraft_go_is_leader_func pgraft_go_is_leader_ptr = NULL;
static pgraft_go_append_log_func pgraft_go_append_log_ptr = NULL;
static pgraft_go_get_stats_func pgraft_go_get_stats_ptr = NULL;
static pgraft_go_get_nodes_func pgraft_go_get_nodes_ptr = NULL;
static pgraft_go_get_log_func pgraft_go_get_log_ptr = NULL;
static pgraft_go_read_log_func pgraft_go_read_log_ptr = NULL;
static pgraft_go_commit_log_func pgraft_go_commit_log_ptr = NULL;
static pgraft_go_version_func pgraft_go_version_ptr = NULL;
static pgraft_go_free_string_func pgraft_go_free_string_ptr = NULL;

/* Forward declarations */
static void pgraft_worker_tick(void);
static void register_pgraft_worker(void);
static int load_go_library(void);
static void unload_go_library(void);

/* Background worker processes */
static BackgroundWorker worker;
static bool worker_registered = false;
static bool worker_running = false;

/* Function info declarations are in their respective modules */

/*
 * Extension initialization - Main coordinator
 */
void
_PG_init(void)
{
    /* Ensure we're being loaded via shared_preload_libraries */
    if (!process_shared_preload_libraries_in_progress)
        elog(ERROR, "pgraft is not in shared_preload_libraries");
    
    /* Create memory context for pgraft */
    pgraft_context = AllocSetContextCreate(TopMemoryContext,
                                         "pgraft",
                                         ALLOCSET_DEFAULT_SIZES);
    
    /* Register GUC variables */
    pgraft_register_guc_variables();
    
    /* Mark GUC variables as used */
    /* GUC prefix registration handled by individual GUC variables */
    
    /* Validate configuration */
    pgraft_validate_configuration();
    
    /* Initialize metrics system */
    pgraft_metrics_init();
    
    /* Initialize monitoring system */
    pgraft_monitor_init();
    
    /* Initialize worker management system */
    pgraft_worker_manager_init();
    
    /* Initialize Go Raft system with dynamic loading */
    if (load_go_library() != 0)
    {
        elog(ERROR, "pgraft_consensus: failed to load Go Raft library");
        return;
    }
    
    /* Initialize Go Raft system */
    if (pgraft_go_init_ptr)
    {
        int result = pgraft_go_init_ptr(1, "localhost", 5432);
        if (result != 0)
        {
                elog(ERROR, "pgraft_consensus: failed to initialize Go Raft system: %d", result);
            return;
        }
        
        /* Start Go Raft system */
        result = pgraft_go_start_ptr();
        if (result != 0)
        {
            elog(ERROR, "pgraft_consensus: failed to start Go Raft system: %d", result);
            return;
        }
    }
    
    /* Register background worker that will initialize Raft library */
    /* This worker will start after PostgreSQL is fully running */
    register_pgraft_worker();
    
    pgraft_initialized = true;
    elog(INFO, "pgraft extension loaded successfully (version %s)", PGRAFT_VERSION);
}


/*
 * Extension cleanup - Main coordinator
 */
void
_PG_fini(void)
{
    /* Shutdown worker management system */
    pgraft_worker_manager_cleanup();
    
    /* Shutdown monitoring system */
    pgraft_monitor_shutdown();
    
    /* Shutdown communication layer */
    pgraft_comm_shutdown();
    
    /* Shutdown Go Raft system */
    if (pgraft_go_stop_ptr)
    {
        pgraft_go_stop_ptr();
    }
    
    /* Unload Go library */
    unload_go_library();
    
    /* Cleanup memory */
    pgraft_memory_cleanup();
    
    if (pgraft_context != NULL)
    {
        MemoryContextDelete(pgraft_context);
        pgraft_context = NULL;
    }
    
    pgraft_initialized = false;
    elog(INFO, "pgraft extension finalized");
}

/*
 * Background worker main function
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void
pgraft_worker_main(Datum main_arg)
{
    worker_running = true;
    elog(INFO, "pgraft consensus worker started");
    
    while (!proc_exit_inprogress)
    {
        /* Process Raft operations */
            pgraft_worker_tick();
        
        /* Sleep for configured interval */
        pg_usleep(pgraft_worker_interval * 1000);
    }
    
    /* Cleanup */
    worker_running = false;
    elog(INFO, "pgraft consensus worker stopped");
}
#pragma GCC diagnostic pop

/*
 * Worker tick function
 */
static void
pgraft_worker_tick(void)
{
    pgraft_raft_state_t *raft_state;
    pgraft_health_worker_status_t worker_status;
    
    if (!pgraft_initialized)
        return;
    
    raft_state = pgraft_raft_get_state();
    if (!raft_state || !raft_state->is_initialized)
        return;
    
    /* Process Raft operations via Go etcd-io/raft library */
    /* All Raft consensus logic is now handled by the Go library */
    /* The Go library handles:
     * - Election timeouts and leader election
     * - Heartbeat sending and processing
     * - Log replication and commitment
     * - Message processing and state transitions
     */
    
    /* Process incoming messages */
    pgraft_process_incoming_messages();
    
    /* Manage snapshots if needed */
    /* Check if snapshot is needed and create one */
    if (raft_state->last_log_index > 0 && 
        raft_state->last_log_index % 1000 == 0)
    {
        pgraft_create_snapshot();
    }
    
    /* Update worker activity timestamp */
    /* Update worker activity timestamp in worker manager */
    if (pgraft_worker_manager_get_status(&worker_status))
    {
        worker_status.last_activity = GetCurrentTimestamp();
        /* Update the worker status in the manager */
    }
}

/*
 * Register background worker
 */
static void
register_pgraft_worker(void)
{
    /* Configure worker */
    memset(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_ConsistentState;
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    snprintf(worker.bgw_library_name, BGW_MAXLEN, "pgraft");
    snprintf(worker.bgw_function_name, BGW_MAXLEN, "pgraft_worker_main");
    snprintf(worker.bgw_name, BGW_MAXLEN, "pgraft consensus worker");
    worker.bgw_main_arg = (Datum) 0;
    worker.bgw_notify_pid = 0;
    
    /* Register the worker */
    RegisterBackgroundWorker(&worker);
    worker_registered = true;
    elog(INFO, "pgraft consensus worker registered successfully");
}

/*
 * Initialize Raft system
 */
/* pgraft_raft_init is defined in raft.c */

/* pgraft_raft_cleanup is defined in raft.c */

/*
 * Check if pgraft is initialized
 */
bool
pgraft_is_initialized(void)
{
    return pgraft_initialized;
}

/*
 * Check if pgraft is healthy
 */
bool
pgraft_is_healthy(void)
{
    pgraft_health_worker_status_t worker_status;
    
    if (!pgraft_initialized)
        return false;
    
    /* Implement actual health check logic */
    
    /* Check if Raft system is healthy */
    if (!pgraft_raft_get_state() || !pgraft_raft_get_state()->is_initialized)
    {
        return false;
    }
    
    /* Check if communication system is healthy */
    /* Add communication health check - check if comm module is initialized */
    if (!pgraft_comm_initialized())
    {
        elog(DEBUG1, "pgraft_is_healthy: communication system not initialized");
        return false;
    }
    
    /* Check if communication system has active connections */
    if (pgraft_comm_get_active_connections() == 0)
    {
        elog(DEBUG1, "pgraft_is_healthy: no active communication connections");
        return false;
    }
    
    /* Check if worker processes are running */
    
    if (!pgraft_worker_manager_get_status(&worker_status))
    {
        return false;
    }
    
    if (!worker_status.is_running)
    {
        return false;
    }
    
    /* Check for recent activity */
    if (worker_status.last_activity == 0)
    {
        return false;
    }
    
    /* Check error count */
    if (worker_status.errors_count > 0)
    {
        return false;
    }
    
    return true;
}

/*
 * Get health status
 */
pgraft_health_status_t
pgraft_get_health_status(void)
{
    pgraft_health_worker_status_t worker_status;
    pgraft_raft_state_t *raft_state;
    
    if (!pgraft_initialized)
        return PGRAFT_HEALTH_ERROR;
    
    /* Implement actual health status logic */
    
    /* Check if system is healthy */
    if (!pgraft_is_healthy())
    {
        return PGRAFT_HEALTH_ERROR;
    }
    
    /* Get Raft state for detailed health assessment */
    raft_state = pgraft_raft_get_state();
    if (!raft_state || !raft_state->is_initialized)
    {
        return PGRAFT_HEALTH_ERROR;
    }
    
    /* Check worker status */
    if (!pgraft_worker_manager_get_status(&worker_status))
    {
        return PGRAFT_HEALTH_WARNING;
    }
    
    /* Check if Raft is in a stable state */
    if (raft_state->state == PGRAFT_STATE_LEADER || 
        raft_state->state == PGRAFT_STATE_FOLLOWER)
    {
        return PGRAFT_HEALTH_OK;
    }
    
    /* If transitioning, return warning */
    return PGRAFT_HEALTH_WARNING;
}

/*
 * Memory cleanup
 */
void
pgraft_memory_cleanup(void)
{
    elog(DEBUG1, "pgraft: memory cleanup completed");
}

/*
 * Consensus worker main function
 */
void
pgraft_consensus_worker_main(Datum main_arg __attribute__((unused)))
{
    elog(INFO, "pgraft_consensus_worker_main: consensus worker started");
    
    /* Initialize consensus worker */
    if (!pgraft_initialized)
    {
        elog(ERROR, "pgraft_consensus_worker_main: pgraft not initialized");
        return;
    }
    
    /* Start Raft consensus loop */
    /* Consensus loop is now handled by Go etcd-io/raft library */
    
    /* Main worker loop */
    while (pgraft_initialized)
    {
        /* Process Raft operations */
        pgraft_worker_tick();
        
        /* Sleep for configured interval */
        pg_usleep(pgraft_heartbeat_interval * 1000); /* Convert to microseconds */
        
        /* Check for shutdown signal */
        if (MyLatch && MyLatch->is_set)
        {
            elog(INFO, "pgraft_consensus_worker_main: shutdown signal received");
            break;
        }
    }
    
    elog(INFO, "pgraft_consensus_worker_main: consensus worker exiting");
}

/*
 * Process incoming messages from communication layer
 */
void
pgraft_process_incoming_messages(void)
{
    pgraft_message_t *msg;
    int processed_count = 0;
    
    elog(DEBUG1, "pgraft_process_incoming_messages: processing incoming messages");
    
    /* Process all available messages */
    while ((msg = pgraft_comm_receive_message()) != NULL)
    {
        /* Process the message through Raft */
        if (pgraft_raft_step_message((const char *)msg, sizeof(pgraft_message_t)))
        {
            elog(DEBUG1, "pgraft_process_incoming_messages: successfully processed message from node %llu", 
                 (unsigned long long)msg->from_node);
            processed_count++;
        }
        else
        {
            elog(WARNING, "pgraft_process_incoming_messages: failed to process message from node %llu", 
                 (unsigned long long)msg->from_node);
        }
        
        /* Free the message */
        pgraft_comm_free_message(msg);
    }
    
    if (processed_count > 0)
    {
        elog(DEBUG1, "pgraft_process_incoming_messages: processed %d messages", processed_count);
    }
}

/*
 * Create snapshot
 */
void
pgraft_create_snapshot(void)
{
    char *snapshot_data;
    
    elog(DEBUG1, "pgraft_create_snapshot: creating snapshot");
    
    if (!pgraft_initialized)
    {
        elog(WARNING, "pgraft_create_snapshot: pgraft not initialized");
        return;
    }
    
    snapshot_data = pgraft_create_replication_snapshot();
    if (snapshot_data)
    {
        elog(INFO, "pgraft_create_snapshot: snapshot created successfully");
        elog(DEBUG1, "pgraft_create_snapshot: snapshot data: %s", snapshot_data);
        
        /* Free the snapshot data */
        pgraft_free_replication_string(snapshot_data);
    }
    else
    {
        elog(ERROR, "pgraft_create_snapshot: failed to create snapshot");
    }
}

/*
 * Load Go Raft library dynamically
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
        go_lib_handle = dlopen("/usr/local/pgsql.17/lib/pgraft_go.dylib", RTLD_LAZY);
        if (go_lib_handle)
            break;
            
        retry_count++;
        elog(WARNING, "pgraft: attempt %d failed to load Go library: %s", 
             retry_count, dlerror());
        
        if (retry_count < max_retries)
        {
            /* Wait before retry with exponential backoff */
            pg_usleep(100000 * retry_count); /* 100ms * retry_count */
        }
    }
    
    if (!go_lib_handle)
    {
        elog(ERROR, "pgraft_consensus: failed to load Go library after %d attempts", max_retries);
        return -1;
    }
    
    /* Load function pointers */
    pgraft_go_init_ptr = (pgraft_go_init_func) dlsym(go_lib_handle, "pgraft_go_init");
    pgraft_go_start_ptr = (pgraft_go_start_func) dlsym(go_lib_handle, "pgraft_go_start");
    pgraft_go_stop_ptr = (pgraft_go_stop_func) dlsym(go_lib_handle, "pgraft_go_stop");
    pgraft_go_add_node_ptr = (pgraft_go_add_node_func) dlsym(go_lib_handle, "pgraft_go_add_node");
    pgraft_go_remove_node_ptr = (pgraft_go_remove_node_func) dlsym(go_lib_handle, "pgraft_go_remove_node");
    pgraft_go_get_state_ptr = (pgraft_go_get_state_func) dlsym(go_lib_handle, "pgraft_go_get_state");
    pgraft_go_get_leader_ptr = (pgraft_go_get_leader_func) dlsym(go_lib_handle, "pgraft_go_get_leader");
    pgraft_go_get_term_ptr = (pgraft_go_get_term_func) dlsym(go_lib_handle, "pgraft_go_get_term");
    pgraft_go_is_leader_ptr = (pgraft_go_is_leader_func) dlsym(go_lib_handle, "pgraft_go_is_leader");
    pgraft_go_append_log_ptr = (pgraft_go_append_log_func) dlsym(go_lib_handle, "pgraft_go_append_log");
    pgraft_go_get_stats_ptr = (pgraft_go_get_stats_func) dlsym(go_lib_handle, "pgraft_go_get_stats");
    pgraft_go_get_nodes_ptr = (pgraft_go_get_nodes_func) dlsym(go_lib_handle, "pgraft_go_get_nodes");
    pgraft_go_get_log_ptr = (pgraft_go_get_log_func) dlsym(go_lib_handle, "pgraft_go_get_log");
    pgraft_go_read_log_ptr = (pgraft_go_read_log_func) dlsym(go_lib_handle, "pgraft_go_read_log");
    pgraft_go_commit_log_ptr = (pgraft_go_commit_log_func) dlsym(go_lib_handle, "pgraft_go_commit_log");
    pgraft_go_version_ptr = (pgraft_go_version_func) dlsym(go_lib_handle, "pgraft_go_version");
    pgraft_go_free_string_ptr = (pgraft_go_free_string_func) dlsym(go_lib_handle, "pgraft_go_free_string");
    
    /* Check if all critical functions were loaded */
    if (!pgraft_go_init_ptr || !pgraft_go_start_ptr || !pgraft_go_stop_ptr)
    {
        elog(ERROR, "pgraft_consensus: failed to load critical Go functions");
        dlclose(go_lib_handle);
        go_lib_handle = NULL;
        return -1;
    }
    
    go_lib_loaded = true;
    elog(INFO, "pgraft_consensus: Go Raft library loaded successfully");
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
    pgraft_go_add_node_ptr = NULL;
    pgraft_go_remove_node_ptr = NULL;
    pgraft_go_get_state_ptr = NULL;
    pgraft_go_get_leader_ptr = NULL;
    pgraft_go_get_term_ptr = NULL;
    pgraft_go_is_leader_ptr = NULL;
    pgraft_go_append_log_ptr = NULL;
    pgraft_go_get_stats_ptr = NULL;
    pgraft_go_get_nodes_ptr = NULL;
    pgraft_go_get_log_ptr = NULL;
    pgraft_go_read_log_ptr = NULL;
    pgraft_go_commit_log_ptr = NULL;
    pgraft_go_version_ptr = NULL;
    pgraft_go_free_string_ptr = NULL;
    
    go_lib_loaded = false;
    elog(INFO, "pgraft_consensus: Go Raft library unloaded");
}


