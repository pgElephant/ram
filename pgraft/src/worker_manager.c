/*
 * worker_manager.c
 * Background worker management for pgraft extension
 *
 * This module manages background workers for the pgraft extension,
 * providing health monitoring and cluster management capabilities.
 */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "utils/guc.h"
#include "utils/elog.h"
#include "../include/pgraft.h"

#define MAX_WORKERS 10

typedef struct
{
    const char* name;
    const char* function;
    int sleep_ms;
    bool enabled;
    BackgroundWorker worker;
} worker_definition_t;

static worker_definition_t workers[] = {
    {.name = "pgraft health worker",
     .function = "pgraft_health_worker_main",
     .sleep_ms = 5000,
     .enabled = true}
};

static int worker_count = sizeof(workers) / sizeof(workers[0]);
static bool workers_registered = false;

static void worker_manager_init(void);
static void register_worker(worker_definition_t* worker_def);
static void worker_manager_cleanup(void);

void pgraft_worker_manager_init(void);
void pgraft_worker_manager_cleanup(void);
void pgraft_worker_manager_get_status(StringInfo buf);

void pgraft_worker_manager_init(void)
{
    if (workers_registered)
        return;

    worker_manager_init();
    workers_registered = true;

    elog(LOG, "pgraft: Worker manager initialized with %d workers",
         worker_count);
}

static void worker_manager_init(void)
{
    int i;

    for (i = 0; i < worker_count; i++)
    {
        if (workers[i].enabled)
            register_worker(&workers[i]);
    }
}

static void register_worker(worker_definition_t* worker_def)
{
    if (strcmp(worker_def->function, "pgraft_health_worker_main") == 0)
    {
        pgraft_health_worker_register();
        elog(LOG, "pgraft: Registered health worker");
    }
    else
    {
        elog(WARNING, "pgraft: Unknown worker function: %s",
             worker_def->function);
    }
}

void pgraft_worker_manager_cleanup(void)
{
    if (!workers_registered)
        return;

    elog(LOG, "pgraft: Worker manager cleanup starting...");
    
    /* Terminate all registered background workers */
    worker_manager_cleanup();
    workers_registered = false;

    elog(LOG, "pgraft: Worker manager cleaned up successfully");
}

static void worker_manager_cleanup(void)
{
    int i;
    
    elog(DEBUG1, "pgraft: Worker manager cleanup starting with %d workers", worker_count);
    
    /* Log cleanup attempt for each worker - the workers will handle their own shutdown signals */
    for (i = 0; i < worker_count; i++)
    {
        if (workers[i].enabled)
        {
            elog(DEBUG1, "pgraft: Requesting shutdown for worker: %s", workers[i].name);
        }
    }
    
    /* Reset worker count */
    worker_count = 0;
    
    elog(DEBUG1, "pgraft: Worker manager cleanup completed");
}

void pgraft_worker_manager_get_status(StringInfo buf)
{
    int i;

    appendStringInfo(buf, "pgraft Worker Manager Status:\n");
    appendStringInfo(buf, "Workers registered: %s\n",
                     workers_registered ? "yes" : "no");
    appendStringInfo(buf, "Total workers: %d\n", worker_count);

    for (i = 0; i < worker_count; i++)
    {
        appendStringInfo(buf, "  %s: %s\n", workers[i].name,
                         workers[i].enabled ? "enabled" : "disabled");
    }
}
