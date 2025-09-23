/*
 * PostgreSQL RAM Daemon - Backup Integration Header
 * 
 * This header defines the backup integration API for pgBackRest, Barman,
 * and custom backup tools.
 */

#ifndef RAMD_BACKUP_H
#define RAMD_BACKUP_H

#include "ramd_common.h"

/* Maximum number of backup tools */
#define RAMD_MAX_BACKUP_TOOLS 10

/* Maximum number of concurrent backup jobs */
#define RAMD_MAX_BACKUP_JOBS 50

/* Backup tool types */
typedef enum {
    RAMD_BACKUP_TOOL_PGBACKREST = 1,
    RAMD_BACKUP_TOOL_BARMAN = 2,
    RAMD_BACKUP_TOOL_CUSTOM = 3
} ramd_backup_tool_type_t;

/* Backup tool configuration structure */
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
} ramd_backup_tool_config_t;

/* Backup job status structure */
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

/* Backup statistics structure */
typedef struct {
    int total_backups;
    int successful_backups;
    int failed_backups;
    int total_restores;
    int successful_restores;
    int failed_restores;
    time_t last_backup_time;
    time_t last_restore_time;
    char last_backup_name[128];
    char last_restore_name[128];
} ramd_backup_stats_t;

/* Function prototypes */

/*
 * Initialize backup system
 */
bool ramd_backup_init(void);

/*
 * Create a backup using the specified tool
 */
bool ramd_backup_create(const char* tool_name, const char* backup_name, 
                       char* error_message, size_t error_size);

/*
 * Restore from backup using the specified tool
 */
bool ramd_backup_restore(const char* tool_name, const char* backup_name, 
                        char* error_message, size_t error_size);

/*
 * List available backups
 */
bool ramd_backup_list(const char* tool_name, char* output, size_t output_size);

/*
 * Get backup job status
 */
bool ramd_backup_get_job_status(int job_id, ramd_backup_job_t* job);

/*
 * List all backup jobs
 */
bool ramd_backup_list_jobs(char* output, size_t output_size);

/*
 * Get backup statistics
 */
bool ramd_backup_get_stats(ramd_backup_stats_t* stats);

/*
 * Verify backup integrity
 */
bool ramd_backup_verify(const char* tool_name, const char* backup_name, 
                       char* error_message, size_t error_size);

/*
 * Clean up old backups based on retention policy
 */
bool ramd_backup_cleanup(const char* tool_name, char* error_message, size_t error_size);

/*
 * Schedule backup based on cron expression
 */
bool ramd_backup_schedule(const char* tool_name, const char* schedule, 
                         char* error_message, size_t error_size);

/*
 * Cancel running backup job
 */
bool ramd_backup_cancel_job(int job_id, char* error_message, size_t error_size);

/*
 * Get backup tool information
 */
bool ramd_backup_get_tool_info(const char* tool_name, ramd_backup_tool_config_t* tool_info);

/*
 * List all configured backup tools
 */
bool ramd_backup_list_tools(char* output, size_t output_size);

/*
 * Enable/disable backup tool
 */
bool ramd_backup_set_tool_enabled(const char* tool_name, bool enabled, 
                                 char* error_message, size_t error_size);

/*
 * Update backup tool configuration
 */
bool ramd_backup_update_tool_config(const char* tool_name, 
                                   const ramd_backup_tool_config_t* config,
                                   char* error_message, size_t error_size);

/*
 * Test backup tool connectivity and configuration
 */
bool ramd_backup_test_tool(const char* tool_name, char* error_message, size_t error_size);

/*
 * Get backup job progress
 */
bool ramd_backup_get_job_progress(int job_id, char* progress, size_t progress_size);

/*
 * Pause backup job
 */
bool ramd_backup_pause_job(int job_id, char* error_message, size_t error_size);

/*
 * Resume backup job
 */
bool ramd_backup_resume_job(int job_id, char* error_message, size_t error_size);

/*
 * Get backup job logs
 */
bool ramd_backup_get_job_logs(int job_id, char* logs, size_t logs_size);

/*
 * Set backup tool priority
 */
bool ramd_backup_set_tool_priority(const char* tool_name, int priority, 
                                  char* error_message, size_t error_size);

/*
 * Get backup tool priority
 */
int ramd_backup_get_tool_priority(const char* tool_name);

/*
 * Backup tool health check
 */
bool ramd_backup_health_check(const char* tool_name, char* status, size_t status_size);

/*
 * Get backup tool metrics
 */
bool ramd_backup_get_tool_metrics(const char* tool_name, char* metrics, size_t metrics_size);

/*
 * Export backup configuration
 */
bool ramd_backup_export_config(char* config, size_t config_size);

/*
 * Import backup configuration
 */
bool ramd_backup_import_config(const char* config, char* error_message, size_t error_size);

/*
 * Validate backup tool configuration
 */
bool ramd_backup_validate_config(const ramd_backup_tool_config_t* config, 
                                char* error_message, size_t error_size);

/*
 * Get backup tool capabilities
 */
bool ramd_backup_get_tool_capabilities(const char* tool_name, char* capabilities, size_t capabilities_size);

/*
 * Set backup tool parameters
 */
bool ramd_backup_set_tool_parameters(const char* tool_name, const char* parameters, 
                                    char* error_message, size_t error_size);

/*
 * Get backup tool parameters
 */
bool ramd_backup_get_tool_parameters(const char* tool_name, char* parameters, size_t parameters_size);

/*
 * Backup tool status check
 */
bool ramd_backup_tool_status(const char* tool_name, char* status, size_t status_size);

/*
 * Backup tool version check
 */
bool ramd_backup_tool_version(const char* tool_name, char* version, size_t version_size);

/*
 * Backup tool configuration validation
 */
bool ramd_backup_validate_tool_config(const char* tool_name, char* error_message, size_t error_size);

/*
 * Backup tool configuration reload
 */
bool ramd_backup_reload_tool_config(const char* tool_name, char* error_message, size_t error_size);

/*
 * Backup tool configuration reset
 */
bool ramd_backup_reset_tool_config(const char* tool_name, char* error_message, size_t error_size);

/*
 * Backup tool configuration backup
 */
bool ramd_backup_backup_tool_config(const char* tool_name, const char* backup_path, 
                                   char* error_message, size_t error_size);

/*
 * Backup tool configuration restore
 */
bool ramd_backup_restore_tool_config(const char* tool_name, const char* backup_path, 
                                    char* error_message, size_t error_size);

/*
 * Backup tool configuration export
 */
bool ramd_backup_export_tool_config(const char* tool_name, const char* export_path, 
                                   char* error_message, size_t error_size);

/*
 * Backup tool configuration import
 */
bool ramd_backup_import_tool_config(const char* tool_name, const char* import_path, 
                                   char* error_message, size_t error_size);

/*
 * Backup tool configuration diff
 */
bool ramd_backup_diff_tool_config(const char* tool_name, const char* other_tool_name, 
                                 char* diff, size_t diff_size);

/*
 * Backup tool configuration merge
 */
bool ramd_backup_merge_tool_config(const char* tool_name, const char* other_tool_name, 
                                  char* error_message, size_t error_size);

/*
 * Backup tool configuration sync
 */
bool ramd_backup_sync_tool_config(const char* tool_name, const char* other_tool_name, 
                                 char* error_message, size_t error_size);

/*
 * Backup tool configuration validate
 */
bool ramd_backup_validate_tool_config_file(const char* tool_name, const char* config_file, 
                                          char* error_message, size_t error_size);

/*
 * Backup tool configuration update
 */
bool ramd_backup_update_tool_config_file(const char* tool_name, const char* config_file, 
                                        char* error_message, size_t error_size);

/*
 * Backup tool configuration reload
 */
bool ramd_backup_reload_tool_config_file(const char* tool_name, const char* config_file, 
                                        char* error_message, size_t error_size);

/*
 * Backup tool configuration backup
 */
bool ramd_backup_backup_tool_config_file(const char* tool_name, const char* config_file, 
                                        const char* backup_path, char* error_message, size_t error_size);

/*
 * Backup tool configuration restore
 */
bool ramd_backup_restore_tool_config_file(const char* tool_name, const char* config_file, 
                                         const char* backup_path, char* error_message, size_t error_size);

/*
 * Backup tool configuration export
 */
bool ramd_backup_export_tool_config_file(const char* tool_name, const char* config_file, 
                                        const char* export_path, char* error_message, size_t error_size);

/*
 * Backup tool configuration import
 */
bool ramd_backup_import_tool_config_file(const char* tool_name, const char* config_file, 
                                        const char* import_path, char* error_message, size_t error_size);

/*
 * Backup tool configuration diff
 */
bool ramd_backup_diff_tool_config_file(const char* tool_name, const char* config_file, 
                                      const char* other_tool_name, const char* other_config_file, 
                                      char* diff, size_t diff_size);

/*
 * Backup tool configuration merge
 */
bool ramd_backup_merge_tool_config_file(const char* tool_name, const char* config_file, 
                                       const char* other_tool_name, const char* other_config_file, 
                                       char* error_message, size_t error_size);

/*
 * Backup tool configuration sync
 */
bool ramd_backup_sync_tool_config_file(const char* tool_name, const char* config_file, 
                                      const char* other_tool_name, const char* other_config_file, 
                                      char* error_message, size_t error_size);

#endif /* RAMD_BACKUP_H */
