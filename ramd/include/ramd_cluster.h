/*-------------------------------------------------------------------------
 *
 * ramd_cluster.h
 *		PostgreSQL Auto-Failover Daemon - Cluster Management
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_CLUSTER_H
#define RAMD_CLUSTER_H

#include "ramd.h"
#include "ramd_config.h"

/* Node information structure */
typedef struct ramd_node_t {
    int32_t			node_id;
    char			hostname[RAMD_MAX_HOSTNAME_LENGTH];
    int32_t			postgresql_port;
    int32_t			rale_port;
    int32_t			dstore_port;
    ramd_node_state_t		state;
    ramd_role_t			role;
    bool			is_leader;
    bool			is_healthy;
    time_t			last_seen;
    time_t			state_changed_at;
    float			health_score;
    int64_t			wal_lsn;
    int32_t			replication_lag_ms;
} ramd_node_t;

/* Cluster structure */
typedef struct ramd_cluster_t {
    char			cluster_name[64];
    int32_t			node_count;
    ramd_node_t			nodes[RAMD_MAX_NODES];
    int32_t			primary_node_id;
    int32_t			leader_node_id;
    int32_t			local_node_id;
    bool			has_quorum;
    bool			in_failover;
    time_t			last_topology_change;
    time_t			last_health_check;
} ramd_cluster_t;

/* Cluster management functions */
bool ramd_cluster_init(ramd_cluster_t *cluster, const ramd_config_t *config);
void ramd_cluster_cleanup(ramd_cluster_t *cluster);

/* Node management */
bool ramd_cluster_add_node(ramd_cluster_t *cluster, int32_t node_id, 
                          const char *hostname, int32_t pg_port, 
                          int32_t rale_port, int32_t dstore_port);
bool ramd_cluster_remove_node(ramd_cluster_t *cluster, int32_t node_id);
ramd_node_t *ramd_cluster_find_node(ramd_cluster_t *cluster, int32_t node_id);
ramd_node_t *ramd_cluster_get_local_node(ramd_cluster_t *cluster);
ramd_node_t *ramd_cluster_get_primary_node(ramd_cluster_t *cluster);
ramd_node_t *ramd_cluster_get_leader_node(ramd_cluster_t *cluster);

/* Node state management */
bool ramd_cluster_update_node_state(ramd_cluster_t *cluster, int32_t node_id, 
                                   ramd_node_state_t new_state);
bool ramd_cluster_update_node_role(ramd_cluster_t *cluster, int32_t node_id, 
                                  ramd_role_t new_role);
bool ramd_cluster_update_node_health(ramd_cluster_t *cluster, int32_t node_id, 
                                    float health_score);

/* Cluster state queries */
bool ramd_cluster_has_quorum(const ramd_cluster_t *cluster);
bool ramd_cluster_has_primary(const ramd_cluster_t *cluster);
bool ramd_cluster_has_leader(const ramd_cluster_t *cluster);
int32_t ramd_cluster_count_healthy_nodes(const ramd_cluster_t *cluster);
int32_t ramd_cluster_count_standby_nodes(const ramd_cluster_t *cluster);

/* Topology management */
bool ramd_cluster_detect_topology_change(ramd_cluster_t *cluster);
void ramd_cluster_update_topology(ramd_cluster_t *cluster);
void ramd_cluster_print_topology(const ramd_cluster_t *cluster);

#endif /* RAMD_CLUSTER_H */
