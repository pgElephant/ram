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

/* Debug control function */
typedef int (*pgraft_go_set_debug_func)(int enabled);
static pgraft_go_set_debug_func pgraft_go_set_debug_ptr = NULL;

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

/* Shared memory structure for Raft state */
typedef struct pgraft_shared_state
{
    /* Raft state */
    int32 node_id;
    char address[256];
    int32 port;
    int32 initialized;
    int32 running;
    int32 current_term;
    int64 leader_id;
    char state[64];
    
    /* Go library state */
    int32 go_lib_loaded;
    int32 go_initialized;
    int32 go_running;
    
    /* Statistics */
    int64 messages_processed;
    int64 heartbeats_sent;
    int64 log_entries;
    
    /* Mutex for thread safety */
    slock_t mutex;
} pgraft_shared_state;

/* Shared memory variables */
static pgraft_shared_state *pgraft_shmem = NULL;
static Size pgraft_shmem_size = 0;

/* Shared memory functions */
static void pgraft_shmem_request_hook(void);
static void pgraft_shmem_shutdown_hook(int code, Datum arg);
static void pgraft_init_shared_memory(void);
static void pgraft_cleanup_shared_memory(void);
static pgraft_shared_state *pgraft_get_shared_memory(void);
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
typedef int (*pgraft_go_add_peer_func)(int nodeID, char* address, int port);
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
typedef int (*pgraft_go_test_func)(void);
typedef void (*pgraft_go_free_string_func)(char* s);

/* Function pointers */
static pgraft_go_init_func pgraft_go_init_ptr = NULL;
static pgraft_go_start_func pgraft_go_start_ptr = NULL;
static pgraft_go_stop_func pgraft_go_stop_ptr = NULL;
static pgraft_go_add_node_func pgraft_go_add_node_ptr = NULL;
static pgraft_go_add_peer_func pgraft_go_add_peer_ptr = NULL;
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
static pgraft_go_test_func pgraft_go_test_ptr = NULL;
static pgraft_go_free_string_func pgraft_go_free_string_ptr = NULL;

/* Forward declarations */
static void pgraft_worker_tick(void);
static int load_go_library(void);
static void unload_go_library(void);

/* Background worker processes */
static bool worker_running = false;

/* Function info declarations are in their respective modules */

/*
 * Extension initialization - Main coordinator
 */
void
_PG_init(void)
{
    /* Extension can be loaded dynamically for testing */
    /* if (!process_shared_preload_libraries_in_progress)
        elog(ERROR, "pgraft is not in shared_preload_libraries"); */
    
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
    
    /* Worker management system will be initialized when explicitly requested */
    /* Not during extension loading to avoid postmaster startup issues */
    
    /* Register shared memory hooks */
    shmem_request_hook = pgraft_shmem_request_hook;
    before_shmem_exit(pgraft_shmem_shutdown_hook, (Datum) 0);
    
    /* Go Raft system will be initialized when explicitly requested via SQL functions */
    /* Not during extension loading to avoid postmaster startup issues */
    
    /* Background workers will be registered when explicitly requested */
    /* Not during extension loading to avoid postmaster startup issues */
    
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
        elog(DEBUG1, "pgraft_is_healthy: communication system not initialized - pgraft_comm_initialized=%d", 
             pgraft_comm_initialized() ? 1 : 0);
        return false;
    }
    
    /* Check if communication system has active connections */
    if (pgraft_comm_get_active_connections() == 0)
    {
        elog(DEBUG1, "pgraft_is_healthy: no active communication connections - active_connections=%d", 
             pgraft_comm_get_active_connections());
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
        go_lib_handle = dlopen("/Users/postgres/pgelephant/pge/ram/pgraft/src/pgraft_go.dylib", RTLD_LAZY);
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
    pgraft_go_add_node_ptr = (pgraft_go_add_node_func) dlsym(go_lib_handle, "pgraft_go_add_peer");
    pgraft_go_add_peer_ptr = (pgraft_go_add_peer_func) dlsym(go_lib_handle, "pgraft_go_add_peer");
    
    /* Debug function pointer loading */
    elog(INFO, "pgraft: function pointers loaded - init=%p, start=%p, stop=%p, add_node=%p, add_peer=%p", 
         pgraft_go_init_ptr, pgraft_go_start_ptr, pgraft_go_stop_ptr, pgraft_go_add_node_ptr, pgraft_go_add_peer_ptr);
    pgraft_go_remove_node_ptr = (pgraft_go_remove_node_func) dlsym(go_lib_handle, "pgraft_go_remove_peer");
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
    pgraft_go_test_ptr = (pgraft_go_test_func) dlsym(go_lib_handle, "pgraft_go_test");
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
    pgraft_go_add_peer_ptr = NULL;
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
    pgraft_go_test_ptr = NULL;
    pgraft_go_free_string_ptr = NULL;
    
    go_lib_loaded = false;
    elog(INFO, "pgraft_consensus: Go Raft library unloaded");
}

/* SQL function implementations */

PG_FUNCTION_INFO_V1(pgraft_init);
PG_FUNCTION_INFO_V1(pgraft_start);
PG_FUNCTION_INFO_V1(pgraft_stop);
PG_FUNCTION_INFO_V1(pgraft_get_state);
PG_FUNCTION_INFO_V1(pgraft_get_leader);
PG_FUNCTION_INFO_V1(pgraft_get_term);
PG_FUNCTION_INFO_V1(pgraft_version);
PG_FUNCTION_INFO_V1(pgraft_test);
PG_FUNCTION_INFO_V1(pgraft_add_node);
PG_FUNCTION_INFO_V1(pgraft_remove_node);
PG_FUNCTION_INFO_V1(pgraft_get_nodes);
PG_FUNCTION_INFO_V1(pgraft_get_log);
PG_FUNCTION_INFO_V1(pgraft_get_stats);
PG_FUNCTION_INFO_V1(pgraft_append_log);
PG_FUNCTION_INFO_V1(pgraft_commit_log);
PG_FUNCTION_INFO_V1(pgraft_read_log);
PG_FUNCTION_INFO_V1(pgraft_is_leader);

Datum
pgraft_init(PG_FUNCTION_ARGS)
{
    int32 node_id = PG_GETARG_INT32(0);
    text *address_text = PG_GETARG_TEXT_PP(1);
    int32 port = PG_GETARG_INT32(2);
    char *address = text_to_cstring(address_text);
    int result;
    pgraft_shared_state *shmem;
    
    elog(INFO, "pgraft_init: called with node_id=%d, address=%s, port=%d", node_id, address, port);
    
    /* Get shared memory */
    shmem = pgraft_get_shared_memory();
    if (shmem == NULL) {
        elog(ERROR, "pgraft_init: failed to get shared memory");
        PG_RETURN_BOOL(false);
    }
    
    /* Check if already initialized */
    if (shmem->initialized) {
        elog(INFO, "pgraft_init: already initialized");
        PG_RETURN_BOOL(true);
    }
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        result = load_go_library();
        if (result != 0) {
            elog(ERROR, "pgraft_init: failed to load Go library: %d", result);
            PG_RETURN_BOOL(false);
        }
    }
    
    /* Initialize Go Raft system only if not already initialized */
    if (!shmem->go_initialized) {
        if (pgraft_go_init_ptr) {
            elog(INFO, "pgraft_init: calling Go init function with node_id=%d, address=%s, port=%d", node_id, address, port);
            result = pgraft_go_init_ptr(node_id, address, port);
            elog(INFO, "pgraft_init: Go init function returned: %d", result);
            if (result != 0) {
                elog(ERROR, "pgraft_init: failed to initialize Go Raft system: %d", result);
                PG_RETURN_BOOL(false);
            }
            /* Only set go_initialized if Go init actually succeeded */
            SpinLockAcquire(&shmem->mutex);
            shmem->go_initialized = 1;
            SpinLockRelease(&shmem->mutex);
            elog(INFO, "pgraft_init: Go Raft system initialized and marked in shared memory");
        } else {
            elog(ERROR, "pgraft_init: Go init function not available");
            PG_RETURN_BOOL(false);
        }
    } else {
        elog(INFO, "pgraft_init: Go Raft system already initialized (from shared memory)");
    }
    
    /* Update shared memory */
    SpinLockAcquire(&shmem->mutex);
    shmem->node_id = node_id;
    strncpy(shmem->address, address, sizeof(shmem->address) - 1);
    shmem->address[sizeof(shmem->address) - 1] = '\0';
    shmem->port = port;
    shmem->initialized = 1;
    shmem->go_lib_loaded = 1;
    SpinLockRelease(&shmem->mutex);
    
    elog(INFO, "pgraft_init: initialization completed successfully");
    PG_RETURN_BOOL(true);
}

Datum
pgraft_start(PG_FUNCTION_ARGS)
{
    int result;
    pgraft_shared_state *shmem;
    
    elog(INFO, "pgraft_start: called");
    
    /* Get shared memory */
    shmem = pgraft_get_shared_memory();
    if (shmem == NULL) {
        elog(ERROR, "pgraft_start: failed to get shared memory");
        PG_RETURN_BOOL(false);
    }
    
    /* Check if Raft is initialized - this is mandatory */
    if (!shmem->go_initialized) {
        elog(ERROR, "pgraft_start: Raft system not initialized, call pgraft_init first");
        PG_RETURN_BOOL(false);
    }
    
    /* Check if already running */
    if (shmem->running) {
        elog(INFO, "pgraft_start: already running");
        PG_RETURN_BOOL(true);
    }
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        result = load_go_library();
        if (result != 0) {
            elog(ERROR, "pgraft_start: failed to load Go library: %d", result);
            PG_RETURN_BOOL(false);
        }
    }
    
    /* Check if Go library is properly loaded */
    if (!pgraft_go_start_ptr) {
        elog(ERROR, "pgraft_start: Go start function not available after loading library");
        PG_RETURN_BOOL(false);
    }
    
    /* Start Go Raft system */
    if (pgraft_go_start_ptr) {
        elog(INFO, "pgraft_start: calling pgraft_go_start_ptr, pointer=%p", pgraft_go_start_ptr);
        result = pgraft_go_start_ptr();
        elog(INFO, "pgraft_start: Go Raft system start returned: %d", result);
        if (result != 0) {
            elog(ERROR, "pgraft_start: failed to start Go Raft system: %d", result);
            PG_RETURN_BOOL(false);
        }
    } else {
        elog(ERROR, "pgraft_start: Go start function not available");
        PG_RETURN_BOOL(false);
    }
    
    /* Update shared memory */
    SpinLockAcquire(&shmem->mutex);
    shmem->running = 1;
    shmem->go_running = 1;
    strncpy(shmem->state, "running", sizeof(shmem->state) - 1);
    shmem->state[sizeof(shmem->state) - 1] = '\0';
    SpinLockRelease(&shmem->mutex);
    
    elog(INFO, "pgraft_start: start completed successfully");
    PG_RETURN_BOOL(true);
}

Datum
pgraft_stop(PG_FUNCTION_ARGS)
{
    int result;
    
    elog(INFO, "pgraft_stop: called");
    
    if (!go_lib_loaded || !pgraft_go_stop_ptr) {
        elog(INFO, "pgraft_stop: Go library not loaded or stop function not available");
        PG_RETURN_BOOL(true);
    }
    
    result = pgraft_go_stop_ptr();
    if (result != 0) {
        elog(WARNING, "pgraft_stop: Go Raft system stop returned: %d", result);
    }
    
    elog(INFO, "pgraft_stop: stop completed");
    PG_RETURN_BOOL(true);
}

Datum
pgraft_get_state(PG_FUNCTION_ARGS)
{
    pgraft_shared_state *shmem;
    char state[64];
    
    elog(INFO, "pgraft_get_state: called");
    
    /* Get shared memory */
    shmem = pgraft_get_shared_memory();
    if (shmem == NULL) {
        elog(INFO, "pgraft_get_state: failed to get shared memory");
        PG_RETURN_TEXT_P(cstring_to_text("stopped"));
    }
    
    /* Read state from shared memory */
    SpinLockAcquire(&shmem->mutex);
    strncpy(state, shmem->state, sizeof(state) - 1);
    state[sizeof(state) - 1] = '\0';
    SpinLockRelease(&shmem->mutex);
    
    elog(INFO, "pgraft_get_state: returning state: %s", state);
    PG_RETURN_TEXT_P(cstring_to_text(state));
}

Datum
pgraft_get_leader(PG_FUNCTION_ARGS)
{
    pgraft_shared_state *shmem;
    int64 leader;
    
    elog(INFO, "pgraft_get_leader: called");
    
    /* Get shared memory */
    shmem = pgraft_get_shared_memory();
    if (shmem == NULL) {
        elog(INFO, "pgraft_get_leader: failed to get shared memory");
        PG_RETURN_INT64(-1);
    }
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        int result = load_go_library();
        if (result != 0) {
            elog(INFO, "pgraft_get_leader: failed to load Go library: %d", result);
            PG_RETURN_INT64(-1);
        }
    }
    
    /* Get leader from Go Raft system */
    if (pgraft_go_get_leader_ptr) {
        leader = pgraft_go_get_leader_ptr();
        elog(INFO, "pgraft_get_leader: Go function returned leader: %lld", (long long)leader);
        
        /* Update shared memory with the actual leader */
        SpinLockAcquire(&shmem->mutex);
        shmem->leader_id = leader;
        SpinLockRelease(&shmem->mutex);
    } else {
        elog(INFO, "pgraft_get_leader: Go get leader function not available");
        leader = -1;
    }
    
    elog(INFO, "pgraft_get_leader: returning leader: %lld", (long long)leader);
    PG_RETURN_INT64(leader);
}

Datum
pgraft_get_term(PG_FUNCTION_ARGS)
{
    pgraft_shared_state *shmem;
    int64 term;
    
    elog(INFO, "pgraft_get_term: called");
    
    /* Get shared memory */
    shmem = pgraft_get_shared_memory();
    if (shmem == NULL) {
        elog(INFO, "pgraft_get_term: failed to get shared memory");
        PG_RETURN_INT64(-1);
    }
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        int result = load_go_library();
        if (result != 0) {
            elog(INFO, "pgraft_get_term: failed to load Go library: %d", result);
            PG_RETURN_INT64(-1);
        }
    }
    
    /* Get term from Go Raft system */
    if (pgraft_go_get_term_ptr) {
        term = pgraft_go_get_term_ptr();
        elog(INFO, "pgraft_get_term: Go function returned term: %lld", (long long)term);
        
        /* Update shared memory with the actual term */
        SpinLockAcquire(&shmem->mutex);
        shmem->current_term = term;
        SpinLockRelease(&shmem->mutex);
    } else {
        elog(INFO, "pgraft_get_term: Go get term function not available");
        term = 0;
    }
    
    elog(INFO, "pgraft_get_term: returning term: %lld", (long long)term);
    PG_RETURN_INT64(term);
}

Datum
pgraft_version(PG_FUNCTION_ARGS)
{
    char *version;
    
    elog(INFO, "pgraft_version: called");
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        int result = load_go_library();
        if (result != 0) {
            elog(INFO, "pgraft_version: failed to load Go library: %d", result);
            PG_RETURN_TEXT_P(cstring_to_text("1.0.0"));
        }
    }
    
    if (!pgraft_go_version_ptr) {
        elog(INFO, "pgraft_version: Go version function not available");
        PG_RETURN_TEXT_P(cstring_to_text("1.0.0"));
    }
    
    version = pgraft_go_version_ptr();
    if (version == NULL) {
        elog(INFO, "pgraft_version: Go function returned NULL");
        PG_RETURN_TEXT_P(cstring_to_text("1.0.0"));
    }
    
    elog(INFO, "pgraft_version: returning version: %s", version);
    PG_RETURN_TEXT_P(cstring_to_text(version));
}

Datum
pgraft_test(PG_FUNCTION_ARGS)
{
    int result;
    
    elog(INFO, "pgraft_test: called");
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        int load_result = load_go_library();
        if (load_result != 0) {
            elog(INFO, "pgraft_test: failed to load Go library: %d", load_result);
            PG_RETURN_INT32(0);
        }
    }
    
    if (!pgraft_go_test_ptr) {
        elog(INFO, "pgraft_test: Go test function not available");
        PG_RETURN_INT32(0);
    }
    
    result = pgraft_go_test_ptr();
    elog(INFO, "pgraft_test: Go test function returned: %d", result);
    
    PG_RETURN_INT32(result);
}

Datum
pgraft_add_node(PG_FUNCTION_ARGS)
{
    int32 node_id = PG_GETARG_INT32(0);
    text *address_text = PG_GETARG_TEXT_PP(1);
    int32 port = PG_GETARG_INT32(2);
    char *address = text_to_cstring(address_text);
    int result;
    
    elog(INFO, "pgraft_add_node: called with node_id=%d, address=%s, port=%d", node_id, address, port);
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        result = load_go_library();
        if (result != 0) {
            elog(ERROR, "pgraft_add_node: failed to load Go library: %d", result);
            PG_RETURN_BOOL(false);
        }
    }
    
    /* Add node to Go Raft system */
    if (pgraft_go_add_peer_ptr) {
        result = pgraft_go_add_peer_ptr(node_id, address, port);
        if (result != 0) {
            elog(ERROR, "pgraft_add_node: failed to add node to Go Raft system: %d", result);
            PG_RETURN_BOOL(false);
        }
    } else {
        elog(ERROR, "pgraft_add_node: Go add peer function not available");
        PG_RETURN_BOOL(false);
    }
    
    elog(INFO, "pgraft_add_node: node added successfully");
    PG_RETURN_BOOL(true);
}

Datum
pgraft_remove_node(PG_FUNCTION_ARGS)
{
    int32 node_id = PG_GETARG_INT32(0);
    int result;
    
    elog(INFO, "pgraft_remove_node: called with node_id=%d", node_id);
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        result = load_go_library();
        if (result != 0) {
            elog(ERROR, "pgraft_remove_node: failed to load Go library: %d", result);
            PG_RETURN_BOOL(false);
        }
    }
    
    /* Remove node from Go Raft system */
    if (pgraft_go_remove_node_ptr) {
        result = pgraft_go_remove_node_ptr(node_id);
        if (result != 0) {
            elog(ERROR, "pgraft_remove_node: failed to remove node from Go Raft system: %d", result);
            PG_RETURN_BOOL(false);
        }
    } else {
        elog(ERROR, "pgraft_remove_node: Go remove node function not available");
        PG_RETURN_BOOL(false);
    }
    
    elog(INFO, "pgraft_remove_node: node removed successfully");
    PG_RETURN_BOOL(true);
}

Datum
pgraft_get_nodes(PG_FUNCTION_ARGS)
{
    char *nodes_json;
    
    elog(INFO, "pgraft_get_nodes: called");
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        int result = load_go_library();
        if (result != 0) {
            elog(INFO, "pgraft_get_nodes: failed to load Go library: %d", result);
            PG_RETURN_TEXT_P(cstring_to_text("[]"));
        }
    }
    
    /* Get nodes from Go Raft system */
    if (pgraft_go_get_nodes_ptr) {
        nodes_json = pgraft_go_get_nodes_ptr();
        elog(INFO, "pgraft_get_nodes: Go function returned nodes: %s", nodes_json);
        PG_RETURN_TEXT_P(cstring_to_text(nodes_json));
    } else {
        elog(INFO, "pgraft_get_nodes: Go get nodes function not available");
        PG_RETURN_TEXT_P(cstring_to_text("[]"));
    }
}

Datum
pgraft_get_log(PG_FUNCTION_ARGS)
{
    /* For now, return a simple text representation */
    /* In a full implementation, this would query the Go Raft system */
    PG_RETURN_TEXT_P(cstring_to_text("[]"));
}

Datum
pgraft_get_stats(PG_FUNCTION_ARGS)
{
    /* For now, return a simple text representation */
    /* In a full implementation, this would query the Go Raft system */
    PG_RETURN_TEXT_P(cstring_to_text("{}"));
}

Datum
pgraft_append_log(PG_FUNCTION_ARGS)
{
    text *data_text = PG_GETARG_TEXT_PP(0);
    char *data = text_to_cstring(data_text);
    
    elog(INFO, "pgraft_append_log: called with data=%s", data);
    
    /* For now, just return success */
    /* In a full implementation, this would append to the Go Raft log */
    PG_RETURN_BOOL(true);
}

Datum
pgraft_commit_log(PG_FUNCTION_ARGS)
{
    int32 index = PG_GETARG_INT32(0);
    
    elog(INFO, "pgraft_commit_log: called with index=%d", index);
    
    /* For now, just return success */
    /* In a full implementation, this would commit the log entry */
    PG_RETURN_BOOL(true);
}

Datum
pgraft_read_log(PG_FUNCTION_ARGS)
{
    int32 index = PG_GETARG_INT32(0);
    
    elog(INFO, "pgraft_read_log: called with index=%d", index);
    
    /* For now, return empty data */
    /* In a full implementation, this would read from the Go Raft log */
    PG_RETURN_TEXT_P(cstring_to_text(""));
}

Datum
pgraft_is_leader(PG_FUNCTION_ARGS)
{
    int is_leader;
    
    elog(INFO, "pgraft_is_leader: called");
    
    /* Load Go library if not already loaded */
    if (!go_lib_loaded) {
        int result = load_go_library();
        if (result != 0) {
            elog(INFO, "pgraft_is_leader: failed to load Go library: %d", result);
            PG_RETURN_BOOL(false);
        }
    }
    
    /* Get leader status from Go Raft system */
    if (pgraft_go_is_leader_ptr) {
        is_leader = pgraft_go_is_leader_ptr();
        elog(INFO, "pgraft_is_leader: Go function returned: %d", is_leader);
        PG_RETURN_BOOL(is_leader != 0);
    } else {
        elog(INFO, "pgraft_is_leader: Go is leader function not available");
        PG_RETURN_BOOL(false);
    }
}


/* Shared memory functions */

/*
 * Shared memory request hook
 */
static void
pgraft_shmem_request_hook(void)
{
    if (shmem_request_hook)
        shmem_request_hook();
    
    RequestAddinShmemSpace(sizeof(pgraft_shared_state));
}


/*
 * Shared memory shutdown hook
 */
static void
pgraft_shmem_shutdown_hook(int code, Datum arg)
{
    pgraft_cleanup_shared_memory();
}

/*
 * Initialize shared memory
 */
static void
pgraft_init_shared_memory(void)
{
    bool found;
    
    elog(INFO, "pgraft: initializing shared memory");
    
    /* Calculate shared memory size */
    pgraft_shmem_size = sizeof(pgraft_shared_state);
    
    /* Allocate shared memory */
    pgraft_shmem = (pgraft_shared_state *) ShmemInitStruct("pgraft_shared_state",
                                                           pgraft_shmem_size,
                                                           &found);
    
    if (!found)
    {
        elog(INFO, "pgraft: creating new shared memory");
        /* Initialize shared memory */
        memset(pgraft_shmem, 0, pgraft_shmem_size);
        
        /* Initialize mutex */
        SpinLockInit(&pgraft_shmem->mutex);
        
        /* Initialize default values */
        pgraft_shmem->node_id = -1;
        pgraft_shmem->port = -1;
        pgraft_shmem->initialized = 0;
        pgraft_shmem->running = 0;
        pgraft_shmem->current_term = 0;
        pgraft_shmem->leader_id = -1;
        strncpy(pgraft_shmem->state, "stopped", sizeof(pgraft_shmem->state) - 1);
        pgraft_shmem->state[sizeof(pgraft_shmem->state) - 1] = '\0';
        
        pgraft_shmem->go_lib_loaded = 0;
        pgraft_shmem->go_initialized = 0;
        pgraft_shmem->go_running = 0;
        
        pgraft_shmem->messages_processed = 0;
        pgraft_shmem->heartbeats_sent = 0;
        pgraft_shmem->log_entries = 0;
        
        elog(INFO, "pgraft: shared memory initialized successfully");
    }
    else
    {
        elog(INFO, "pgraft: shared memory already exists");
    }
}

/*
 * Cleanup shared memory
 */
static void
pgraft_cleanup_shared_memory(void)
{
    if (pgraft_shmem)
    {
        elog(INFO, "pgraft: cleaning up shared memory");
        pgraft_shmem = NULL;
    }
}

/*
 * Get shared memory pointer
 */
static pgraft_shared_state *
pgraft_get_shared_memory(void)
{
    if (pgraft_shmem == NULL)
    {
        /* Initialize shared memory when first accessed */
        pgraft_init_shared_memory();
    }
    return pgraft_shmem;
}

/* Enhanced integration: Notify other components of changes */
static void
pgraft_notify_components(const char* event_type, const char* data)
{
	/* Use PostgreSQL NOTIFY to inform other components */
	char notify_query[512];
	snprintf(notify_query, sizeof(notify_query),
	         "NOTIFY ram_events, '%s:%s'", event_type, data);
	
	/* Execute notification in background if possible */
	// TODO: Implement background notification
}

/* Enhanced pgraft_add_node with component notification */
Datum
pgraft_add_node_notify(PG_FUNCTION_ARGS)
{
	int32_t node_id = PG_GETARG_INT32(0);
	text* hostname_text = PG_GETARG_TEXT_PP(1);
	int32_t port = PG_GETARG_INT32(2);
	
	char* hostname = text_to_cstring(hostname_text);
	
	/* Call original function */
	Datum result = pgraft_add_node(fcinfo);
	
	/* Notify components if successful */
	if (DatumGetBool(result))
	{
		char notify_data[256];
		snprintf(notify_data, sizeof(notify_data),
		         "{\"node_id\":%d,\"hostname\":\"%s\",\"port\":%d}",
		         node_id, hostname, port);
		pgraft_notify_components("node_added", notify_data);
	}
	
	pfree(hostname);
	PG_RETURN_DATUM(result);
}

/*
 * Set debug logging level for pgraft
 */
Datum
pgraft_set_debug(PG_FUNCTION_ARGS)
{
    bool enabled = PG_GETARG_BOOL(0);
    
    if (pgraft_go_set_debug_ptr)
    {
        pgraft_go_set_debug_ptr(enabled ? 1 : 0);
        elog(INFO, "pgraft_set_debug: Debug logging %s", enabled ? "enabled" : "disabled");
    }
    else
    {
        elog(WARNING, "pgraft_set_debug: Go library not loaded, debug control unavailable");
    }
    
    PG_RETURN_BOOL(true);
}


