/*-------------------------------------------------------------------------
 *
 * ramd_sync_replication.h
 *		PostgreSQL Auto-Failover Daemon - Synchronous Replication Management
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_SYNC_REPLICATION_H
#define RAMD_SYNC_REPLICATION_H

#include "ramd.h"
#include "ramd_postgresql.h"

/* PostgreSQL replication state enumeration */
typedef enum
{
	RAMD_REPLICATION_STATE_UNKNOWN = 0,
	RAMD_REPLICATION_STATE_STARTUP,
	RAMD_REPLICATION_STATE_CATCHUP,
	RAMD_REPLICATION_STATE_STREAMING,
	RAMD_REPLICATION_STATE_BACKUP,
	RAMD_REPLICATION_STATE_STOPPING
} ramd_postgresql_replication_state_t;

/* Synchronous Replication Modes */
typedef enum
{
	RAMD_SYNC_OFF,          /* Asynchronous replication */
	RAMD_SYNC_LOCAL,        /* Synchronous commit locally only */
	RAMD_SYNC_REMOTE_WRITE, /* Wait for remote write */
	RAMD_SYNC_REMOTE_APPLY  /* Wait for remote apply */
} ramd_sync_mode_t;

/* Synchronous Replication Configuration */
typedef struct ramd_sync_config_t
{
	ramd_sync_mode_t mode;
	char synchronous_standby_names[512];
	int32_t num_sync_standbys;
	int32_t sync_timeout_ms;
	bool enforce_sync_standbys;
	char application_name_pattern[128];
} ramd_sync_config_t;

/* Synchronous Standby Information */
typedef struct ramd_sync_standby_t
{
	int32_t node_id;
	char hostname[64];
	int32_t port;
	char application_name[64];
	bool is_sync;
	bool is_connected;
	int64_t flush_lag_bytes;
	int64_t replay_lag_bytes;
	time_t last_sync_time;
	ramd_postgresql_replication_state_t state;
} ramd_sync_standby_t;

/* Synchronous Replication Status */
typedef struct ramd_sync_status_t
{
	ramd_sync_mode_t current_mode;
	int32_t num_sync_standbys_configured;
	int32_t num_sync_standbys_connected;
	int32_t num_potential_standbys;
	ramd_sync_standby_t standbys[RAMD_MAX_NODES];
	bool all_sync_standbys_healthy;
	time_t last_status_update;
} ramd_sync_status_t;

/* Function prototypes */
bool ramd_sync_replication_init(ramd_sync_config_t* config);
void ramd_sync_replication_cleanup(void);

bool ramd_sync_replication_configure(const ramd_sync_config_t* config);
bool ramd_sync_replication_set_mode(ramd_sync_mode_t mode);
bool ramd_sync_replication_add_standby(int32_t node_id,
                                       const char* application_name);
bool ramd_sync_replication_remove_standby(int32_t node_id);

bool ramd_sync_replication_get_status(ramd_sync_status_t* status);
bool ramd_sync_replication_check_health(void);

bool ramd_sync_replication_update_postgresql_config(void);
bool ramd_sync_replication_reload_config(void);

/* Standby management */
bool ramd_sync_standby_promote_to_sync(int32_t node_id);
bool ramd_sync_standby_demote_from_sync(int32_t node_id);
bool ramd_sync_standby_is_eligible(int32_t node_id);

/* Monitoring and lag checking */
bool ramd_sync_replication_check_lag(ramd_sync_standby_t* standby);
bool ramd_sync_replication_wait_for_sync(int32_t timeout_ms);

/* Configuration helpers */
char* ramd_sync_mode_to_string(ramd_sync_mode_t mode);
ramd_sync_mode_t ramd_sync_string_to_mode(const char* mode_str);
bool ramd_sync_generate_standby_names(char* output, size_t output_size,
                                      const ramd_sync_standby_t* standbys,
                                      int32_t count);

/* Enhanced streaming replication functions */
bool ramd_sync_replication_take_basebackup(const ramd_config_t* config,
                                           const char* primary_host,
                                           int32_t primary_port);
bool ramd_sync_replication_configure_recovery(const ramd_config_t* config,
                                              const char* primary_host,
                                              int32_t primary_port);
bool ramd_sync_replication_setup_streaming(const ramd_config_t* config,
                                           const char* primary_host,
                                           int32_t primary_port,
                                           const char* application_name);
bool ramd_sync_replication_promote_standby(const ramd_config_t* config);

#endif /* RAMD_SYNC_REPLICATION_H */
