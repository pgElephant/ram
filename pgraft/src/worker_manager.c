/*-------------------------------------------------------------------------
 *
 * worker_manager.c
 *		Background worker management for pgraft extension
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "../include/pgraft.h"
#include <pthread.h>
#include <string.h>

typedef struct pgraft_worker_def
{
    const char *name;
    const char *function;
    int bgw_flags;
    int bgw_restart_time;
    const char *bgw_library_name;
    const char *bgw_function_name;
    const char *bgw_notify_pid;
    const char *bgw_main_arg;
    int bgw_extra;
} pgraft_worker_def_t;

typedef struct pgraft_worker
{
    BackgroundWorker worker;
    bool is_registered;
    bool is_running;
    pid_t pid;
    TimestampTz start_time;
} pgraft_worker_t;

static pgraft_worker_def_t worker_defs[] = {
    {
        .name = "pgraft_consensus_worker",
        .function = "pgraft_consensus_worker_main",
        .bgw_flags = BGWORKER_SHMEM_ACCESS,
        .bgw_restart_time = 5,
        .bgw_library_name = "pgraft",
        .bgw_function_name = "pgraft_consensus_worker_main",
        .bgw_notify_pid = NULL,
        .bgw_main_arg = NULL,
        .bgw_extra = 0
    },
    {
        .name = "pgraft_health_worker",
        .function = "pgraft_health_worker_main",
        .bgw_flags = BGWORKER_SHMEM_ACCESS,
        .bgw_restart_time = 10,
        .bgw_library_name = "pgraft",
        .bgw_function_name = "pgraft_health_worker_main",
        .bgw_notify_pid = NULL,
        .bgw_main_arg = NULL,
        .bgw_extra = 0
    }
};

#define NUM_WORKERS (sizeof(worker_defs) / sizeof(worker_defs[0]))

static pgraft_worker_t workers[NUM_WORKERS];
static bool workers_initialized = false;

static bool
register_worker(const pgraft_worker_def_t *worker_def, pgraft_worker_t *worker)
{
    BackgroundWorker worker_bgw;
    
    memset(&worker_bgw, 0, sizeof(worker_bgw));
    
    strncpy(worker_bgw.bgw_name, worker_def->name, BGW_MAXLEN - 1);
    worker_bgw.bgw_flags = worker_def->bgw_flags;
    worker_bgw.bgw_restart_time = worker_def->bgw_restart_time;
    worker_bgw.bgw_main_arg = Int32GetDatum(0);
    
    if (worker_def->bgw_library_name)
        strncpy(worker_bgw.bgw_library_name, worker_def->bgw_library_name, BGW_MAXLEN - 1);
    
    if (worker_def->bgw_function_name)
        strncpy(worker_bgw.bgw_function_name, worker_def->bgw_function_name, BGW_MAXLEN - 1);
    
    RegisterBackgroundWorker(&worker_bgw);
    
    worker->is_registered = true;
    worker->is_running = false;
    worker->pid = 0;
    worker->start_time = 0;
    
    elog(INFO, "pgraft: registered worker '%s'", worker_def->name);
    return true;
}

void
pgraft_worker_manager_init(void)
{
    int i;
    
    if (workers_initialized)
    {
        elog(WARNING, "pgraft: worker manager already initialized");
        return;
    }
    
    memset(workers, 0, sizeof(workers));
    
    for (i = 0; i < NUM_WORKERS; i++)
    {
        if (!register_worker(&worker_defs[i], &workers[i]))
        {
            elog(ERROR, "pgraft: failed to register worker '%s'", 
                 worker_defs[i].name);
            return;
        }
    }
    
    workers_initialized = true;
    elog(INFO, "pgraft: worker manager initialized with %lu workers", (unsigned long)NUM_WORKERS);
}

void
pgraft_worker_manager_cleanup(void)
{
    int i;
    
    if (!workers_initialized)
    {
        return;
    }
    
    for (i = 0; i < NUM_WORKERS; i++)
    {
        workers[i].is_registered = false;
        workers[i].is_running = false;
    }
    
    workers_initialized = false;
    elog(INFO, "pgraft: worker manager cleaned up");
}

bool
pgraft_worker_manager_get_status(pgraft_health_worker_status_t *status)
{
    int i;
    int running_count = 0;
    
    if (!workers_initialized)
    {
        return false;
    }
    
    for (i = 0; i < NUM_WORKERS; i++)
    {
        if (workers[i].is_running)
        {
            running_count++;
        }
    }
    
    status->is_running = (running_count > 0);
    status->last_activity = GetCurrentTimestamp();
    status->health_checks_performed = running_count;
    status->last_health_status = running_count > 0 ? PGRAFT_HEALTH_OK : PGRAFT_HEALTH_ERROR;
    status->warnings_count = 0;
    status->errors_count = NUM_WORKERS - running_count;
    
    return true;
}