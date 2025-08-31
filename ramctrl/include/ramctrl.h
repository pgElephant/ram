/*-------------------------------------------------------------------------
 *
 * ramctrl.h
 *		PostgreSQL RAM Control Utility - Main header
 *
 * ramctrl is a command-line utility to control and monitor the
 * PostgreSQL RAM cluster (pg_ram extension + ramd daemon).
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_H
#define RAMCTRL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Version information */
#define RAMCTRL_VERSION_MAJOR		1
#define RAMCTRL_VERSION_MINOR		0
#define RAMCTRL_VERSION_PATCH		0
#define RAMCTRL_VERSION_STRING		"1.0.0"

/* Exit codes following Linux CLI standards */
#define RAMCTRL_EXIT_SUCCESS		0	/* Success */
#define RAMCTRL_EXIT_FAILURE		1	/* General failure */
#define RAMCTRL_EXIT_USAGE			2	/* Invalid usage/arguments */
#define RAMCTRL_EXIT_NOENT			3	/* No such file or directory */
#define RAMCTRL_EXIT_CONNFAILED		4	/* Connection failed */
#define RAMCTRL_EXIT_PERMISSION		5	/* Permission denied */
#define RAMCTRL_EXIT_TIMEOUT		6	/* Operation timed out */
#define RAMCTRL_EXIT_UNAVAILABLE	7	/* Service unavailable */

/* Configuration constants */
#define RAMCTRL_MAX_HOSTNAME_LENGTH	256
#define RAMCTRL_MAX_PATH_LENGTH		512
#define RAMCTRL_MAX_COMMAND_LENGTH	1024
#define RAMCTRL_MAX_NODES			16

/* Command types */
typedef enum ramctrl_command
{
	RAMCTRL_CMD_UNKNOWN,
	RAMCTRL_CMD_STATUS,
	RAMCTRL_CMD_START,
	RAMCTRL_CMD_STOP,
	RAMCTRL_CMD_RESTART,
	RAMCTRL_CMD_PROMOTE,
	RAMCTRL_CMD_DEMOTE,
	RAMCTRL_CMD_FAILOVER,
	RAMCTRL_CMD_SHOW_CLUSTER,
	RAMCTRL_CMD_SHOW_NODES,
	RAMCTRL_CMD_ADD_NODE,
	RAMCTRL_CMD_REMOVE_NODE,
	RAMCTRL_CMD_ENABLE_MAINTENANCE,
	RAMCTRL_CMD_DISABLE_MAINTENANCE,
	RAMCTRL_CMD_LOGS,
	RAMCTRL_CMD_HELP,
	RAMCTRL_CMD_VERSION,
	/* New replication management commands */
	RAMCTRL_CMD_SHOW_REPLICATION,
	RAMCTRL_CMD_SET_REPLICATION_MODE,
	RAMCTRL_CMD_SET_LAG_THRESHOLD,
	RAMCTRL_CMD_WAL_E_BACKUP,
	RAMCTRL_CMD_WAL_E_RESTORE,
	RAMCTRL_CMD_WAL_E_LIST,
	RAMCTRL_CMD_WAL_E_DELETE,
	RAMCTRL_CMD_BOOTSTRAP_RUN,
	RAMCTRL_CMD_BOOTSTRAP_VALIDATE
} ramctrl_command_t;

/* Node status */
typedef enum ramctrl_node_status
{
	RAMCTRL_NODE_STATUS_UNKNOWN = 0,
	RAMCTRL_NODE_STATUS_RUNNING,
	RAMCTRL_NODE_STATUS_STOPPED,
	RAMCTRL_NODE_STATUS_FAILED,
	RAMCTRL_NODE_STATUS_MAINTENANCE
} ramctrl_node_status_t;

/* Cluster status */
typedef enum ramctrl_cluster_status
{
	RAMCTRL_CLUSTER_STATUS_UNKNOWN = 0,
	RAMCTRL_CLUSTER_STATUS_HEALTHY,
	RAMCTRL_CLUSTER_STATUS_DEGRADED,
	RAMCTRL_CLUSTER_STATUS_FAILED,
	RAMCTRL_CLUSTER_STATUS_MAINTENANCE
} ramctrl_cluster_status_t;

/* Node information */
typedef struct ramctrl_node_info
{
	int32_t		node_id;
	char		hostname[RAMCTRL_MAX_HOSTNAME_LENGTH];
	int32_t		port;
	ramctrl_node_status_t status;
	time_t		last_seen;
	bool		is_primary;
	bool		is_standby;
	bool		is_leader;
	bool		is_healthy;
	float		health_score;
	int64_t		wal_lsn;
	int32_t		replication_lag_ms;
} ramctrl_node_info_t;

/* Cluster information */
typedef struct ramctrl_cluster_info
{
	int32_t		cluster_id;
	char		cluster_name[64];
	int32_t		total_nodes;
	int32_t		active_nodes;
	int32_t		primary_node_id;
	int32_t		leader_node_id;
	bool		has_quorum;
	bool		auto_failover_enabled;
	ramctrl_cluster_status_t status;
	time_t		last_update;
} ramctrl_cluster_info_t;

/* Control context */
typedef struct ramctrl_context
{
	char		hostname[256];
	int			port;
	char		database[256];
	char		user[256];
	char		password[256];
	char		config_file[512];
	int			timeout_seconds;
	bool		verbose;
	bool		json_output;
	bool		table_output;
	ramctrl_command_t command;
	char		command_args[16][256];
	int			command_argc;
} ramctrl_context_t;

/* Function prototypes */

/* Main control functions */
extern bool ramctrl_init(ramctrl_context_t *ctx);
extern void ramctrl_cleanup(ramctrl_context_t *ctx);
extern int ramctrl_execute_command(ramctrl_context_t *ctx);

/* Command line parsing */
extern bool ramctrl_parse_args(ramctrl_context_t *ctx, int argc, char *argv[]);
extern void ramctrl_usage(const char *progname);
extern void ramctrl_version(void);

/* Command function declarations */
extern int ramctrl_cmd_status(ramctrl_context_t *ctx);
extern int ramctrl_cmd_start(ramctrl_context_t *ctx);
extern int ramctrl_cmd_stop(ramctrl_context_t *ctx);
extern int ramctrl_cmd_restart(ramctrl_context_t *ctx);
extern int ramctrl_cmd_promote(ramctrl_context_t *ctx);
extern int ramctrl_cmd_demote(ramctrl_context_t *ctx);
extern int ramctrl_cmd_failover(ramctrl_context_t *ctx);
extern int ramctrl_cmd_show_cluster(ramctrl_context_t *ctx);
extern int ramctrl_cmd_show_nodes(ramctrl_context_t *ctx);
extern int ramctrl_cmd_add_node(ramctrl_context_t *ctx);
extern int ramctrl_cmd_remove_node(ramctrl_context_t *ctx);
extern int ramctrl_cmd_enable_maintenance(ramctrl_context_t *ctx);
extern int ramctrl_cmd_disable_maintenance(ramctrl_context_t *ctx);
extern int ramctrl_cmd_logs(ramctrl_context_t *ctx);
/* New replication management command functions */
extern int ramctrl_cmd_show_replication(ramctrl_context_t *ctx);
extern int ramctrl_cmd_set_replication_mode(ramctrl_context_t *ctx);
extern int ramctrl_cmd_set_lag_threshold(ramctrl_context_t *ctx);
extern int ramctrl_cmd_wal_e_backup(ramctrl_context_t *ctx);
extern int ramctrl_cmd_wal_e_restore(ramctrl_context_t *ctx);
extern int ramctrl_cmd_wal_e_list(ramctrl_context_t *ctx);
extern int ramctrl_cmd_wal_e_delete(ramctrl_context_t *ctx);
extern int ramctrl_cmd_bootstrap_run(ramctrl_context_t *ctx);
extern int ramctrl_cmd_bootstrap_validate(ramctrl_context_t *ctx);

/* Cluster operations */
extern bool ramctrl_get_cluster_info(ramctrl_context_t *ctx, ramctrl_cluster_info_t *info);
extern bool ramctrl_get_node_info(ramctrl_context_t *ctx, ramctrl_node_info_t *nodes, int32_t *node_count);

/* Database operation wrappers */
extern bool ramctrl_promote_node(ramctrl_context_t *ctx, int32_t node_id);
extern bool ramctrl_demote_node(ramctrl_context_t *ctx, int32_t node_id);
extern bool ramctrl_trigger_failover(ramctrl_context_t *ctx, int32_t target_node_id);
extern bool ramctrl_add_node(ramctrl_context_t *ctx, int32_t node_id, const char *hostname, int32_t port);
extern bool ramctrl_remove_node(ramctrl_context_t *ctx, int32_t node_id);
extern bool ramctrl_enable_maintenance_mode(ramctrl_context_t *ctx, int32_t node_id);
extern bool ramctrl_disable_maintenance_mode(ramctrl_context_t *ctx, int32_t node_id);
extern bool ramctrl_get_all_nodes(ramctrl_context_t *ctx, ramctrl_node_info_t **nodes, int *node_count);

#endif							/* RAMCTRL_H */
