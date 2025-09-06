/*-------------------------------------------------------------------------
 *
 * ramd_maintenance.h
 *		PostgreSQL Auto-Failover Daemon - Maintenance Mode Management
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_MAINTENANCE_H
#define RAMD_MAINTENANCE_H

#include "ramd.h"

/* Maintenance mode types */
typedef enum
{
	RAMD_MAINTENANCE_NONE,             /* No maintenance mode */
	RAMD_MAINTENANCE_NODE,             /* Single node maintenance */
	RAMD_MAINTENANCE_CLUSTER,          /* Cluster-wide maintenance */
	RAMD_MAINTENANCE_PLANNED_FAILOVER, /* Planned failover operation */
	RAMD_MAINTENANCE_BACKUP,           /* Backup operations */
	RAMD_MAINTENANCE_UPGRADE,          /* Software/OS upgrade */
	RAMD_MAINTENANCE_NETWORK           /* Network maintenance */
} ramd_maintenance_type_t;

/* Maintenance mode status */
typedef enum
{
	RAMD_MAINTENANCE_STATUS_INACTIVE,
	RAMD_MAINTENANCE_STATUS_PENDING,
	RAMD_MAINTENANCE_STATUS_ACTIVE,
	RAMD_MAINTENANCE_STATUS_DRAINING,
	RAMD_MAINTENANCE_STATUS_COMPLETING,
	RAMD_MAINTENANCE_STATUS_FAILED
} ramd_maintenance_status_t;

/* Maintenance operation configuration */
typedef struct ramd_maintenance_config_t
{
	ramd_maintenance_type_t type;
	int32_t target_node_id;
	bool disable_auto_failover;
	bool drain_connections;
	int32_t drain_timeout_ms;
	bool prevent_writes;
	bool backup_before_maintenance;
	char reason[256];
	char contact_info[128];
	time_t scheduled_start;
	time_t scheduled_end;
	int32_t max_duration_ms;
} ramd_maintenance_config_t;

/* Maintenance operation state */
typedef struct ramd_maintenance_state_t
{
	bool in_maintenance;
	ramd_maintenance_type_t type;
	ramd_maintenance_status_t status;
	int32_t target_node_id;
	time_t start_time;
	time_t end_time;
	time_t scheduled_end;
	char reason[256];
	char contact_info[128];
	char initiated_by[64];
	bool auto_failover_disabled;
	bool connections_drained;
	int32_t active_connections;
	char backup_id[64];
	char status_message[256];
} ramd_maintenance_state_t;

/* Pre-maintenance checks */
typedef struct ramd_maintenance_check_t
{
	bool cluster_healthy;
	bool all_nodes_reachable;
	bool replication_current;
	bool no_active_transactions;
	bool backup_available;
	bool sufficient_standbys;
	int32_t active_connections;
	char check_details[512];
} ramd_maintenance_check_t;

/* Function prototypes */
bool ramd_maintenance_init(void);
void ramd_maintenance_cleanup(void);

/* Maintenance mode operations */
bool ramd_maintenance_enter(const ramd_maintenance_config_t* config);
bool ramd_maintenance_exit(int32_t node_id);
bool ramd_maintenance_get_status(int32_t node_id,
                                 ramd_maintenance_state_t* state);

/* Pre-maintenance validation */
bool ramd_maintenance_pre_check(const ramd_maintenance_config_t* config,
                                ramd_maintenance_check_t* checks);
bool ramd_maintenance_is_safe_to_enter(const ramd_maintenance_config_t* config);

/* Connection draining */
bool ramd_maintenance_drain_connections(int32_t node_id, int32_t timeout_ms);
bool ramd_maintenance_get_connection_count(int32_t node_id, int32_t* count);
bool ramd_maintenance_prevent_new_connections(int32_t node_id, bool prevent);

/* Bootstrap automation functions */
bool ramd_maintenance_bootstrap_new_node(
    const ramd_config_t* config, const char* node_name, const char* node_host,
    int32_t node_port, const char* primary_host, int32_t primary_port);
bool ramd_maintenance_bootstrap_primary_node(const ramd_config_t* config,
                                             const char* cluster_name,
                                             const char* primary_host,
                                             int32_t primary_port);
bool ramd_maintenance_bootstrap_cluster(
    const ramd_config_t* config, const char* cluster_name,
    const char* primary_host, int32_t primary_port, const char** standby_hosts,
    int32_t* standby_ports, int32_t standby_count);
bool ramd_maintenance_setup_replica(const ramd_config_t* config,
                                    const char* cluster_name,
                                    const char* replica_host,
                                    int32_t replica_port,
                                    int32_t node_id);

/* Node management functions */
bool ramd_maintenance_start_postgresql_node(const ramd_config_t* config,
                                            const char* host, int32_t port);
bool ramd_maintenance_take_basebackup_from_primary(const char* data_dir,
                                                   const char* primary_host,
                                                   int32_t primary_port);
bool ramd_maintenance_configure_recovery_for_primary(const char* data_dir,
                                                     const char* primary_host,
                                                     int32_t primary_port);

/* Health verification functions */
bool ramd_maintenance_verify_cluster_health(
    const ramd_config_t* config, const char* primary_host, int32_t primary_port,
    const char** standby_hosts, int32_t* standby_ports, int32_t standby_count);
bool ramd_maintenance_verify_node_health(const char* host, int32_t port);
bool ramd_maintenance_verify_replication_status(const char* host, int32_t port);

/* Failover control during maintenance */
bool ramd_maintenance_disable_auto_failover(int32_t node_id);
bool ramd_maintenance_enable_auto_failover(int32_t node_id);
bool ramd_maintenance_is_auto_failover_disabled(int32_t node_id);

/* Backup operations */
bool ramd_maintenance_create_backup(int32_t node_id, char* backup_id,
                                    size_t backup_id_size);
bool ramd_maintenance_verify_backup(const char* backup_id);

/* Monitoring during maintenance */
bool ramd_maintenance_monitor_progress(int32_t node_id);
bool ramd_maintenance_update_status(int32_t node_id, const char* message);

/* Scheduled maintenance */
bool ramd_maintenance_schedule(const ramd_maintenance_config_t* config);
bool ramd_maintenance_cancel_scheduled(int32_t node_id);
bool ramd_maintenance_list_scheduled(ramd_maintenance_state_t* states,
                                     int32_t* count);

/* Utility functions */
const char* ramd_maintenance_type_to_string(ramd_maintenance_type_t type);
const char* ramd_maintenance_status_to_string(ramd_maintenance_status_t status);
ramd_maintenance_type_t ramd_maintenance_string_to_type(const char* type_str);
bool ramd_maintenance_is_cluster_safe_for_maintenance(int32_t target_node_id);

#endif /* RAMD_MAINTENANCE_H */
