/*-------------------------------------------------------------------------
 *
 * health_worker.c
 *		Health monitoring worker for pgraft extension
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "../include/pgraft.h"

#include "storage/latch.h"
#include "storage/proc.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <string.h>

static pgraft_health_worker_status_t g_health_status;
static bool g_health_initialized = false;

void
pgraft_health_worker_init(void)
{
    memset(&g_health_status, 0, sizeof(g_health_status));
    
    g_health_status.is_running = false;
    g_health_status.last_activity = 0;
    g_health_status.health_checks_performed = 0;
    g_health_status.last_health_status = PGRAFT_HEALTH_OK;
    g_health_status.warnings_count = 0;
    g_health_status.errors_count = 0;
    
    g_health_initialized = true;
    
    elog(INFO, "pgraft: health worker initialized");
}

void
pgraft_health_worker_start(void)
{
    if (g_health_status.is_running)
    {
        elog(WARNING, "pgraft: health worker already running");
        return;
    }
    
    g_health_status.is_running = true;
    g_health_status.last_activity = GetCurrentTimestamp();
    
    elog(INFO, "pgraft: health worker started");
}

void
pgraft_health_worker_stop(void)
{
    if (!g_health_status.is_running)
    {
        elog(WARNING, "pgraft: health worker not running");
        return;
    }
    
    g_health_status.is_running = false;
    g_health_status.last_activity = GetCurrentTimestamp();
    
    elog(INFO, "pgraft: health worker stopped");
}

void
pgraft_health_worker_cleanup(void)
{
    pgraft_health_worker_stop();
    g_health_initialized = false;
    
    elog(INFO, "pgraft: health worker cleaned up");
}

bool
pgraft_health_worker_get_status(pgraft_health_worker_status_t *status)
{
    if (!g_health_initialized)
    {
        return false;
    }
    
    *status = g_health_status;
    return true;
}

bool
pgraft_health_worker_check(void)
{
    bool is_healthy = true;
    pgraft_raft_state_t *raft_state;
    int healthy_nodes;
    
    if (!g_health_status.is_running)
    {
        return false;
    }
    
    /* Check Raft system health */
    raft_state = pgraft_raft_get_state();
    if (!raft_state || !raft_state->is_initialized)
    {
        is_healthy = false;
    }
    
    /* Check communication system health */
    if (!pgraft_comm_initialized())
    {
        is_healthy = false;
    }
    
    /* Check number of healthy nodes */
    healthy_nodes = pgraft_get_healthy_nodes_count();
    if (healthy_nodes < 1)
    {
        is_healthy = false;
    }
    
    /* Update health status */
    if (is_healthy)
    {
        g_health_status.last_health_status = PGRAFT_HEALTH_OK;
        g_health_status.errors_count = 0;
    }
    else
    {
        g_health_status.last_health_status = PGRAFT_HEALTH_ERROR;
        g_health_status.errors_count++;
    }
    
    g_health_status.last_activity = GetCurrentTimestamp();
    g_health_status.health_checks_performed++;
    
    elog(DEBUG1, "pgraft: health check completed, status: %s", 
         is_healthy ? "HEALTHY" : "UNHEALTHY");
    
    return is_healthy;
}

void __attribute__((visibility("default")))
pgraft_health_worker_main(Datum main_arg __attribute__((unused)))
{
    /* Unblock signals so we can handle shutdown requests */
    BackgroundWorkerUnblockSignals();
    
    /* Clear any existing latch state */
    if (MyLatch)
        ResetLatch(MyLatch);
    
    elog(INFO, "pgraft: health worker main function started");
    
    pgraft_health_worker_init();
    pgraft_health_worker_start();
    
    for (;;)
    {
        /* Check for shutdown signal */
        if (MyLatch && MyLatch->is_set)
        {
            elog(INFO, "pgraft: health worker received shutdown signal");
            break;
        }
        
        /* Check for process exit */
        if (proc_exit_inprogress)
        {
            elog(INFO, "pgraft: health worker received process exit signal");
            break;
        }
       
        elog(DEBUG1, "pgraft: health worker main function running");
        pg_usleep(30000000);
     
        pgraft_health_worker_check();
    }
    
    pgraft_health_worker_cleanup();
    elog(INFO, "pgraft: health worker main function exiting");
}