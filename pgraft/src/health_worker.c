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
#include "../include/pgraft.h"
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
    
    elog(INFO, "pgraft_health_worker_init: health worker initialized");
}

void
pgraft_health_worker_start(void)
{
    if (g_health_status.is_running)
    {
        elog(WARNING, "pgraft_health_worker_start: health worker already running");
        return;
    }
    
    g_health_status.is_running = true;
    g_health_status.last_activity = GetCurrentTimestamp();
    
    elog(INFO, "pgraft_health_worker_start: health worker started");
}

void
pgraft_health_worker_stop(void)
{
    if (!g_health_status.is_running)
    {
        elog(WARNING, "pgraft_health_worker_stop: health worker not running");
        return;
    }
    
    g_health_status.is_running = false;
    g_health_status.last_activity = GetCurrentTimestamp();
    
    elog(INFO, "pgraft_health_worker_stop: health worker stopped");
}

void
pgraft_health_worker_cleanup(void)
{
    pgraft_health_worker_stop();
    g_health_initialized = false;
    
    elog(INFO, "pgraft_health_worker_cleanup: health worker cleaned up");
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
    
    if (!g_health_status.is_running)
    {
        return false;
    }
    
    if (g_health_status.is_running)
    {
        g_health_status.last_health_status = PGRAFT_HEALTH_OK;
    }
    else
    {
        g_health_status.last_health_status = PGRAFT_HEALTH_ERROR;
        is_healthy = false;
    }
    
    g_health_status.last_activity = GetCurrentTimestamp();
    g_health_status.health_checks_performed++;
    
    if (is_healthy)
    {
        g_health_status.errors_count = 0;
    }
    else
    {
        g_health_status.errors_count++;
    }
    
    elog(DEBUG1, "pgraft_health_worker_check: health check completed, status: %s", 
         is_healthy ? "HEALTHY" : "UNHEALTHY");
    
    return is_healthy;
}

void
pgraft_health_worker_main(Datum main_arg __attribute__((unused)))
{
    elog(INFO, "pgraft_health_worker_main: health worker main function started");
    
    pgraft_health_worker_init();
    pgraft_health_worker_start();
    
    while (g_health_status.is_running)
    {
        pgraft_health_worker_check();
        pg_usleep(30000000);
    }
    
    pgraft_health_worker_cleanup();
    elog(INFO, "pgraft_health_worker_main: health worker main function exiting");
}