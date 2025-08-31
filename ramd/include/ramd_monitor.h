/*-------------------------------------------------------------------------
 *
 * ramd_monitor.h
 *		PostgreSQL Auto-Failover Daemon - Monitoring
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_MONITOR_H
#define RAMD_MONITOR_H

#include <pthread.h>

#include "ramd.h"
#include "ramd_config.h"
#include "ramd_cluster.h"
#include "ramd_postgresql.h"

/* Monitor context */
typedef struct ramd_monitor_t {
    bool				enabled;
    bool				running;
    pthread_t				thread;
    time_t				last_check;
    int32_t				check_interval_ms;
    int32_t				health_check_timeout_ms;
    ramd_cluster_t			*cluster;
    const ramd_config_t			*config;
    ramd_postgresql_connection_t	local_connection;
} ramd_monitor_t;

/* Monitoring functions */
bool ramd_monitor_init(ramd_monitor_t *monitor, ramd_cluster_t *cluster,
                      const ramd_config_t *config);
void ramd_monitor_cleanup(ramd_monitor_t *monitor);
bool ramd_monitor_start(ramd_monitor_t *monitor);
void ramd_monitor_stop(ramd_monitor_t *monitor);
bool ramd_monitor_is_running(const ramd_monitor_t *monitor);

/* Monitoring thread */
void *ramd_monitor_thread_main(void *arg);
void ramd_monitor_run_cycle(ramd_monitor_t *monitor);

/* Health checking */
bool ramd_monitor_check_local_node(ramd_monitor_t *monitor);
bool ramd_monitor_check_remote_nodes(ramd_monitor_t *monitor);
bool ramd_monitor_check_cluster_health(ramd_monitor_t *monitor);

/* Leadership monitoring */
bool ramd_monitor_check_leadership(ramd_monitor_t *monitor);
bool ramd_monitor_detect_leadership_change(ramd_monitor_t *monitor);
bool ramd_monitor_handle_leadership_gained(ramd_monitor_t *monitor);
bool ramd_monitor_handle_leadership_lost(ramd_monitor_t *monitor);

/* Primary monitoring */
bool ramd_monitor_check_primary_health(ramd_monitor_t *monitor);
bool ramd_monitor_detect_primary_failure(ramd_monitor_t *monitor);
bool ramd_monitor_handle_primary_failure(ramd_monitor_t *monitor);

/* Role change detection */
bool ramd_monitor_detect_role_changes(ramd_monitor_t *monitor);
bool ramd_monitor_handle_role_change(ramd_monitor_t *monitor, 
                                   ramd_role_t old_role,
                                   ramd_role_t new_role);

/* State synchronization */
bool ramd_monitor_sync_cluster_state(ramd_monitor_t *monitor);
bool ramd_monitor_update_node_states(ramd_monitor_t *monitor);
bool ramd_monitor_broadcast_state_change(ramd_monitor_t *monitor,
                                        int32_t node_id,
                                        ramd_node_state_t new_state);

/* Health score calculation */
float ramd_monitor_calculate_node_health(ramd_monitor_t *monitor,
                                        ramd_node_t *node);
bool ramd_monitor_is_node_healthy(const ramd_monitor_t *monitor,
                                 const ramd_node_t *node);

#endif /* RAMD_MONITOR_H */
