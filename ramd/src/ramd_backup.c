/*
 * PostgreSQL RAM Daemon - Backup Integration Module
 * 
 * This module provides integration with backup tools like pgBackRest and Barman
 * for automated backup and restore operations.
 */

#include "ramd_backup.h"
#include "ramd_logging.h"
#include "ramd_config.h"
#include "ramd_postgresql.h"
#include "ramd_http_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

/* Backup tool types */
typedef enum {
    RAMD_BACKUP_TOOL_PGBACKREST,
    RAMD_BACKUP_TOOL_BARMAN,
    RAMD_BACKUP_TOOL_CUSTOM
} ramd_backup_tool_type_t;

/* Backup tool configuration */
typedef struct {
    ramd_backup_tool_type_t type;
    char name[64];
    char command[512];
    char config_file[256];
    char backup_path[512];
    char restore_path[512];
    bool enabled;
    int retention_days;
    char schedule[64];
    char last_backup[32];
    char last_restore[32];
    int backup_count;
    int restore_count;
} ramd_backup_tool_t;

/* Backup job status */
typedef struct {
    int job_id;
    char tool_name[64];
    char operation[32];  /* "backup", "restore", "verify" */
    char status[32];     /* "running", "completed", "failed" */
    time_t start_time;
    time_t end_time;
    int exit_code;
    char error_message[512];
    char backup_name[128];
    char progress[256];
} ramd_backup_job_t;

/* Global backup context */
static ramd_backup_tool_t g_backup_tools[RAMD_MAX_BACKUP_TOOLS];
static int g_backup_tool_count = 0;
static ramd_backup_job_t g_backup_jobs[RAMD_MAX_BACKUP_JOBS];
static int g_backup_job_count = 0;
static pthread_mutex_t g_backup_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations */
static bool ramd_backup_validate_tool(ramd_backup_tool_t* tool);
static bool ramd_backup_execute_command(const char* command, char* output, size_t output_size);
static int ramd_backup_get_next_job_id(void);
static void* ramd_backup_job_worker(void* arg);
static bool ramd_backup_pgbackrest_init(ramd_backup_tool_t* tool);
static bool ramd_backup_barman_init(ramd_backup_tool_t* tool);
static bool ramd_backup_custom_init(ramd_backup_tool_t* tool);

/*
 * Initialize backup system
 */
bool
ramd_backup_init(void)
{
    ramd_log_info("Initializing backup system");
    
    /* Initialize backup tools from configuration */
    for (int i = 0; i < g_ramd_daemon->config.backup_tool_count; i++) {
        ramd_backup_tool_t* tool = &g_backup_tools[g_backup_tool_count];
        
        /* Copy tool configuration */
        strncpy(tool->name, g_ramd_daemon->config.backup_tools[i].name, sizeof(tool->name) - 1);
        tool->type = g_ramd_daemon->config.backup_tools[i].type;
        strncpy(tool->command, g_ramd_daemon->config.backup_tools[i].command, sizeof(tool->command) - 1);
        strncpy(tool->config_file, g_ramd_daemon->config.backup_tools[i].config_file, sizeof(tool->config_file) - 1);
        strncpy(tool->backup_path, g_ramd_daemon->config.backup_tools[i].backup_path, sizeof(tool->backup_path) - 1);
        strncpy(tool->restore_path, g_ramd_daemon->config.backup_tools[i].restore_path, sizeof(tool->restore_path) - 1);
        tool->enabled = g_ramd_daemon->config.backup_tools[i].enabled;
        tool->retention_days = g_ramd_daemon->config.backup_tools[i].retention_days;
        strncpy(tool->schedule, g_ramd_daemon->config.backup_tools[i].schedule, sizeof(tool->schedule) - 1);
        
        /* Initialize tool-specific configuration */
        bool init_success = false;
        switch (tool->type) {
            case RAMD_BACKUP_TOOL_PGBACKREST:
                init_success = ramd_backup_pgbackrest_init(tool);
                break;
            case RAMD_BACKUP_TOOL_BARMAN:
                init_success = ramd_backup_barman_init(tool);
                break;
            case RAMD_BACKUP_TOOL_CUSTOM:
                init_success = ramd_backup_custom_init(tool);
                break;
            default:
                ramd_log_error("Unknown backup tool type: %d", tool->type);
                continue;
        }
        
        if (init_success && tool->enabled) {
            g_backup_tool_count++;
            ramd_log_success("Backup tool initialized: %s", tool->name);
        } else {
            ramd_log_warning("Failed to initialize backup tool: %s", tool->name);
        }
    }
    
    ramd_log_success("Backup system initialized with %d tools", g_backup_tool_count);
    return true;
}

/*
 * Create a backup using the specified tool
 */
bool
ramd_backup_create(const char* tool_name, const char* backup_name, char* error_message, size_t error_size)
{
    pthread_mutex_lock(&g_backup_mutex);
    
    /* Find the backup tool */
    ramd_backup_tool_t* tool = NULL;
    for (int i = 0; i < g_backup_tool_count; i++) {
        if (strcmp(g_backup_tools[i].name, tool_name) == 0) {
            tool = &g_backup_tools[i];
            break;
        }
    }
    
    if (!tool) {
        snprintf(error_message, error_size, "Backup tool not found: %s", tool_name);
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    
    if (!tool->enabled) {
        snprintf(error_message, error_size, "Backup tool is disabled: %s", tool_name);
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    
    /* Create backup job */
    if (g_backup_job_count >= RAMD_MAX_BACKUP_JOBS) {
        snprintf(error_message, error_size, "Maximum number of backup jobs reached");
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    
    ramd_backup_job_t* job = &g_backup_jobs[g_backup_job_count];
    job->job_id = ramd_backup_get_next_job_id();
    strncpy(job->tool_name, tool_name, sizeof(job->tool_name) - 1);
    strncpy(job->operation, "backup", sizeof(job->operation) - 1);
    strncpy(job->status, "running", sizeof(job->status) - 1);
    job->start_time = time(NULL);
    job->end_time = 0;
    job->exit_code = -1;
    strncpy(job->backup_name, backup_name ? backup_name : "auto", sizeof(job->backup_name) - 1);
    strcpy(job->error_message, "");
    strcpy(job->progress, "Starting backup...");
    
    g_backup_job_count++;
    
    pthread_mutex_unlock(&g_backup_mutex);
    
    /* Start backup job in background */
    pthread_t thread;
    if (pthread_create(&thread, NULL, ramd_backup_job_worker, job) != 0) {
        snprintf(error_message, error_size, "Failed to start backup job thread");
        return false;
    }
    
    pthread_detach(thread);
    
    ramd_log_operation("Backup job started: %s (ID: %d)", tool_name, job->job_id);
    return true;
}

/*
 * Restore from backup using the specified tool
 */
bool
ramd_backup_restore(const char* tool_name, const char* backup_name, char* error_message, size_t error_size)
{
    pthread_mutex_lock(&g_backup_mutex);
    
    /* Find the backup tool */
    ramd_backup_tool_t* tool = NULL;
    for (int i = 0; i < g_backup_tool_count; i++) {
        if (strcmp(g_backup_tools[i].name, tool_name) == 0) {
            tool = &g_backup_tools[i];
            break;
        }
    }
    
    if (!tool) {
        snprintf(error_message, error_size, "Backup tool not found: %s", tool_name);
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    
    if (!tool->enabled) {
        snprintf(error_message, error_size, "Backup tool is disabled: %s", tool_name);
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    
    /* Create restore job */
    if (g_backup_job_count >= RAMD_MAX_BACKUP_JOBS) {
        snprintf(error_message, error_size, "Maximum number of backup jobs reached");
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    
    ramd_backup_job_t* job = &g_backup_jobs[g_backup_job_count];
    job->job_id = ramd_backup_get_next_job_id();
    strncpy(job->tool_name, tool_name, sizeof(job->tool_name) - 1);
    strncpy(job->operation, "restore", sizeof(job->operation) - 1);
    strncpy(job->status, "running", sizeof(job->status) - 1);
    job->start_time = time(NULL);
    job->end_time = 0;
    job->exit_code = -1;
    strncpy(job->backup_name, backup_name, sizeof(job->backup_name) - 1);
    strcpy(job->error_message, "");
    strcpy(job->progress, "Starting restore...");
    
    g_backup_job_count++;
    
    pthread_mutex_unlock(&g_backup_mutex);
    
    /* Start restore job in background */
    pthread_t thread;
    if (pthread_create(&thread, NULL, ramd_backup_job_worker, job) != 0) {
        snprintf(error_message, error_size, "Failed to start restore job thread");
        return false;
    }
    
    pthread_detach(thread);
    
    ramd_log_operation("Restore job started: %s (ID: %d)", tool_name, job->job_id);
    return true;
}

/*
 * List available backups
 */
bool
ramd_backup_list(const char* tool_name, char* output, size_t output_size)
{
    pthread_mutex_lock(&g_backup_mutex);
    
    /* Find the backup tool */
    ramd_backup_tool_t* tool = NULL;
    for (int i = 0; i < g_backup_tool_count; i++) {
        if (strcmp(g_backup_tools[i].name, tool_name) == 0) {
            tool = &g_backup_tools[i];
            break;
        }
    }
    
    if (!tool) {
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    
    /* Build list command based on tool type */
    char command[1024];
    switch (tool->type) {
        case RAMD_BACKUP_TOOL_PGBACKREST:
            snprintf(command, sizeof(command), "pgbackrest info --output=json");
            break;
        case RAMD_BACKUP_TOOL_BARMAN:
            snprintf(command, sizeof(command), "barman list-backup %s --json", tool->name);
            break;
        case RAMD_BACKUP_TOOL_CUSTOM:
            snprintf(command, sizeof(command), "%s list", tool->command);
            break;
        default:
            pthread_mutex_unlock(&g_backup_mutex);
            return false;
    }
    
    pthread_mutex_unlock(&g_backup_mutex);
    
    /* Execute command and return output */
    return ramd_backup_execute_command(command, output, output_size);
}

/*
 * Get backup job status
 */
bool
ramd_backup_get_job_status(int job_id, ramd_backup_job_t* job)
{
    pthread_mutex_lock(&g_backup_mutex);
    
    for (int i = 0; i < g_backup_job_count; i++) {
        if (g_backup_jobs[i].job_id == job_id) {
            *job = g_backup_jobs[i];
            pthread_mutex_unlock(&g_backup_mutex);
            return true;
        }
    }
    
    pthread_mutex_unlock(&g_backup_mutex);
    return false;
}

/*
 * List all backup jobs
 */
bool
ramd_backup_list_jobs(char* output, size_t output_size)
{
    pthread_mutex_lock(&g_backup_mutex);
    
    char* ptr = output;
    size_t remaining = output_size;
    int written = 0;
    
    written = snprintf(ptr, remaining, "{\"jobs\":[");
    if (written < 0 || (size_t)written >= remaining) {
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    ptr += written;
    remaining -= written;
    
    for (int i = 0; i < g_backup_job_count; i++) {
        ramd_backup_job_t* job = &g_backup_jobs[i];
        
        if (i > 0) {
            written = snprintf(ptr, remaining, ",");
            if (written < 0 || (size_t)written >= remaining) {
                pthread_mutex_unlock(&g_backup_mutex);
                return false;
            }
            ptr += written;
            remaining -= written;
        }
        
        written = snprintf(ptr, remaining,
            "{\"job_id\":%d,\"tool_name\":\"%s\",\"operation\":\"%s\","
            "\"status\":\"%s\",\"start_time\":%ld,\"end_time\":%ld,"
            "\"exit_code\":%d,\"backup_name\":\"%s\",\"progress\":\"%s\"}",
            job->job_id, job->tool_name, job->operation, job->status,
            job->start_time, job->end_time, job->exit_code,
            job->backup_name, job->progress);
        
        if (written < 0 || (size_t)written >= remaining) {
            pthread_mutex_unlock(&g_backup_mutex);
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    written = snprintf(ptr, remaining, "]}");
    if (written < 0 || (size_t)written >= remaining) {
        pthread_mutex_unlock(&g_backup_mutex);
        return false;
    }
    
    pthread_mutex_unlock(&g_backup_mutex);
    return true;
}

/*
 * Initialize pgBackRest tool
 */
static bool
ramd_backup_pgbackrest_init(ramd_backup_tool_t* tool)
{
    /* Validate pgBackRest installation */
    char command[512];
    snprintf(command, sizeof(command), "pgbackrest version");
    
    char output[256];
    if (!ramd_backup_execute_command(command, output, sizeof(output))) {
        ramd_log_error("pgBackRest not found or not working");
        return false;
    }
    
    /* Set default commands if not provided */
    if (strlen(tool->command) == 0) {
        snprintf(tool->command, sizeof(tool->command), "pgbackrest");
    }
    
    if (strlen(tool->backup_path) == 0) {
        snprintf(tool->backup_path, sizeof(tool->backup_path), "/var/lib/pgbackrest");
    }
    
    if (strlen(tool->restore_path) == 0) {
        snprintf(tool->restore_path, sizeof(tool->restore_path), "/var/lib/postgresql/restore");
    }
    
    /* Create backup directory if it doesn't exist */
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", tool->backup_path);
    system(mkdir_cmd);
    
    ramd_log_debug("pgBackRest tool initialized: %s", tool->name);
    return true;
}

/*
 * Initialize Barman tool
 */
static bool
ramd_backup_barman_init(ramd_backup_tool_t* tool)
{
    /* Validate Barman installation */
    char command[512];
    snprintf(command, sizeof(command), "barman --version");
    
    char output[256];
    if (!ramd_backup_execute_command(command, output, sizeof(output))) {
        ramd_log_error("Barman not found or not working");
        return false;
    }
    
    /* Set default commands if not provided */
    if (strlen(tool->command) == 0) {
        snprintf(tool->command, sizeof(tool->command), "barman");
    }
    
    if (strlen(tool->backup_path) == 0) {
        snprintf(tool->backup_path, sizeof(tool->backup_path), "/var/lib/barman");
    }
    
    if (strlen(tool->restore_path) == 0) {
        snprintf(tool->restore_path, sizeof(tool->restore_path), "/var/lib/postgresql/restore");
    }
    
    /* Create backup directory if it doesn't exist */
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", tool->backup_path);
    system(mkdir_cmd);
    
    ramd_log_debug("Barman tool initialized: %s", tool->name);
    return true;
}

/*
 * Initialize custom backup tool
 */
static bool
ramd_backup_custom_init(ramd_backup_tool_t* tool)
{
    /* Validate custom tool command */
    if (strlen(tool->command) == 0) {
        ramd_log_error("Custom backup tool command not specified");
        return false;
    }
    
    /* Test if command exists and is executable */
    char test_cmd[512];
    snprintf(test_cmd, sizeof(test_cmd), "which %s", tool->command);
    
    char output[256];
    if (!ramd_backup_execute_command(test_cmd, output, sizeof(output))) {
        ramd_log_error("Custom backup tool not found: %s", tool->command);
        return false;
    }
    
    /* Set default paths if not provided */
    if (strlen(tool->backup_path) == 0) {
        snprintf(tool->backup_path, sizeof(tool->backup_path), "/var/lib/backups");
    }
    
    if (strlen(tool->restore_path) == 0) {
        snprintf(tool->restore_path, sizeof(tool->restore_path), "/var/lib/postgresql/restore");
    }
    
    /* Create backup directory if it doesn't exist */
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", tool->backup_path);
    system(mkdir_cmd);
    
    ramd_log_debug("Custom backup tool initialized: %s", tool->name);
    return true;
}

/*
 * Execute backup command
 */
static bool
ramd_backup_execute_command(const char* command, char* output, size_t output_size)
{
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        ramd_log_error("Failed to execute command: %s", command);
        return false;
    }
    
    size_t total_read = 0;
    char buffer[1024];
    
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t len = strlen(buffer);
        if (total_read + len < output_size - 1) {
            strncpy(output + total_read, buffer, len);
            total_read += len;
        } else {
            break;
        }
    }
    
    output[total_read] = '\0';
    
    int exit_code = pclose(pipe);
    return (exit_code == 0);
}

/*
 * Get next job ID
 */
static int
ramd_backup_get_next_job_id(void)
{
    static int next_job_id = 1;
    return next_job_id++;
}

/*
 * Backup job worker thread
 */
static void*
ramd_backup_job_worker(void* arg)
{
    ramd_backup_job_t* job = (ramd_backup_job_t*)arg;
    
    /* Find the backup tool */
    ramd_backup_tool_t* tool = NULL;
    for (int i = 0; i < g_backup_tool_count; i++) {
        if (strcmp(g_backup_tools[i].name, job->tool_name) == 0) {
            tool = &g_backup_tools[i];
            break;
        }
    }
    
    if (!tool) {
        pthread_mutex_lock(&g_backup_mutex);
        strcpy(job->status, "failed");
        strcpy(job->error_message, "Backup tool not found");
        job->end_time = time(NULL);
        job->exit_code = 1;
        pthread_mutex_unlock(&g_backup_mutex);
        return NULL;
    }
    
    /* Build command based on operation */
    char command[1024];
    if (strcmp(job->operation, "backup") == 0) {
        switch (tool->type) {
            case RAMD_BACKUP_TOOL_PGBACKREST:
                snprintf(command, sizeof(command), "pgbackrest backup --stanza=main");
                break;
            case RAMD_BACKUP_TOOL_BARMAN:
                snprintf(command, sizeof(command), "barman backup %s", tool->name);
                break;
            case RAMD_BACKUP_TOOL_CUSTOM:
                snprintf(command, sizeof(command), "%s backup %s", tool->command, job->backup_name);
                break;
        }
    } else if (strcmp(job->operation, "restore") == 0) {
        switch (tool->type) {
            case RAMD_BACKUP_TOOL_PGBACKREST:
                snprintf(command, sizeof(command), "pgbackrest restore --stanza=main --target=%s", tool->restore_path);
                break;
            case RAMD_BACKUP_TOOL_BARMAN:
                snprintf(command, sizeof(command), "barman recover %s %s %s", tool->name, job->backup_name, tool->restore_path);
                break;
            case RAMD_BACKUP_TOOL_CUSTOM:
                snprintf(command, sizeof(command), "%s restore %s %s", tool->command, job->backup_name, tool->restore_path);
                break;
        }
    }
    
    /* Execute command */
    char output[4096];
    bool success = ramd_backup_execute_command(command, output, sizeof(output));
    
    /* Update job status */
    pthread_mutex_lock(&g_backup_mutex);
    job->end_time = time(NULL);
    if (success) {
        strcpy(job->status, "completed");
        job->exit_code = 0;
        strcpy(job->progress, "Operation completed successfully");
    } else {
        strcpy(job->status, "failed");
        job->exit_code = 1;
        strncpy(job->error_message, output, sizeof(job->error_message) - 1);
        strcpy(job->progress, "Operation failed");
    }
    pthread_mutex_unlock(&g_backup_mutex);
    
    /* Log completion */
    if (success) {
        ramd_log_success("Backup job completed: %s (ID: %d)", job->tool_name, job->job_id);
    } else {
        ramd_log_failure("Backup job failed: %s (ID: %d) - %s", job->tool_name, job->job_id, job->error_message);
    }
    
    return NULL;
}

/*
 * Validate backup tool configuration
 */
static bool
ramd_backup_validate_tool(ramd_backup_tool_t* tool)
{
    if (strlen(tool->name) == 0) {
        ramd_log_error("Backup tool name is required");
        return false;
    }
    
    if (strlen(tool->command) == 0) {
        ramd_log_error("Backup tool command is required");
        return false;
    }
    
    if (tool->retention_days <= 0) {
        ramd_log_error("Backup retention days must be positive");
        return false;
    }
    
    return true;
}
