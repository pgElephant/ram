/*
 * ram/ramctrl/src/ramctrl_formation.c
 *   Implementation of ramctrl cluster/formation functionality
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * Cluster formation and management functionality for ramctrl.
 */

#include "ramctrl.h"
#include "ramctrl_database.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

bool cluster_exists(const char *cluster_name)
{
    ramctrl_context_t ctx;
    ramctrl_cluster_info_t cluster_info;
    
    if (cluster_name == NULL || strlen(cluster_name) == 0)
        return false;
    
    /* Initialize context for database operations */
    memset(&ctx, 0, sizeof(ctx));
    
    /* Check if cluster exists in database */
    if (!ramctrl_get_cluster_info(&ctx, &cluster_info))
        return false;
    
    return (strcmp(cluster_info.cluster_name, cluster_name) == 0);
}

bool node_exists_in_cluster(const char *cluster_name, const char *node_name)
{
    ramctrl_context_t ctx;
    ramctrl_node_info_t *nodes = NULL;
    int node_count = 0;
    int i;
    bool found = false;
    
    if (cluster_name == NULL || node_name == NULL)
        return false;
    
    /* Initialize context for database operations */
    memset(&ctx, 0, sizeof(ctx));
    
    /* Get all nodes in the cluster */
    if (!ramctrl_get_all_nodes(&ctx, &nodes, &node_count))
        return false;
    
    /* Search for the node by name */
    for (i = 0; i < node_count; i++)
    {
        if (strcmp(nodes[i].hostname, node_name) == 0)
        {
            found = true;
            break;
        }
    }
    
    if (nodes)
        free(nodes);
    
    return found;
}
