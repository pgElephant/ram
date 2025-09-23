/*-------------------------------------------------------------------------
 *
 * ramctrl.h
 *		PostgreSQL RAM Control Utility - Main header
 *
 * ramctrl is a command-line utility to control and monitor the
 * PostgreSQL RAM cluster (pgraft extension + ramd daemon).
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
#define RAMCTRL_VERSION_MAJOR 1
#define RAMCTRL_VERSION_MINOR 0
#define RAMCTRL_VERSION_PATCH 0
#define RAMCTRL_VERSION_STRING "1.0.0"

/* Exit codes following Linux CLI standards */
#define RAMCTRL_EXIT_SUCCESS 0     /* Success */
#define RAMCTRL_EXIT_FAILURE 1     /* General failure */
#define RAMCTRL_EXIT_USAGE 2       /* Invalid usage/arguments */
#define RAMCTRL_EXIT_NOENT 3       /* No such file or directory */
#define RAMCTRL_EXIT_CONNFAILED 4  /* Connection failed */
#define RAMCTRL_EXIT_PERMISSION 5  /* Permission denied */
#define RAMCTRL_EXIT_TIMEOUT 6     /* Operation timed out */
#define RAMCTRL_EXIT_UNAVAILABLE 7 /* Service unavailable */

/* Use centralized defaults from ramctrl_defaults.h */
#include "ramctrl_defaults.h"

/* Main command types */
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
	RAMCTRL_CMD_LOGS,
	RAMCTRL_CMD_HELP,
	RAMCTRL_CMD_VERSION,
	/* Hierarchical commands */
	RAMCTRL_CMD_SHOW,
	RAMCTRL_CMD_NODE,
	RAMCTRL_CMD_WATCH,
	RAMCTRL_CMD_REPLICATION,
	RAMCTRL_CMD_REPLICA,
	RAMCTRL_CMD_BACKUP,
	RAMCTRL_CMD_BOOTSTRAP
} ramctrl_command_t;

/* Show subcommands */
typedef enum ramctrl_show_command
{
	RAMCTRL_SHOW_UNKNOWN,
	RAMCTRL_SHOW_CLUSTER,
	RAMCTRL_SHOW_NODES,
	RAMCTRL_SHOW_REPLICATION,
	RAMCTRL_SHOW_STATUS,
	RAMCTRL_SHOW_CONFIG,
	RAMCTRL_SHOW_LOGS
} ramctrl_show_command_t;

/* Node subcommands */
typedef enum ramctrl_node_command
{
	RAMCTRL_NODE_UNKNOWN,
	RAMCTRL_NODE_ADD,
	RAMCTRL_NODE_REMOVE,
	RAMCTRL_NODE_LIST,
	RAMCTRL_NODE_STATUS,
	RAMCTRL_NODE_ENABLE_MAINTENANCE,
	RAMCTRL_NODE_DISABLE_MAINTENANCE
} ramctrl_node_command_t;

/* Watch subcommands */
typedef enum ramctrl_watch_command
{
	RAMCTRL_WATCH_UNKNOWN,
	RAMCTRL_WATCH_CLUSTER,
	RAMCTRL_WATCH_NODES,
	RAMCTRL_WATCH_REPLICATION,
	RAMCTRL_WATCH_STATUS
} ramctrl_watch_command_t;

/* Replication subcommands */
typedef enum ramctrl_replication_command
{
	RAMCTRL_REPLICATION_UNKNOWN,
	RAMCTRL_REPLICATION_STATUS,
	RAMCTRL_REPLICATION_SET_MODE,
	RAMCTRL_REPLICATION_SET_LAG_THRESHOLD,
	RAMCTRL_REPLICATION_SHOW_SLOTS
} ramctrl_replication_command_t;

/* Replica subcommands */
typedef enum ramctrl_replica_command
{
	RAMCTRL_REPLICA_UNKNOWN,
	RAMCTRL_REPLICA_ADD,
	RAMCTRL_REPLICA_REMOVE,
	RAMCTRL_REPLICA_LIST,
	RAMCTRL_REPLICA_STATUS
} ramctrl_replica_command_t;

/* Backup subcommands */
typedef enum ramctrl_backup_command
{
	RAMCTRL_BACKUP_UNKNOWN,
	RAMCTRL_BACKUP_CREATE,
	RAMCTRL_BACKUP_RESTORE,
	RAMCTRL_BACKUP_LIST,
	RAMCTRL_BACKUP_DELETE
} ramctrl_backup_command_t;

/* Bootstrap subcommands */
typedef enum ramctrl_bootstrap_command
{
	RAMCTRL_BOOTSTRAP_UNKNOWN,
	RAMCTRL_BOOTSTRAP_RUN,
	RAMCTRL_BOOTSTRAP_VALIDATE,
	RAMCTRL_BOOTSTRAP_INIT
} ramctrl_bootstrap_command_t;

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
	int32_t node_id;
	char hostname[RAMCTRL_MAX_HOSTNAME_LENGTH];
	char node_address[RAMCTRL_MAX_HOSTNAME_LENGTH];
	char node_name[RAMCTRL_MAX_HOSTNAME_LENGTH];
	int32_t port;
	int32_t node_port;
	ramctrl_node_status_t status;
	time_t last_seen;
	bool is_primary;
	bool is_standby;
	bool is_leader;
	bool is_healthy;
	bool is_active;
	float health_score;
	int64_t wal_lsn;
	int32_t replication_lag_ms;
} ramctrl_node_info_t;

/* Cluster information */
typedef struct ramctrl_cluster_info
{
	int32_t cluster_id;
	char cluster_name[RAMCTRL_MAX_HOSTNAME_LENGTH];
	int32_t total_nodes;
	int32_t active_nodes;
	int32_t node_count;
	int32_t primary_node_id;
	int32_t leader_node_id;
	bool has_quorum;
	bool auto_failover_enabled;
	ramctrl_cluster_status_t status;
	time_t last_update;
} ramctrl_cluster_info_t;

/* Control context */
typedef struct ramctrl_context
{
	char api_url[RAMCTRL_MAX_PATH_LENGTH];
	char config_file[RAMCTRL_MAX_PATH_LENGTH];
	int timeout_seconds;
	bool verbose;
	bool json_output;
	bool table_output;
	bool quiet;
	bool force;
	bool dry_run;
	ramctrl_command_t command;
	/* Subcommands */
	ramctrl_show_command_t show_command;
	ramctrl_node_command_t node_command;
	ramctrl_watch_command_t watch_command;
	ramctrl_replication_command_t replication_command;
	ramctrl_replica_command_t replica_command;
	ramctrl_backup_command_t backup_command;
	ramctrl_bootstrap_command_t bootstrap_command;
	char command_args[RAMCTRL_MAX_NODES][RAMCTRL_MAX_HOSTNAME_LENGTH];
	int command_argc;
	char postgresql_data_dir[RAMCTRL_MAX_PATH_LENGTH];
} ramctrl_context_t;

/* Function prototypes */

/* Main control functions */
extern bool ramctrl_init(ramctrl_context_t* ctx);
extern void ramctrl_cleanup(ramctrl_context_t* ctx);
extern int ramctrl_execute_command(ramctrl_context_t* ctx);

/* Command line parsing */
extern bool ramctrl_parse_args(ramctrl_context_t* ctx, int argc, char* argv[]);
extern void ramctrl_usage(const char* progname);
extern void ramctrl_version(void);

/* Basic command function declarations */
extern int ramctrl_cmd_status(ramctrl_context_t* ctx);
extern int ramctrl_cmd_start(ramctrl_context_t* ctx);
extern int ramctrl_cmd_stop(ramctrl_context_t* ctx);
extern int ramctrl_cmd_restart(ramctrl_context_t* ctx);
extern int ramctrl_cmd_promote(ramctrl_context_t* ctx);
extern int ramctrl_cmd_demote(ramctrl_context_t* ctx);
extern int ramctrl_cmd_failover(ramctrl_context_t* ctx);
extern int ramctrl_cmd_logs(ramctrl_context_t* ctx);
extern int ramctrl_cmd_help(ramctrl_context_t* ctx);

/* Hierarchical command functions */
extern int ramctrl_cmd_show(ramctrl_context_t* ctx);
extern int ramctrl_cmd_node(ramctrl_context_t* ctx);
extern int ramctrl_cmd_watch_new(ramctrl_context_t* ctx);
extern int ramctrl_cmd_replication(ramctrl_context_t* ctx);
extern int ramctrl_cmd_replica(ramctrl_context_t* ctx);
extern int ramctrl_cmd_add_replica(ramctrl_context_t* ctx);
extern int ramctrl_cmd_remove_replica(ramctrl_context_t* ctx);
extern int ramctrl_cmd_list_replicas(ramctrl_context_t* ctx);
extern int ramctrl_cmd_replica_status(ramctrl_context_t* ctx);
extern int ramctrl_cmd_backup(ramctrl_context_t* ctx);
extern int ramctrl_cmd_bootstrap(ramctrl_context_t* ctx);

/* Show subcommand help functions */
extern void ramctrl_show_help(void);
extern void ramctrl_node_help(void);
extern void ramctrl_watch_help(void);
extern void ramctrl_replica_help(void);
extern void ramctrl_replication_help(void);
extern void ramctrl_backup_help(void);
extern void ramctrl_bootstrap_help(void);

/* Cluster operations */
extern bool ramctrl_get_cluster_info(ramctrl_context_t* ctx,
                                     ramctrl_cluster_info_t* info);
extern bool ramctrl_get_node_info(ramctrl_context_t* ctx,
                                  ramctrl_node_info_t* nodes,
                                  int32_t* node_count);

/* Database operation wrappers */
extern bool ramctrl_promote_node(ramctrl_context_t* ctx, int32_t node_id);
extern bool ramctrl_demote_node(ramctrl_context_t* ctx, int32_t node_id);
extern bool ramctrl_trigger_failover(ramctrl_context_t* ctx,
                                     int32_t target_node_id);
extern bool ramctrl_add_node(ramctrl_context_t* ctx, int32_t node_id,
                             const char* hostname, int32_t port);
extern bool ramctrl_remove_node(ramctrl_context_t* ctx, int32_t node_id);
extern bool ramctrl_enable_maintenance_mode(ramctrl_context_t* ctx,
                                            int32_t node_id);
extern bool ramctrl_disable_maintenance_mode(ramctrl_context_t* ctx,
                                             int32_t node_id);
extern bool ramctrl_get_all_nodes(ramctrl_context_t* ctx,
                                  ramctrl_node_info_t** nodes, int* node_count);

/* Formation command functions */
extern int ramctrl_cmd_show_cluster(ramctrl_context_t* ctx, const char* cluster_name);
extern int ramctrl_cmd_add_node(ramctrl_context_t* ctx, const char* node_name, 
                                const char* node_address, int node_port);
extern int ramctrl_cmd_remove_node(ramctrl_context_t* ctx, const char* node_name);
extern bool ramctrl_remove_node_from_consensus(ramctrl_context_t* ctx, const char* node_address);
extern bool ramctrl_remove_node_from_cluster(ramctrl_context_t* ctx, const char* node_address);
extern bool ramctrl_add_node_to_cluster(ramctrl_context_t* ctx, ramctrl_node_info_t* node);
extern bool ramctrl_add_node_to_consensus(ramctrl_context_t* ctx, ramctrl_node_info_t* node);

#endif /* RAMCTRL_H */
