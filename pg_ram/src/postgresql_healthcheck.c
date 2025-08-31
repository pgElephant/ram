/*-------------------------------------------------------------------------
 *
 * postgresql_healthcheck.c
 *		PostgreSQL health check implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/elog.h"
#include "utils/timestamp.h"
#include "postgresql_healthcheck.h"
#include "postgresql_monitor.h"

postgresql_healthcheck_config_t *postgresql_healthcheck_config = NULL;

bool
postgresql_healthcheck_init(void)
{
    if (postgresql_healthcheck_config != NULL && postgresql_healthcheck_config->enabled)
        return true;
        
    postgresql_healthcheck_config = (postgresql_healthcheck_config_t *) 
        palloc0(sizeof(postgresql_healthcheck_config_t));
    
    if (postgresql_healthcheck_config == NULL)
    {
        elog(ERROR, "pg_ram: Failed to allocate PostgreSQL healthcheck configuration");
        return false;
    }
    
    /* Initialize configuration */
    postgresql_healthcheck_config->enabled = true;
    postgresql_healthcheck_config->check_frequency_seconds = 60;
    postgresql_healthcheck_config->failure_threshold = 3;
    postgresql_healthcheck_config->recovery_threshold = 2;
    postgresql_healthcheck_config->auto_recovery_enabled = false;
    postgresql_healthcheck_config->alert_on_failure = true;
    
    elog(LOG, "pg_ram: PostgreSQL healthcheck initialized successfully");
    return true;
}

void
postgresql_healthcheck_cleanup(void)
{
    if (postgresql_healthcheck_config == NULL)
        return;
        
    pfree(postgresql_healthcheck_config);
    postgresql_healthcheck_config = NULL;
    
    elog(LOG, "pg_ram: PostgreSQL healthcheck cleanup completed");
}

bool
postgresql_healthcheck_run_all(postgresql_healthcheck_status_t *status_out)
{
    if (postgresql_healthcheck_config == NULL || !postgresql_healthcheck_config->enabled)
        return false;
        
    if (status_out == NULL)
        return false;
        
    /* Initialize status */
    memset(status_out, 0, sizeof(postgresql_healthcheck_status_t));
    status_out->last_check_time = GetCurrentTimestamp();
    
    /* Run individual health checks */
    int check_idx = 0;
    
    /* Database connectivity check */
    if (postgresql_healthcheck_database_connectivity(&status_out->checks[check_idx]))
    {
        status_out->passed_checks++;
    }
    else
    {
        status_out->failed_checks++;
    }
    status_out->total_checks++;
    check_idx++;
    
    /* Replication lag check */
    if (postgresql_healthcheck_replication_lag(&status_out->checks[check_idx]))
    {
        status_out->passed_checks++;
    }
    else
    {
        status_out->failed_checks++;
    }
    status_out->total_checks++;
    check_idx++;
    
    /* Connection limits check */
    if (postgresql_healthcheck_connection_limits(&status_out->checks[check_idx]))
    {
        status_out->passed_checks++;
    }
    else
    {
        status_out->failed_checks++;
    }
    status_out->total_checks++;
    check_idx++;
    
    /* Calculate overall health */
    status_out->overall_healthy = (status_out->failed_checks == 0);
    status_out->overall_score = postgresql_healthcheck_calculate_score(status_out);
    
    return true;
}

bool
postgresql_healthcheck_database_connectivity(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL)
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "database_connectivity");
    strcpy(result_out->description, "Check if PostgreSQL is accepting connections");
    result_out->check_time = GetCurrentTimestamp();
    
    /* Simple connectivity check */
    if (MyProcPid > 0 && !proc_exit_inprogress)
    {
        result_out->passed = true;
        result_out->severity_level = 1;  /* INFO */
        strcpy(result_out->error_message, "Database is accepting connections");
    }
    else
    {
        result_out->passed = false;
        result_out->severity_level = 4;  /* CRITICAL */
        strcpy(result_out->error_message, "Database is not accepting connections");
    }
    
    return result_out->passed;
}

bool
postgresql_healthcheck_replication_lag(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL)
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "replication_lag");
    strcpy(result_out->description, "Check replication lag status");
    result_out->check_time = GetCurrentTimestamp();
    
    /* Use the monitor function to check replication health */
    if (postgresql_monitor_is_replication_healthy())
    {
        result_out->passed = true;
        result_out->severity_level = 1;  /* INFO */
        strcpy(result_out->error_message, "Replication is healthy");
    }
    else
    {
        result_out->passed = false;
        result_out->severity_level = 3;  /* ERROR */
        strcpy(result_out->error_message, "Replication lag is too high");
    }
    
    return result_out->passed;
}

bool
postgresql_healthcheck_connection_limits(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL)
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "connection_limits");
    strcpy(result_out->description, "Check connection usage against limits");
    result_out->check_time = GetCurrentTimestamp();
    
    /* Use the monitor function to check connections */
    if (postgresql_monitor_check_connections())
    {
        result_out->passed = true;
        result_out->severity_level = 1;  /* INFO */
        strcpy(result_out->error_message, "Connection usage is within safe limits");
    }
    else
    {
        result_out->passed = false;
        result_out->severity_level = 2;  /* WARNING */
        strcpy(result_out->error_message, "Connection usage is approaching limits");
    }
    
    return result_out->passed;
}

bool postgresql_healthcheck_disk_space(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL) 
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "disk_space");
    result_out->check_time = GetCurrentTimestamp();
    result_out->severity_level = 4; /* Critical */
    
    /* Check disk space for data directory and WAL directory */
    StringInfoData query;
    initStringInfo(&query);
    appendStringInfo(&query, 
        "SELECT pg_size_pretty(pg_database_size(current_database())) as db_size, "
        "pg_size_pretty(pg_total_relation_size('pg_stat_activity')) as sys_size");
    
    /* Execute query to check disk usage */
    if (SPI_connect() != SPI_OK_CONNECT)
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to connect to database for disk space check");
        return false;
    }
    
    int ret = SPI_execute(query.data, true, 1);
    if (ret == SPI_OK_SELECT && SPI_processed > 0)
    {
        result_out->passed = true;
        strcpy(result_out->details, "Disk space check completed successfully");
    }
    else
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to retrieve disk space information");
    }
    
    SPI_finish();
    pfree(query.data);
    return result_out->passed;
}

bool postgresql_healthcheck_memory_usage(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL) 
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "memory_usage");
    result_out->check_time = GetCurrentTimestamp();
    result_out->severity_level = 3; /* Warning */
    
    /* Check shared buffers and other memory usage */
    if (SPI_connect() != SPI_OK_CONNECT)
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to connect to database for memory check");
        return false;
    }
    
    int ret = SPI_execute("SELECT setting FROM pg_settings WHERE name = 'shared_buffers'", true, 1);
    if (ret == SPI_OK_SELECT && SPI_processed > 0)
    {
        char *shared_buffers = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
        snprintf(result_out->details, sizeof(result_out->details), 
                "Shared buffers: %s", shared_buffers ? shared_buffers : "unknown");
        result_out->passed = true;
    }
    else
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to retrieve memory usage information");
    }
    
    SPI_finish();
    return result_out->passed;
}

bool postgresql_healthcheck_wal_archiving(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL) 
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "wal_archiving");
    result_out->check_time = GetCurrentTimestamp();
    result_out->severity_level = 3; /* Warning */
    
    /* Check WAL archiving status */
    if (SPI_connect() != SPI_OK_CONNECT)
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to connect to database for WAL archiving check");
        return false;
    }
    
    int ret = SPI_execute(
        "SELECT archived_count, failed_count FROM pg_stat_archiver", true, 1);
        
    if (ret == SPI_OK_SELECT && SPI_processed > 0)
    {
        char *archived = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
        char *failed = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2);
        
        int failed_count = failed ? atoi(failed) : 0;
        result_out->passed = (failed_count == 0);
        
        snprintf(result_out->details, sizeof(result_out->details),
                "Archived: %s, Failed: %s", 
                archived ? archived : "0", failed ? failed : "0");
                
        if (!result_out->passed)
            strcpy(result_out->error_message, "WAL archiving failures detected");
    }
    else
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to retrieve WAL archiving status");
    }
    
    SPI_finish();
    return result_out->passed;
}

bool postgresql_healthcheck_vacuum_status(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL) 
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "vacuum_status");
    result_out->check_time = GetCurrentTimestamp();
    result_out->severity_level = 2; /* Info */
    
    /* Check autovacuum status and last vacuum times */
    if (SPI_connect() != SPI_OK_CONNECT)
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to connect to database for vacuum status check");
        return false;
    }
    
    int ret = SPI_execute(
        "SELECT COUNT(*) FROM pg_stat_user_tables "
        "WHERE last_vacuum IS NULL AND last_autovacuum IS NULL", true, 1);
        
    if (ret == SPI_OK_SELECT && SPI_processed > 0)
    {
        char *never_vacuumed = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
        int count = never_vacuumed ? atoi(never_vacuumed) : 0;
        
        result_out->passed = (count < 10); /* Threshold of 10 tables */
        snprintf(result_out->details, sizeof(result_out->details),
                "Tables never vacuumed: %d", count);
                
        if (!result_out->passed)
            strcpy(result_out->error_message, "Too many tables without recent vacuum");
    }
    else
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to retrieve vacuum status");
    }
    
    SPI_finish();
    return result_out->passed;
}

bool postgresql_healthcheck_lock_contention(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL) 
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "lock_contention");
    result_out->check_time = GetCurrentTimestamp();
    result_out->severity_level = 3; /* Warning */
    
    /* Check for blocked queries and lock contention */
    if (SPI_connect() != SPI_OK_CONNECT)
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to connect to database for lock contention check");
        return false;
    }
    
    int ret = SPI_execute(
        "SELECT COUNT(*) FROM pg_stat_activity "
        "WHERE wait_event_type = 'Lock' AND state = 'active'", true, 1);
        
    if (ret == SPI_OK_SELECT && SPI_processed > 0)
    {
        char *blocked_queries = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
        int count = blocked_queries ? atoi(blocked_queries) : 0;
        
        result_out->passed = (count < 5); /* Threshold of 5 blocked queries */
        snprintf(result_out->details, sizeof(result_out->details),
                "Blocked queries: %d", count);
                
        if (!result_out->passed)
            strcpy(result_out->error_message, "High lock contention detected");
    }
    else
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to retrieve lock contention information");
    }
    
    SPI_finish();
    return result_out->passed;
}

bool postgresql_healthcheck_checkpoint_performance(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL) 
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "checkpoint_performance");
    result_out->check_time = GetCurrentTimestamp();
    result_out->severity_level = 2; /* Info */
    
    /* Check checkpoint timing and performance */
    if (SPI_connect() != SPI_OK_CONNECT)
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to connect to database for checkpoint check");
        return false;
    }
    
    int ret = SPI_execute(
        "SELECT checkpoints_timed, checkpoints_req, "
        "checkpoint_write_time, checkpoint_sync_time "
        "FROM pg_stat_bgwriter", true, 1);
        
    if (ret == SPI_OK_SELECT && SPI_processed > 0)
    {
        char *timed = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
        char *requested = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2);
        
        result_out->passed = true;
        snprintf(result_out->details, sizeof(result_out->details),
                "Timed checkpoints: %s, Requested: %s", 
                timed ? timed : "0", requested ? requested : "0");
    }
    else
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to retrieve checkpoint performance data");
    }
    
    SPI_finish();
    return result_out->passed;
}

bool postgresql_healthcheck_backup_status(postgresql_healthcheck_result_t *result_out)
{
    if (result_out == NULL) 
        return false;
        
    memset(result_out, 0, sizeof(postgresql_healthcheck_result_t));
    strcpy(result_out->check_name, "backup_status");
    result_out->check_time = GetCurrentTimestamp();
    result_out->severity_level = 4; /* Critical */
    
    /* Check backup status and recent backup activity */
    if (SPI_connect() != SPI_OK_CONNECT)
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to connect to database for backup status check");
        return false;
    }
    
    /* Check if backup is currently running */
    int ret = SPI_execute(
        "SELECT pg_is_in_backup(), "
        "CASE WHEN pg_is_in_backup() THEN pg_backup_start_time() ELSE NULL END", 
        true, 1);
        
    if (ret == SPI_OK_SELECT && SPI_processed > 0)
    {
        char *in_backup = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
        bool backup_running = (in_backup && strcmp(in_backup, "t") == 0);
        
        result_out->passed = true;
        if (backup_running)
            strcpy(result_out->details, "Backup currently in progress");
        else
            strcpy(result_out->details, "No backup currently running");
    }
    else
    {
        result_out->passed = false;
        strcpy(result_out->error_message, "Failed to retrieve backup status");
    }
    
    SPI_finish();
    return result_out->passed;
}

bool
postgresql_healthcheck_run_single(const char *check_name, postgresql_healthcheck_result_t *result_out)
{
    if (check_name == NULL || result_out == NULL)
        return false;
        
    if (strcmp(check_name, "database_connectivity") == 0)
        return postgresql_healthcheck_database_connectivity(result_out);
    else if (strcmp(check_name, "replication_lag") == 0)
        return postgresql_healthcheck_replication_lag(result_out);
    else if (strcmp(check_name, "connection_limits") == 0)
        return postgresql_healthcheck_connection_limits(result_out);
        
    return false;
}

bool
postgresql_healthcheck_is_critical_failure(const postgresql_healthcheck_status_t *status)
{
    if (status == NULL)
        return true;
        
    for (int i = 0; i < status->total_checks; i++)
    {
        if (!status->checks[i].passed && status->checks[i].severity_level >= 4)
            return true;
    }
    
    return false;
}

float
postgresql_healthcheck_calculate_score(const postgresql_healthcheck_status_t *status)
{
    if (status == NULL || status->total_checks == 0)
        return 0.0;
        
    return (float) status->passed_checks / status->total_checks;
}

char*
postgresql_healthcheck_get_summary(const postgresql_healthcheck_status_t *status)
{
    if (status == NULL)
        return pstrdup("No health check status available");
        
    char *summary = (char *) palloc(512);
    snprintf(summary, 512,
        "Health Check Summary: %d/%d checks passed (%.1f%% success rate) - %s",
        status->passed_checks, status->total_checks,
        status->overall_score * 100.0,
        status->overall_healthy ? "HEALTHY" : "UNHEALTHY");
        
    return summary;
}
