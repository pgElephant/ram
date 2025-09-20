/*-------------------------------------------------------------------------
 *
 * ramd_basebackup.c
 *      PostgreSQL RAM Daemon - Base backup functions
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * This file implements base backup functionality for the ramd daemon.
 * It provides functions to take and manage base backups using pg_basebackup.
 *
 *-------------------------------------------------------------------------
 */

#include "libpq-fe.h"
#include <stdlib.h>
#include <string.h>

#include "ramd_basebackup.h"
#include "ramd_logging.h"

typedef struct BaseBackupOptions
{
    char *label;
    char *target_dir;
    bool progress;
    bool verbose;
    bool write_recovery_conf;
    bool verify_checksums;
    int compression_level;
    int max_rate;
    int wal_method;
    int nthreads;
} BaseBackupOptions;

static int execute_pg_basebackup(const char *conninfo, BaseBackupOptions *options);
static void validate_backup_options(BaseBackupOptions *options);
static char* build_basebackup_command(const char *conninfo, BaseBackupOptions *options);

int 
ramd_take_basebackup(PGconn *conn, const char *target_dir, const char *label)
{
    BaseBackupOptions options;
    char *conninfo;
    int ret;

    memset(&options, 0, sizeof(BaseBackupOptions));
    options.label = (char *)label;
    options.target_dir = (char *)target_dir;
    options.progress = true;
    options.verbose = true;
    options.write_recovery_conf = true;
    options.verify_checksums = true;
    options.compression_level = 9;
    options.max_rate = 0;
    options.wal_method = 1;
    options.nthreads = 4;

    char conninfo_buffer[512];
    snprintf(conninfo_buffer, sizeof(conninfo_buffer), "host=%s port=%s dbname=%s user=%s",
             PQhost(conn), PQport(conn), PQdb(conn), PQuser(conn));
    conninfo = conninfo_buffer;

    validate_backup_options(&options);

    ret = execute_pg_basebackup(conninfo, &options);

    PQfreemem(conninfo);
    return ret;
}

static void
validate_backup_options(BaseBackupOptions *options)
{
    if (!options->target_dir)
    {
        ramd_log_error("Target directory must be specified");
        exit(1);
    }

    if (options->compression_level < 0 || options->compression_level > 9)
    {
        ramd_log_error("Invalid compression level: %d", options->compression_level);
        exit(1);
    }

    if (options->max_rate < 0)
    {
        ramd_log_error("Invalid max rate: %d", options->max_rate);
        exit(1);
    }

    if (options->nthreads < 1)
    {
        ramd_log_error("Invalid number of threads: %d", options->nthreads);
        exit(1);
    }
}

static char*
build_basebackup_command(const char *conninfo, BaseBackupOptions *options)
{
    char *cmd;
    size_t cmd_len = 1024;
    size_t pos = 0;
    
    cmd = malloc(cmd_len);
    if (!cmd)
    {
        ramd_log_error("Failed to allocate memory for command string");
        return NULL;
    }

    pos += (size_t)snprintf(cmd + pos, cmd_len - pos, "pg_basebackup -D %s", options->target_dir);

    if (options->label)
        pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " -l \"%s\"", options->label);
    
    if (options->progress)
        pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " -P");
    
    if (options->verbose)
        pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " -v");
    
    if (options->write_recovery_conf)
        pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " -R");
    
    if (options->verify_checksums)
        pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " --verify-checksums");

    if (options->compression_level > 0)
        pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " -Z %d", options->compression_level);

    if (options->max_rate > 0)
        pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " --max-rate=%d", options->max_rate);

    pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " -X %s", options->wal_method == 1 ? "stream" : "fetch");
    
    if (options->nthreads > 1)
        pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " -T %d", options->nthreads);

    pos += (size_t)snprintf(cmd + pos, cmd_len - pos, " \"%s\"", conninfo);

    return cmd;
}

static int
execute_pg_basebackup(const char *conninfo, BaseBackupOptions *options)
{
    char *cmd;
    int ret;

    cmd = build_basebackup_command(conninfo, options);
    if (!cmd)
    {
        ramd_log_error("Failed to build basebackup command");
        return -1;
    }
    
    ramd_log_info("Executing: %s", cmd);
    
    ret = system(cmd);
    free(cmd);

    if (ret != 0)
    {
        ramd_log_error("pg_basebackup failed with return code %d", ret);
        return -1;
    }

    return 0;
}
