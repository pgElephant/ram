/*-------------------------------------------------------------------------
 *
 * ramd_config.h
 *		PostgreSQL Auto-Failover Daemon - Configuration
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_CONFIG_H
#define RAMD_CONFIG_H

#include "ramd.h"
#include "ramd_logging.h"

/* Configuration structure */
typedef struct ramd_config
{
	/* Node identification */
	int32_t				node_id;
	char				hostname[RAMD_MAX_HOSTNAME_LENGTH];
	int32_t				postgresql_port;
	int32_t				rale_port;
	int32_t				dstore_port;

	/* PostgreSQL connection */
	char				postgresql_bin_dir[RAMD_MAX_PATH_LENGTH];
	char				postgresql_data_dir[RAMD_MAX_PATH_LENGTH];
	char				postgresql_config_file[RAMD_MAX_PATH_LENGTH];
	char				postgresql_log_dir[RAMD_MAX_PATH_LENGTH];
	char				database_name[64];
	char				database_user[64];
	char				database_password[64];
	char				postgresql_user[64];
	char				replication_user[64];

	/* Cluster settings */
	char				cluster_name[64];
	int32_t				cluster_size;
	bool				auto_failover_enabled;
	bool				synchronous_replication;

	/* Monitoring settings */
	int32_t				monitor_interval_ms;
	int32_t				health_check_timeout_ms;
	int32_t				failover_timeout_ms;
	int32_t				recovery_timeout_ms;

	/* Logging settings */
	char				log_file[RAMD_MAX_PATH_LENGTH];
	ramd_log_level_t	log_level;
	bool				log_to_syslog;
	bool				log_to_console;

	/* HTTP API settings */
	bool				http_api_enabled;
	char				http_bind_address[64];
	int32_t				http_port;
	bool				http_auth_enabled;
	char				http_auth_token[256];

	/* Synchronous replication settings */
	char				sync_standby_names[512];
	int32_t				num_sync_standbys;
	int32_t				sync_timeout_ms;
	bool				enforce_sync_standbys;

	/* Maintenance mode settings */
	bool				maintenance_mode_enabled;
	int32_t				maintenance_drain_timeout_ms;
	bool				maintenance_backup_before;

	/* Daemon settings */
	char				pid_file[RAMD_MAX_PATH_LENGTH];
	bool				daemonize;
	char				user[64];
	char				group[64];
} ramd_config_t;

/* Configuration functions */
extern bool ramd_config_init(ramd_config_t *config);
extern bool ramd_config_load_file(ramd_config_t *config, const char *config_file);
extern bool ramd_config_validate(const ramd_config_t *config);
extern void ramd_config_cleanup(ramd_config_t *config);
extern void ramd_config_print(const ramd_config_t *config);

/* Configuration defaults */
extern void ramd_config_set_defaults(ramd_config_t *config);

/* Configuration file parsing */
extern bool ramd_config_parse_line(ramd_config_t *config, const char *line);
extern bool ramd_config_parse_key_value(ramd_config_t *config, const char *key, const char *value);

#endif							/* RAMD_CONFIG_H */
