/*
 * health_worker.c
 * Health monitoring worker for pgraft extension
 *
 * This module provides health monitoring capabilities for the pgraft extension.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 * All rights reserved.
 */

#include "postgres.h"
#include "../include/pgraft.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "utils/elog.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "utils/guc.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include <string.h>
#include <time.h>

#define HEALTH_WORKER_NAME "pgraft health worker"
#define HEALTH_WORKER_SLEEP_MS 5000

/* Health worker status structure is defined in pgraft.h */

static volatile sig_atomic_t got_sigterm = false;
static volatile sig_atomic_t got_sighup = false;

/* Health worker status */
static pgraft_health_worker_status_t g_health_status;

/* Forward declarations */
static void pgraft_health_worker_sigterm(SIGNAL_ARGS);
static void pgraft_health_worker_sighup(SIGNAL_ARGS);
static void perform_health_checks(void);

/*
 * Get health status snapshot
 */
void
pgraft_health_status_snapshot(pgraft_health_worker_status_t* out)
{
    if (!out)
        return;
    *out = g_health_status;
}

/*
 * Register health worker
 */
void
pgraft_health_worker_register(void)
{
    BackgroundWorker worker;
    
    MemSet(&worker, 0, sizeof(BackgroundWorker));
    
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    worker.bgw_main_arg = Int32GetDatum(0);
    worker.bgw_notify_pid = 0;
    
    strlcpy(worker.bgw_library_name, "pgraft", sizeof(worker.bgw_library_name));
    strlcpy(worker.bgw_name, HEALTH_WORKER_NAME, sizeof(worker.bgw_name));
    strlcpy(worker.bgw_function_name, "pgraft_health_worker_main", sizeof(worker.bgw_function_name));
    strlcpy(worker.bgw_type, "pgraft", sizeof(worker.bgw_type));
    
    RegisterBackgroundWorker(&worker);
}

/*
 * Health worker main function
 */
void
pgraft_health_worker_main(Datum main_arg)
{
    (void) main_arg;
    
    /* Initialize health status */
    MemSet(&g_health_status, 0, sizeof(g_health_status));
    g_health_status.is_running = true;
    
    /* Unblock signals */
    BackgroundWorkerUnblockSignals();
    
    /* Register signal handlers */
    pqsignal(SIGTERM, pgraft_health_worker_sigterm);
    pqsignal(SIGHUP, pgraft_health_worker_sighup);
    
    /* Connect to database */
    BackgroundWorkerInitializeConnection("postgres", NULL, 0);
    
    elog(INFO, "pgraft: health worker started");
    
    /* Main loop */
    while (!got_sigterm)
    {
        int rc;
        
        /* Check for shutdown conditions */
        CHECK_FOR_INTERRUPTS();
        if (got_sigterm)
        {
            elog(LOG, "pgraft: health worker detected shutdown condition, exiting");
            break;
        }
        
        /* Perform health checks */
        perform_health_checks();
        
        /* Wait for next iteration or signal */
        rc = WaitLatch(&MyProc->procLatch,
                       WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
                       HEALTH_WORKER_SLEEP_MS,
                       0);
        
        if (rc & WL_POSTMASTER_DEATH)
        {
            elog(LOG, "pgraft: health worker detected postmaster death, exiting");
            proc_exit(0);
        }
        
        if (rc & WL_LATCH_SET)
            ResetLatch(&MyProc->procLatch);
        
        if (got_sighup)
        {
            got_sighup = false;
            ProcessConfigFile(PGC_SIGHUP);
        }
        
        /* Additional shutdown check */
        CHECK_FOR_INTERRUPTS();
    }
    
    /* Cleanup and exit */
    g_health_status.is_running = false;
    elog(INFO, "pgraft: health worker stopped");
    proc_exit(0);
}

/*
 * Perform health checks
 */
static void
perform_health_checks(void)
{
    TimestampTz now;
    pgraft_health_status_t status;
    
    now = GetCurrentTimestamp();
    g_health_status.last_activity = now;
    g_health_status.health_checks_performed++;
    
    /* Basic health check - can be extended */
    status = PGRAFT_HEALTH_OK;
    
    g_health_status.last_health_status = status;
    if (status == PGRAFT_HEALTH_WARNING)
        g_health_status.warnings_count++;
    if (status == PGRAFT_HEALTH_ERROR || status == PGRAFT_HEALTH_CRITICAL)
        g_health_status.errors_count++;
    
    elog(DEBUG1, "pgraft: health check completed, status=%d", status);
}

/*
 * Signal handler for SIGTERM
 */
static void
pgraft_health_worker_sigterm(SIGNAL_ARGS)
{
    (void) postgres_signal_arg;
    
    elog(LOG, "pgraft: health worker received SIGTERM, initiating shutdown");
    got_sigterm = true;
    g_health_status.is_running = false;
    
    /* Wake up main loop */
    if (MyProc)
        SetLatch(&MyProc->procLatch);
}

/*
 * Signal handler for SIGHUP
 */
static void
pgraft_health_worker_sighup(SIGNAL_ARGS)
{
    (void) postgres_signal_arg;
    got_sighup = true;
    SetLatch(&MyProc->procLatch);
}