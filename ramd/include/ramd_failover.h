/*-------------------------------------------------------------------------
 *
 * ramd_failover.h
 *		PostgreSQL Auto-Failover Daemon - Enhanced Failover Logic
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_FAILOVER_H
#define RAMD_FAILOVER_H

#include "ramd.h"
#include "ramd_config.h"
#include "ramd_cluster.h"

/* Failover states */
typedef enum {
    RAMD_FAILOVER_STATE_NORMAL = 0,
    RAMD_FAILOVER_STATE_DETECTING,
    RAMD_FAILOVER_STATE_PROMOTING,
    RAMD_FAILOVER_STATE_RECOVERING,
    RAMD_FAILOVER_STATE_COMPLETED,
    RAMD_FAILOVER_STATE_FAILED
} ramd_failover_state_t;

/* Failover context */
typedef struct ramd_failover_context_t {
    ramd_failover_state_t	state;
    int32_t			failed_node_id;
    int32_t			new_primary_node_id;
    time_t			started_at;
    time_t			completed_at;
    char			reason[256];
    bool			auto_triggered;
    int32_t			retry_count;
} ramd_failover_context_t;

/* Failover detection and execution */
bool ramd_failover_detect_primary_failure(ramd_cluster_t *cluster);
bool ramd_failover_should_trigger(const ramd_cluster_t *cluster, 
                                 const ramd_config_t *config);
bool ramd_failover_execute(ramd_cluster_t *cluster, const ramd_config_t *config,
                          ramd_failover_context_t *context);

/* Enhanced failover steps */
bool ramd_failover_select_new_primary(const ramd_cluster_t *cluster, 
                                     int32_t *new_primary_id);
bool ramd_failover_promote_node(ramd_cluster_t *cluster, const ramd_config_t *config,
                               int32_t node_id);
bool ramd_failover_demote_failed_primary(ramd_cluster_t *cluster, 
                                        const ramd_config_t *config,
                                        int32_t failed_node_id);
bool ramd_failover_update_standby_nodes(ramd_cluster_t *cluster, 
                                       const ramd_config_t *config,
                                       int32_t new_primary_id);

/* New enhanced functions */
bool ramd_failover_stop_replication_on_node(const ramd_config_t *config, int32_t node_id);
bool ramd_failover_update_sync_replication_config(ramd_cluster_t *cluster, int32_t new_primary_id);
bool ramd_failover_rebuild_failed_replicas(ramd_cluster_t *cluster, const ramd_config_t *config);
bool ramd_failover_rebuild_replica_node(ramd_cluster_t *cluster, const ramd_config_t *config, int32_t node_id);

/* Recovery operations */
bool ramd_failover_recover_failed_node(ramd_cluster_t *cluster, 
                                      const ramd_config_t *config,
                                      int32_t failed_node_id);
bool ramd_failover_take_basebackup(const ramd_config_t *config, 
                                  const char *primary_host,
                                  int32_t primary_port);
bool ramd_failover_configure_recovery(const ramd_config_t *config,
                                     const char *primary_host,
                                     int32_t primary_port);

/* Failover validation */
bool ramd_failover_validate_promotion(const ramd_cluster_t *cluster,
                                     int32_t promoted_node_id);
bool ramd_failover_validate_cluster_state(const ramd_cluster_t *cluster);

/* Failover context management */
void ramd_failover_context_init(ramd_failover_context_t *context);
void ramd_failover_context_cleanup(ramd_failover_context_t *context);
void ramd_failover_context_set_reason(ramd_failover_context_t *context,
                                     const char *reason);

#endif /* RAMD_FAILOVER_H */
