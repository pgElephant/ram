/*-------------------------------------------------------------------------
 *
 * ramd_config_reload.h
 *		PostgreSQL Auto-Failover Daemon - Dynamic Configuration Reload
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_CONFIG_RELOAD_H
#define RAMD_CONFIG_RELOAD_H

#include "ramd.h"
#include "ramd_config.h"

/* Configuration reload status */
typedef enum
{
	RAMD_CONFIG_RELOAD_SUCCESS,
	RAMD_CONFIG_RELOAD_FAILED,
	RAMD_CONFIG_RELOAD_PARTIAL,
	RAMD_CONFIG_RELOAD_NO_CHANGES
} ramd_config_reload_status_t;

/* Configuration change types */
typedef enum
{
	RAMD_CONFIG_CHANGE_NONE = 0,
	RAMD_CONFIG_CHANGE_LOGGING = (1 << 0),
	RAMD_CONFIG_CHANGE_MONITORING = (1 << 1),
	RAMD_CONFIG_CHANGE_FAILOVER = (1 << 2),
	RAMD_CONFIG_CHANGE_POSTGRESQL = (1 << 3),
	RAMD_CONFIG_CHANGE_CLUSTER = (1 << 4),
	RAMD_CONFIG_CHANGE_SYNCHRONOUS_REP = (1 << 5),
	RAMD_CONFIG_CHANGE_HTTP_API = (1 << 6),
	RAMD_CONFIG_CHANGE_MAINTENANCE = (1 << 7)
} ramd_config_change_flags_t;

/* Configuration reload result */
typedef struct ramd_config_reload_result_t
{
	ramd_config_reload_status_t status;
	ramd_config_change_flags_t changes_detected;
	ramd_config_change_flags_t changes_applied;
	time_t reload_time;
	char error_message[256];
	char warnings[512];
} ramd_config_reload_result_t;

/* Function prototypes */
bool ramd_config_reload_init(void);
void ramd_config_reload_cleanup(void);

/* Configuration reload operations */
bool ramd_config_reload_from_file(const char* config_file,
                                  ramd_config_reload_result_t* result);
bool ramd_config_reload_signal_handler(void);

/* Configuration comparison and validation */
ramd_config_change_flags_t ramd_config_compare(const ramd_config_t* old_config,
                                               const ramd_config_t* new_config);
bool ramd_config_validate_reload(const ramd_config_t* new_config,
                                 char* error_msg, size_t error_size);

/* Hot reload handlers for different subsystems */
bool ramd_config_reload_logging(const ramd_config_t* old_config,
                                const ramd_config_t* new_config);
bool ramd_config_reload_monitoring(const ramd_config_t* old_config,
                                   const ramd_config_t* new_config);
bool ramd_config_reload_failover(const ramd_config_t* old_config,
                                 const ramd_config_t* new_config);
bool ramd_config_reload_postgresql(const ramd_config_t* old_config,
                                   const ramd_config_t* new_config);
bool ramd_config_reload_cluster(const ramd_config_t* old_config,
                                const ramd_config_t* new_config);
bool ramd_config_reload_sync_replication(const ramd_config_t* old_config,
                                         const ramd_config_t* new_config);
bool ramd_config_reload_http_api(const ramd_config_t* old_config,
                                 const ramd_config_t* new_config);
bool ramd_config_reload_maintenance(const ramd_config_t* old_config,
                                    const ramd_config_t* new_config);

/* Signal handling for configuration reload */
void ramd_config_reload_signal_setup(void);
void ramd_config_reload_signal_sighup(int sig);

/* Utility functions */
const char*
ramd_config_reload_status_to_string(ramd_config_reload_status_t status);
const char* ramd_config_change_flags_to_string(ramd_config_change_flags_t flags,
                                               char* buffer, size_t size);
bool ramd_config_backup_current(const char* backup_path);
bool ramd_config_restore_backup(const char* backup_path);

#endif /* RAMD_CONFIG_RELOAD_H */
