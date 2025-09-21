/*-------------------------------------------------------------------------
 *
 * ramctrl_replication.h
 *		Advanced replication management for RAM system.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_REPLICATION_H
#define RAMCTRL_REPLICATION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "ramctrl_defaults.h"

/* Replication modes */
typedef enum
{
	REPL_MODE_UNKNOWN,           /* Unknown/undefined mode */
	REPL_MODE_ASYNC,             /* Asynchronous replication */
	REPL_MODE_SYNC_REMOTE_WRITE, /* Synchronous with remote_write */
	REPL_MODE_SYNC_REMOTE_APPLY, /* Synchronous with remote_apply */
	REPL_MODE_AUTO               /* Auto-select based on cluster health */
} replication_mode_t;

/* Replication lag thresholds */
typedef struct
{
	int64_t maximum_lag_on_failover; /* Max lag bytes before failover */
	int64_t warning_lag_threshold;   /* Warning threshold in bytes */
	int64_t critical_lag_threshold;  /* Critical threshold in bytes */
	int lag_check_interval_ms;       /* Lag check frequency */
} replication_lag_config_t;

/* WAL-E backup configuration */
typedef struct
{
	char wal_e_path[RAMCTRL_MAX_PATH_LENGTH];        /* Path to WAL-E binary */
	char s3_bucket[RAMCTRL_MAX_HOSTNAME_LENGTH];         /* S3 bucket for backups */
	char s3_prefix[RAMCTRL_MAX_HOSTNAME_LENGTH];         /* S3 prefix for backups */
	char encryption_key[RAMCTRL_MAX_HOSTNAME_LENGTH];    /* Encryption key */
	bool encryption_enabled;     /* Enable backup encryption */
	int backup_retention_days;   /* Days to keep backups */
	int backup_interval_hours;   /* Hours between backups */
	bool point_in_time_recovery; /* Enable PITR */
} wal_e_config_t;

/* Bootstrap script configuration */
typedef struct
{
	char script_path[RAMCTRL_MAX_PATH_LENGTH]; /* Path to bootstrap script */
	char script_args[RAMCTRL_MAX_COMMAND_LENGTH]; /* Script arguments */
	int timeout_seconds;   /* Script timeout */
	bool run_on_primary;   /* Run on primary node */
	bool run_on_standby;   /* Run on standby nodes */
	char environment[RAMCTRL_MAX_COMMAND_LENGTH]; /* Environment variables */
} bootstrap_script_config_t;

/* Replication status */
typedef struct
{
	replication_mode_t mode;   /* Current replication mode */
	int64_t current_lag_bytes; /* Current lag in bytes */
	int current_lag_ms;        /* Current lag in milliseconds */
	time_t last_lag_check;     /* Last lag check timestamp */
	bool is_sync_standby;      /* Is this a sync standby */
	bool is_healthy;           /* Replication health status */
	char application_name[RAMCTRL_MAX_HOSTNAME_LENGTH]; /* Application name */
	char client_addr[RAMCTRL_MAX_HOSTNAME_LENGTH];      /* Client address */
	char state[RAMCTRL_MAX_HOSTNAME_LENGTH];            /* Replication state */
	char sent_lsn[RAMCTRL_MAX_HOSTNAME_LENGTH];         /* Sent LSN */
	char write_lsn[RAMCTRL_MAX_HOSTNAME_LENGTH];        /* Write LSN */
	char flush_lsn[RAMCTRL_MAX_HOSTNAME_LENGTH];        /* Flush LSN */
	char replay_lsn[RAMCTRL_MAX_HOSTNAME_LENGTH];       /* Replay LSN */
} replication_status_t;

/* Replication configuration */
typedef struct
{
	replication_mode_t mode;                    /* Replication mode */
	int max_sync_standbys;                      /* Maximum sync standbys */
	int min_sync_standbys;                      /* Minimum sync standbys */
	char sync_standby_names[RAMCTRL_MAX_COMMAND_LENGTH];               /* Sync standby names */
	replication_lag_config_t lag_config;        /* Lag configuration */
	wal_e_config_t wal_e_config;                /* WAL-E configuration */
	bootstrap_script_config_t bootstrap_config; /* Bootstrap configuration */
	bool auto_sync_mode;                        /* Auto-sync mode */
	int sync_timeout_ms;                        /* Sync timeout */
	bool validate_failover;                     /* Validate before failover */
} replication_config_t;

/* Function declarations */
bool ramctrl_replication_init(replication_config_t* config);
bool ramctrl_replication_get_status(replication_status_t* status);
bool ramctrl_replication_set_mode(replication_mode_t mode);
bool ramctrl_replication_update_lag_config(
    replication_lag_config_t* lag_config);
bool ramctrl_replication_validate_failover(int64_t lag_bytes);
bool ramctrl_replication_trigger_sync_switch(void);

/* WAL-E integration functions */
bool ramctrl_wal_e_init(wal_e_config_t* config);
bool ramctrl_wal_e_create_backup(void);
bool ramctrl_wal_e_restore_backup(const char* backup_name);
bool ramctrl_wal_e_list_backups(char* backup_list, size_t list_size);
bool ramctrl_wal_e_delete_backup(const char* backup_name);

/* Bootstrap script functions */
bool ramctrl_bootstrap_init(bootstrap_script_config_t* config);
bool ramctrl_bootstrap_run_script(const char* node_type);
bool ramctrl_bootstrap_validate_script(void);

/* Utility functions */
const char* ramctrl_replication_mode_to_string(replication_mode_t mode);
replication_mode_t ramctrl_replication_string_to_mode(const char* mode_str);
bool ramctrl_replication_is_sync_mode(replication_mode_t mode);
int64_t ramctrl_replication_calculate_lag(const char* sent_lsn,
                                          const char* replay_lsn);

#endif /* RAMCTRL_REPLICATION_H */
