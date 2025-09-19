/*
 * ramctrl_formation.h
 * Cluster formation and management commands for ramctrl
 *
 * Copyright (c) 2024-2025, pgElephant, Inc. All rights reserved.
 */

#ifndef RAMCTRL_FORMATION_H
#define RAMCTRL_FORMATION_H

#include "ramctrl.h"

/* Function declarations */
int ramctrl_cmd_create_cluster(ramctrl_context_t *ctx, const char *cluster_name);
int ramctrl_cmd_delete_cluster(ramctrl_context_t *ctx, const char *cluster_name);
int ramctrl_cmd_add_node(ramctrl_context_t *ctx, const char *node_name, 
                         const char *node_address, int node_port);
int ramctrl_cmd_remove_node(ramctrl_context_t *ctx, const char *node_name);
int ramctrl_cmd_list_clusters(ramctrl_context_t *ctx);
int ramctrl_cmd_show_cluster(ramctrl_context_t *ctx, const char *cluster_name);

#endif /* RAMCTRL_FORMATION_H */
