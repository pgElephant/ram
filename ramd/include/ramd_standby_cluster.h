/*-------------------------------------------------------------------------
 *
 * ramd_standby_cluster.h
 *		Standby cluster support for disaster recovery
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_STANDBY_CLUSTER_H
#define RAMD_STANDBY_CLUSTER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

/* Standby cluster configuration */
typedef struct ramd_standby_cluster_config
{
	bool enabled;                       /* Enable standby cluster mode */
	char standby_cluster_name[64];      /* Name of this standby cluster */
	char primary_cluster_name[64];      /* Name of primary cluster */
	char primary_cluster_endpoint[256]; /* Connection to primary cluster */
	char primary_host[256];
	int32_t primary_port;
	char primary_database[64];
	char replication_user[64];
	char replication_password[64];
	char replication_slot_name[64];
	char slot_name[64];         /* Replication slot name */
	bool use_replication_slots; /* Use replication slots */
	bool is_promoted;
	int32_t sync_interval_seconds;     /* How often to sync with primary */
	int32_t promotion_timeout_seconds; /* Timeout for promotion */
	bool auto_promote_on_failure;      /* Auto-promote if primary fails */
	char recovery_conf_template[512];  /* Template for recovery.conf */
	char promotion_script[512];        /* Script to run on promotion */
	int32_t max_lag_bytes;             /* Max lag before alerting */
	int32_t health_check_interval;
	time_t last_primary_contact;
	int32_t max_lag_ms;
	pthread_t monitor_thread;
	void* primary_connection; /* PGconn * */
} ramd_standby_cluster_config_t;

/* Standby cluster state */
typedef enum ramd_standby_cluster_state
{
	RAMD_STANDBY_STATE_UNKNOWN = 0,
	RAMD_STANDBY_STATE_SYNCING,      /* Syncing with primary */
	RAMD_STANDBY_STATE_SYNCHRONIZED, /* In sync with primary */
	RAMD_STANDBY_STATE_LAGGED,       /* Lagging behind primary */
	RAMD_STANDBY_STATE_DISCONNECTED, /* Disconnected from primary */
	RAMD_STANDBY_STATE_PROMOTING,    /* Currently promoting */
	RAMD_STANDBY_STATE_PROMOTED,     /* Has been promoted to primary */
	RAMD_STANDBY_STATE_FAILED        /* Failed state */
} ramd_standby_cluster_state_t;


/* Standby cluster status */
typedef struct ramd_standby_cluster_status
{
	ramd_standby_cluster_state_t state;
	time_t last_sync_time;
	int64_t lag_bytes;
	int64_t lag_seconds;
	bool primary_available;
	char primary_lsn[32];
	char standby_lsn[32];
	int32_t connected_nodes;
	int32_t healthy_nodes;
	char last_error[256];
} ramd_standby_cluster_status_t;

/* Disaster recovery plan */
typedef struct ramd_disaster_recovery_plan
{
	char plan_name[64];
	int32_t rto_seconds;           /* Recovery Time Objective */
	int32_t rpo_seconds;           /* Recovery Point Objective */
	bool auto_failback;            /* Auto failback when primary recovers */
	bool auto_failover_enabled;    /* Auto failover enabled */
	int32_t failover_timeout;      /* Failover timeout */
	char failover_script[512];     /* Script for DR failover */
	char failback_script[512];     /* Script for DR failback */
	char notification_script[512]; /* Script for notifications */
	char dns_update_script[512];   /* Script for DNS updates */
} ramd_disaster_recovery_plan_t;

/* Function prototypes */
extern bool ramd_standby_cluster_init(ramd_standby_cluster_config_t* config);
extern void ramd_standby_cluster_cleanup(void);
extern bool ramd_standby_cluster_is_enabled(void);
extern bool ramd_standby_cluster_start_sync(void);
extern bool ramd_standby_cluster_stop_sync(void);
extern bool ramd_standby_cluster_promote(void);
extern bool ramd_standby_cluster_demote(void);
extern bool
ramd_standby_cluster_get_status(ramd_standby_cluster_status_t* status);
extern ramd_standby_cluster_state_t ramd_standby_cluster_get_state(void);
extern bool ramd_standby_cluster_check_primary_health(void);
extern bool ramd_standby_cluster_update_lag_info(void);
extern bool ramd_standby_cluster_setup_replication_slot(void);
extern bool ramd_standby_cluster_remove_replication_slot(void);
extern bool ramd_standby_cluster_create_replication_slot(void);
extern bool ramd_standby_cluster_setup_replication(void);
extern bool ramd_standby_cluster_connect_to_primary(void);
extern void* ramd_standby_monitor_thread(void* arg);

/* Configuration helpers */
extern void
ramd_standby_cluster_config_set_defaults(ramd_standby_cluster_config_t* config);
extern bool
ramd_standby_cluster_config_validate(ramd_standby_cluster_config_t* config);
extern bool ramd_standby_cluster_config_load_from_file(const char* config_file);
extern bool ramd_standby_cluster_config_save_to_file(const char* config_file);

/* Disaster recovery functions */
extern bool ramd_disaster_recovery_init(ramd_disaster_recovery_plan_t* plan);
extern bool ramd_disaster_recovery_execute_failover(void);
extern bool ramd_disaster_recovery_execute_failback(void);
extern bool
ramd_disaster_recovery_validate_plan(ramd_disaster_recovery_plan_t* plan);
extern bool ramd_disaster_recovery_test_connectivity(void);
extern bool ramd_standby_cluster_execute_dr_failover(void);
extern long ramd_standby_cluster_get_replication_lag(void);

/* Monitoring and alerting */
extern bool ramd_standby_cluster_monitor_lag(void);
extern bool ramd_standby_cluster_send_alert(const char* alert_type,
                                            const char* message);
extern bool
ramd_standby_cluster_log_status_change(ramd_standby_cluster_state_t old_state,
                                       ramd_standby_cluster_state_t new_state);

/* Cross-cluster communication */
extern bool ramd_standby_cluster_ping_primary(void);
extern bool ramd_standby_cluster_request_promotion_permission(void);
extern bool ramd_standby_cluster_notify_primary_of_promotion(void);
extern bool ramd_standby_cluster_register_with_primary(void);
extern bool ramd_standby_cluster_unregister_from_primary(void);

/* Utility functions */
extern const char*
ramd_standby_cluster_state_to_string(ramd_standby_cluster_state_t state);
extern ramd_standby_cluster_state_t
ramd_standby_cluster_string_to_state(const char* state_str);
extern bool ramd_standby_cluster_is_promotion_safe(void);
extern bool ramd_standby_cluster_create_recovery_conf(void);
extern bool ramd_standby_cluster_backup_configuration(void);
extern bool ramd_standby_cluster_restore_configuration(void);

#endif /* RAMD_STANDBY_CLUSTER_H */
