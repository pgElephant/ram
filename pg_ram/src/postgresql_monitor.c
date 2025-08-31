/*-------------------------------------------------------------------------
 *
 * postgresql_monitor.c
 *		PostgreSQL monitoring implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/lwlock.h"
#include "storage/buf_internals.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "access/xlog.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "postgresql_monitor.h"
#include "pgram_guc.h"

postgresql_monitor_config_t *postgresql_monitor_config = NULL;

bool
postgresql_monitor_init(void)
{
    if (postgresql_monitor_config != NULL && postgresql_monitor_config->enabled)
        return true;
        
    postgresql_monitor_config = (postgresql_monitor_config_t *) 
        palloc0(sizeof(postgresql_monitor_config_t));
    
    if (postgresql_monitor_config == NULL)
    {
        elog(ERROR, "pg_ram: Failed to allocate PostgreSQL monitor configuration");
        return false;
    }
    
    /* Initialize configuration */
    postgresql_monitor_config->enabled = true;
    postgresql_monitor_config->check_interval_seconds = 30;
    postgresql_monitor_config->timeout_ms = 5000;
    postgresql_monitor_config->health_threshold = 0.8;  /* 80% */
    postgresql_monitor_config->detailed_monitoring = true;
    
    elog(LOG, "pg_ram: PostgreSQL monitor initialized successfully");
    return true;
}

void
postgresql_monitor_cleanup(void)
{
    if (postgresql_monitor_config == NULL)
        return;
        
    pfree(postgresql_monitor_config);
    postgresql_monitor_config = NULL;
    
    elog(LOG, "pg_ram: PostgreSQL monitor cleanup completed");
}

bool
postgresql_monitor_health_check(postgresql_health_t *health_out)
{
    if (postgresql_monitor_config == NULL || !postgresql_monitor_config->enabled)
    {
        elog(WARNING, "pg_ram: PostgreSQL monitor not initialized");
        return false;
    }
    
    if (health_out == NULL)
        return false;
        
    /* Initialize health structure */
    memset(health_out, 0, sizeof(postgresql_health_t));
    health_out->last_health_check = GetCurrentTimestamp();
    
    /* Basic PostgreSQL status checks */
    health_out->is_running = (MyProcPid > 0);
    health_out->is_accepting_connections = (!IsPostmasterEnvironment || 
                                           pmState == PM_RUN);
    health_out->is_in_recovery = RecoveryInProgress();
    health_out->is_primary = !health_out->is_in_recovery;
    
    /* Connection monitoring */
    health_out->max_connections = MaxConnections;
    health_out->active_connections = NumBackends;
    if (health_out->max_connections > 0)
    {
        health_out->connection_usage_percentage = 
            (float) health_out->active_connections / health_out->max_connections * 100.0;
    }
    
    /* WAL monitoring */
    if (health_out->is_primary)
    {
        health_out->current_wal_lsn = GetXLogInsertRecPtr();
        health_out->is_streaming_replication_active = (max_wal_senders > 0);
    }
    else
    {
        /* For standby servers */
        health_out->received_lsn = GetWalRcvWriteRecPtr(NULL);
        health_out->replayed_lsn = GetXLogReplayRecPtr(NULL);
        
        /* Calculate replication lag */
        if (!XLogRecPtrIsInvalid(health_out->received_lsn) && 
            !XLogRecPtrIsInvalid(health_out->replayed_lsn))
        {
            health_out->wal_lag_seconds = 
                (int32_t) ((health_out->received_lsn - health_out->replayed_lsn) / 
                          (1024 * 1024));  /* Rough estimate */
        }
    }
    
    /* Buffer pool monitoring */
    if (NBuffers > 0)
    {
        health_out->shared_buffers_total = NBuffers;
        health_out->shared_buffers_used = BgWriterStats.buf_written_backend;
        health_out->buffer_hit_ratio = 
            (float) BgWriterStats.buf_hits / 
            (BgWriterStats.buf_hits + BgWriterStats.buf_reads) * 100.0;
    }
    
    /* Background writer activity */
    health_out->background_writer_activity = BgWriterStats.buf_written_backend;
    
    /* Calculate overall health score */
    health_out->overall_health_score = postgresql_monitor_get_health_score();
    
    /* Set status message */
    if (health_out->overall_health_score >= 0.9)
        snprintf(health_out->status_message, sizeof(health_out->status_message), 
                "PostgreSQL is healthy and performing well");
    else if (health_out->overall_health_score >= 0.7)
        snprintf(health_out->status_message, sizeof(health_out->status_message), 
                "PostgreSQL is operational with minor issues");
    else if (health_out->overall_health_score >= 0.5)
        snprintf(health_out->status_message, sizeof(health_out->status_message), 
                "PostgreSQL has performance issues that need attention");
    else
        snprintf(health_out->status_message, sizeof(health_out->status_message), 
                "PostgreSQL has critical issues requiring immediate attention");
    
    return true;
}

bool
postgresql_monitor_is_healthy(void)
{
    postgresql_health_t health;
    
    if (!postgresql_monitor_health_check(&health))
        return false;
        
    return (health.overall_health_score >= 
            postgresql_monitor_config->health_threshold);
}

float
postgresql_monitor_get_health_score(void)
{
    float score = 0.0;
    int factors = 0;
    
    /* Connection health (20% weight) */
    if (MaxConnections > 0)
    {
        float connection_ratio = (float) NumBackends / MaxConnections;
        if (connection_ratio < 0.8)
            score += 0.2;
        else if (connection_ratio < 0.9)
            score += 0.1;
        factors++;
    }
    
    /* Replication health (30% weight) */
    if (postgresql_monitor_is_replication_healthy())
    {
        score += 0.3;
    }
    factors++;
    
    /* WAL health (25% weight) */
    if (postgresql_monitor_check_wal_health())
    {
        score += 0.25;
    }
    factors++;
    
    /* Performance metrics (25% weight) */
    if (postgresql_monitor_check_performance_metrics())
    {
        score += 0.25;
    }
    factors++;
    
    return (factors > 0) ? score : 0.0;
}

bool
postgresql_monitor_can_accept_writes(void)
{
    postgresql_health_t health;
    
    if (!postgresql_monitor_health_check(&health))
        return false;
        
    return (health.is_running && 
            health.is_accepting_connections && 
            health.is_primary &&
            health.connection_usage_percentage < 95.0);
}

bool
postgresql_monitor_is_replication_healthy(void)
{
    if (RecoveryInProgress())
    {
        /* For standby servers, check replication lag */
        XLogRecPtr received_lsn = GetWalRcvWriteRecPtr(NULL);
        XLogRecPtr replayed_lsn = GetXLogReplayRecPtr(NULL);
        
        if (XLogRecPtrIsInvalid(received_lsn) || XLogRecPtrIsInvalid(replayed_lsn))
            return false;
            
        /* Consider healthy if lag is less than 64MB */
        return ((received_lsn - replayed_lsn) < (64 * 1024 * 1024));
    }
    else
    {
        /* For primary servers, check if streaming replication is active */
        return (max_wal_senders > 0);
    }
}

bool
postgresql_monitor_check_wal_health(void)
{
    /* Check WAL archiving status and disk space */
    /* This is a simplified check - in reality you'd check more details */
    return true;  /* Assume healthy for now */
}

bool
postgresql_monitor_check_connections(void)
{
    if (MaxConnections <= 0)
        return false;
        
    float usage = (float) NumBackends / MaxConnections;
    return (usage < 0.9);  /* Healthy if less than 90% used */
}

bool
postgresql_monitor_check_disk_space(void)
{
    /* Check data directory disk space */
    /* This would require system calls to check actual disk usage */
    return true;  /* Assume healthy for now */
}

bool
postgresql_monitor_check_performance_metrics(void)
{
    /* Check buffer hit ratio, checkpoint frequency, etc. */
    if (BgWriterStats.buf_hits + BgWriterStats.buf_reads > 0)
    {
        float hit_ratio = (float) BgWriterStats.buf_hits / 
                         (BgWriterStats.buf_hits + BgWriterStats.buf_reads);
        return (hit_ratio > 0.95);  /* 95% hit ratio is good */
    }
    
    return true;
}

char*
postgresql_monitor_get_status_summary(void)
{
    postgresql_health_t health;
    char *summary;
    
    if (!postgresql_monitor_health_check(&health))
        return pstrdup("PostgreSQL monitor failed to get status");
        
    summary = (char *) palloc(1024);
    snprintf(summary, 1024,
        "PostgreSQL Status: %s | Health Score: %.2f | "
        "Role: %s | Connections: %d/%d (%.1f%%) | "
        "Replication: %s | WAL LSN: %X/%X",
        health.is_running ? "Running" : "Stopped",
        health.overall_health_score,
        health.is_primary ? "Primary" : "Standby",
        health.active_connections,
        health.max_connections,
        health.connection_usage_percentage,
        health.is_streaming_replication_active ? "Active" : "Inactive",
        (uint32) (health.current_wal_lsn >> 32),
        (uint32) health.current_wal_lsn);
        
    return summary;
}

bool
postgresql_monitor_get_replication_status(char *status_out, size_t status_size)
{
    if (status_out == NULL || status_size == 0)
        return false;
        
    if (RecoveryInProgress())
    {
        XLogRecPtr received_lsn = GetWalRcvWriteRecPtr(NULL);
        XLogRecPtr replayed_lsn = GetXLogReplayRecPtr(NULL);
        
        snprintf(status_out, status_size,
            "Standby server - Received: %X/%X, Replayed: %X/%X, Lag: %ld bytes",
            (uint32) (received_lsn >> 32), (uint32) received_lsn,
            (uint32) (replayed_lsn >> 32), (uint32) replayed_lsn,
            (long) (received_lsn - replayed_lsn));
    }
    else
    {
        XLogRecPtr current_lsn = GetXLogInsertRecPtr();
        snprintf(status_out, status_size,
            "Primary server - Current WAL LSN: %X/%X, Active senders: %d",
            (uint32) (current_lsn >> 32), (uint32) current_lsn,
            max_wal_senders);
    }
    
    return true;
}

bool
postgresql_monitor_get_connection_info(char *info_out, size_t info_size)
{
    if (info_out == NULL || info_size == 0)
        return false;
        
    snprintf(info_out, info_size,
        "Connections: %d active, %d maximum (%.1f%% usage)",
        NumBackends, MaxConnections,
        MaxConnections > 0 ? (float) NumBackends / MaxConnections * 100.0 : 0.0);
        
    return true;
}
