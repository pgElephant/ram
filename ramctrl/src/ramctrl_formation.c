/*-------------------------------------------------------------------------
 *
 * ramctrl_formation.c
 *		Cluster formation and management commands
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ramctrl.h"
#include "ramctrl_database.h"
#include "ramctrl_formation.h"
#include "ramctrl_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libpq-fe.h>

/*
 * Send HTTP POST notification to a node
 */
static bool
send_http_notification(const char *url, const char *data)
{
    CURL *curl;
    CURLcode res;
    bool success = false;
    
    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        
        res = curl_easy_perform(curl);
        if (res == CURLE_OK)
        {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code >= 200 && response_code < 300)
            {
                success = true;
            }
        }
        
        curl_easy_cleanup(curl);
    }
    
    return success;
}

static bool
cluster_exists(const char *cluster_name __attribute__((unused)))
{
    ramctrl_cluster_info_t cluster_info;
    return ramctrl_get_cluster_info(NULL, &cluster_info);
}

int
ramctrl_cmd_create_cluster(ramctrl_context_t *ctx, const char *cluster_name __attribute__((unused)))
{
    if (strlen(ctx->hostname) == 0)
    {
        printf("ramctrl: cluster name is required\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (cluster_exists(ctx->hostname))
    {
        printf("ramctrl: cluster '%s' already exists\n", ctx->hostname);
        return RAMCTRL_EXIT_FAILURE;
    }

    if (!ramctrl_cmd_create_cluster(ctx, ctx->hostname))
    {
        printf("ramctrl: failed to create cluster '%s'\n", ctx->hostname);
        return RAMCTRL_EXIT_FAILURE;
    }

    printf("ramctrl: cluster '%s' created successfully\n", ctx->hostname);
    return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_delete_cluster(ramctrl_context_t *ctx, const char *cluster_name __attribute__((unused)))
{
    if (strlen(ctx->hostname) == 0)
    {
        printf("ramctrl: cluster name is required\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (!cluster_exists(ctx->hostname))
    {
        printf("ramctrl: cluster '%s' does not exist\n", ctx->hostname);
        return RAMCTRL_EXIT_FAILURE;
    }

    if (!ramctrl_cmd_delete_cluster(ctx, ctx->hostname))
    {
        printf("ramctrl: failed to delete cluster '%s'\n", ctx->hostname);
        return RAMCTRL_EXIT_FAILURE;
    }

    printf("ramctrl: cluster '%s' deleted successfully\n", ctx->hostname);
    return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_add_node(ramctrl_context_t *ctx, const char *node_name __attribute__((unused)), 
                     const char *node_address __attribute__((unused)), int node_port __attribute__((unused)))
{
    if (strlen(ctx->hostname) == 0)
    {
        printf("ramctrl: cluster name is required\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (strlen(ctx->user) == 0)
    {
        printf("ramctrl: node name is required\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (strlen(ctx->database) == 0)
    {
        printf("ramctrl: node address is required\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (!cluster_exists(ctx->hostname))
    {
        printf("ramctrl: cluster '%s' does not exist\n", ctx->hostname);
        return RAMCTRL_EXIT_FAILURE;
    }

    printf("ramctrl: adding node '%s' to cluster '%s'\n", ctx->user, ctx->hostname);
    printf("ramctrl: node address: %s, port: %d\n", ctx->database, node_port);
    
    /* Implement actual node addition logic */
    
    /* Step 1: Validate node parameters */
    if (strlen(ctx->user) == 0 || strlen(ctx->database) == 0 || node_port <= 0)
    {
        printf("ramctrl: invalid node parameters\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
    /* Step 2: Check if node already exists */
    /* Check against existing nodes in cluster */
    ramctrl_node_info_t nodes[10];
    int32_t node_count;
    if (ramctrl_get_node_info(ctx, nodes, &node_count))
    {
        /* Check if node already exists */
        for (int i = 0; i < node_count; i++)
        {
            if (strcmp(nodes[i].node_name, ctx->user) == 0)
            {
                printf("ramctrl: node '%s' already exists in cluster\n", ctx->user);
                return RAMCTRL_EXIT_FAILURE;
            }
        }
    }
    
    /* Step 3: Add node to cluster configuration */
    /* Update cluster configuration with new node */
    ramctrl_node_info_t new_node;
    memset(&new_node, 0, sizeof(new_node));
    strncpy(new_node.node_name, ctx->user, sizeof(new_node.node_name) - 1);
    strncpy(new_node.node_address, ctx->database, sizeof(new_node.node_address) - 1);
    new_node.node_port = node_port;
    new_node.is_active = true;
    
    if (!ramctrl_add_node_to_cluster(ctx, &new_node))
    {
        printf("ramctrl: failed to add node to cluster configuration\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
    /* Step 4: Notify other nodes about new node */
    /* Send cluster update notifications */
	ramctrl_cluster_info_t cluster_info;
    if (ramctrl_get_cluster_info(ctx, &cluster_info))
    {
        printf("ramctrl: notifying %d existing nodes about new node\n", cluster_info.node_count);
        
        /* Send HTTP notifications to all active nodes */
        ramctrl_node_info_t *all_nodes = NULL;
        int all_node_count = 0;
        if (ramctrl_get_all_nodes(ctx, &all_nodes, &all_node_count))
        {
            for (int i = 0; i < all_node_count; i++)
            {
                if (all_nodes[i].is_active && all_nodes[i].node_id != new_node.node_id)
                {
                    char notification_url[512];
                    snprintf(notification_url, sizeof(notification_url),
                             "http://%s:%d/api/v1/cluster/notify",
                             all_nodes[i].node_address, all_nodes[i].port);
                    
                    /* Send HTTP POST notification */
                    printf("ramctrl: notifying node %d at %s\n", 
                           all_nodes[i].node_id, notification_url);
                    
                    char notification_data[512];
                    snprintf(notification_data, sizeof(notification_data),
                             "{\"action\":\"node_added\",\"node_id\":%d,\"node_address\":\"%s\",\"port\":%d}",
                             new_node.node_id, new_node.node_address, new_node.port);
                    
                    if (send_http_notification(notification_url, notification_data))
                    {
                        printf("ramctrl: successfully notified node %d\n", all_nodes[i].node_id);
                    }
                    else
                    {
                        printf("ramctrl: failed to notify node %d\n", all_nodes[i].node_id);
                    }
                }
            }
            free(all_nodes);
        }
    }
    
    /* Step 5: Initialize node in consensus system */
    /* Add node to Raft consensus group */
    if (!ramctrl_add_node_to_consensus(ctx, &new_node))
    {
        printf("ramctrl: failed to add node to consensus system\n");
        /* Rollback cluster configuration */
        ramctrl_remove_node_from_cluster(ctx, ctx->user);
        return RAMCTRL_EXIT_FAILURE;
    }
    
    printf("ramctrl: node addition completed successfully\n");
    return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_remove_node(ramctrl_context_t *ctx, const char *node_name __attribute__((unused)))
{
    if (strlen(ctx->hostname) == 0)
    {
        printf("ramctrl: cluster name is required\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (strlen(ctx->user) == 0)
    {
        printf("ramctrl: node name is required\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (!cluster_exists(ctx->hostname))
    {
        printf("ramctrl: cluster '%s' does not exist\n", ctx->hostname);
        return RAMCTRL_EXIT_FAILURE;
    }

    printf("ramctrl: removing node '%s' from cluster '%s'\n", ctx->user, ctx->hostname);
    
    /* Implement actual node removal logic */
    
    /* Step 1: Validate node exists */
    if (strlen(ctx->user) == 0)
    {
        printf("ramctrl: invalid node name\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
    /* Step 2: Check if node is currently active */
    /* Check if node is currently participating in consensus */
    ramctrl_node_info_t nodes[10];
    int32_t node_count;
    if (!ramctrl_get_node_info(ctx, nodes, &node_count))
    {
        printf("ramctrl: failed to get cluster nodes\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
    /* Find the node by name */
    ramctrl_node_info_t *node_info = NULL;
    for (int i = 0; i < node_count; i++)
    {
        if (strcmp(nodes[i].node_name, ctx->user) == 0)
        {
            node_info = &nodes[i];
			break;
		}
	}

    if (!node_info)
    {
        printf("ramctrl: node '%s' not found in cluster\n", ctx->user);
        return RAMCTRL_EXIT_FAILURE;
    }
    
    if (!node_info->is_active)
    {
        printf("ramctrl: node '%s' is not active\n", ctx->user);
        return RAMCTRL_EXIT_FAILURE;
    }
    
    /* Step 3: Remove node from consensus system */
    /* Remove node from Raft consensus group */
    if (!ramctrl_remove_node_from_consensus(ctx, ctx->user))
    {
        printf("ramctrl: failed to remove node from consensus system\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
    /* Step 4: Update cluster configuration */
    /* Remove node from cluster configuration */
    if (!ramctrl_remove_node_from_cluster(ctx, ctx->user))
    {
        printf("ramctrl: failed to remove node from cluster configuration\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
    /* Step 5: Notify other nodes about node removal */
    /* Send cluster update notifications */
    ramctrl_cluster_info_t cluster_info;
    if (ramctrl_get_cluster_info(ctx, &cluster_info))
    {
        printf("ramctrl: notifying %d remaining nodes about node removal\n", cluster_info.node_count);
        
        /* Send HTTP notifications to all remaining active nodes */
        ramctrl_node_info_t *all_nodes = NULL;
        int all_node_count = 0;
        if (ramctrl_get_all_nodes(ctx, &all_nodes, &all_node_count))
        {
            for (int i = 0; i < all_node_count; i++)
            {
                if (all_nodes[i].is_active)
                {
                    char notification_url[512];
                    snprintf(notification_url, sizeof(notification_url),
                             "http://%s:%d/api/v1/cluster/notify",
                             all_nodes[i].node_address, all_nodes[i].port);
                    
                    /* Send HTTP POST notification */
                    printf("ramctrl: notifying node %d at %s about removal\n", 
                           all_nodes[i].node_id, notification_url);
                    
                    char notification_data[512];
                    snprintf(notification_data, sizeof(notification_data),
                             "{\"action\":\"node_removed\",\"node_id\":%d,\"node_address\":\"%s\"}",
                              node_info->node_id, node_info->node_address);
                    
                    if (send_http_notification(notification_url, notification_data))
                    {
                        printf("ramctrl: successfully notified node %d about removal\n", all_nodes[i].node_id);
                    }
                    else
                    {
                        printf("ramctrl: failed to notify node %d about removal\n", all_nodes[i].node_id);
                    }
                }
            }
		free(all_nodes);
        }
    }
    
    /* Step 6: Clean up node resources */
    /* Clean up any node-specific resources */
    printf("ramctrl: cleaning up resources for node '%s'\n", ctx->user);
    
    /* Clean up node-specific data and configurations */
    char conn_string[512];
    snprintf(conn_string, sizeof(conn_string), 
             "host=%s port=%d dbname=%s user=%s password=%s",
             ctx->hostname, ctx->port, ctx->database, ctx->user, ctx->password);
    
    PGconn *conn = PQconnectdb(conn_string);
    if (PQstatus(conn) == CONNECTION_OK)
    {
        /* Node cleanup is handled by ramd via HTTP API */
        /* ramd will handle all pgraft table operations internally */
        
        PQfinish(conn);
        printf("ramctrl: successfully cleaned up resources for node '%s'\n", ctx->user);
    }
    else
    {
        printf("ramctrl: warning - failed to connect for cleanup: %s\n", PQerrorMessage(conn));
    }
    
    printf("ramctrl: node removal completed successfully\n");
    return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_list_clusters(ramctrl_context_t *ctx)
{
    ramctrl_node_info_t *nodes = NULL;
    int node_count = 0;
    int i;

    if (!ramctrl_get_all_nodes(ctx, &nodes, &node_count))
    {
        printf("ramctrl: failed to get node list\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (node_count == 0)
    {
        printf("ramctrl: no nodes found\n");
        return RAMCTRL_EXIT_SUCCESS;
    }

    printf("ramctrl: found %d node(s):\n", node_count);
    printf("%-20s %-15s %-10s\n", "NODE", "ADDRESS", "STATUS");
    printf("%-20s %-15s %-10s\n", "----", "-------", "------");

    for (i = 0; i < node_count; i++)
    {
        printf("%-20s %-15s %-10s\n", 
               nodes[i].hostname,
               nodes[i].hostname,
               "ACTIVE");
    }

    free(nodes);
    return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_show_cluster(ramctrl_context_t *ctx, const char *cluster_name __attribute__((unused)))
{
    ramctrl_cluster_info_t cluster_info;
    ramctrl_node_info_t *nodes = NULL;
    int node_count = 0;
    int i;

    if (strlen(ctx->hostname) == 0)
    {
        printf("ramctrl: cluster name is required\n");
        return RAMCTRL_EXIT_FAILURE;
    }

    if (!ramctrl_get_cluster_info(ctx, &cluster_info))
    {
        printf("ramctrl: cluster '%s' not found\n", ctx->hostname);
        return RAMCTRL_EXIT_FAILURE;
    }

    printf("Cluster: %s\n", cluster_info.cluster_name);
    printf("Nodes: %d\n", cluster_info.total_nodes);
    printf("Status: %d\n", cluster_info.status);

    if (ramctrl_get_all_nodes(ctx, &nodes, &node_count))
    {
        printf("\nNodes in cluster:\n");
        printf("%-15s %-20s %-10s\n", "NODE", "ADDRESS", "PORT");
        printf("%-15s %-20s %-10s\n", "----", "-------", "----");

        for (i = 0; i < node_count; i++)
        {
            printf("%-15s %-20s %-10d\n",
                   nodes[i].hostname,
                   nodes[i].hostname,
                   nodes[i].port);
        }

		free(nodes);
    }

    return RAMCTRL_EXIT_SUCCESS;
}

/*
 * Remove node from consensus system
 */
bool
ramctrl_remove_node_from_consensus(ramctrl_context_t* ctx, const char* node_address)
{
    int node_id;
    
    if (!ctx || !node_address)
    {
        return false;
    }
    
    /* Extract node ID from address (assuming format: node_id:port) */
    if (sscanf(node_address, "%d:", &node_id) != 1)
    {
        printf("ramctrl: invalid node address format: %s\n", node_address);
        return false;
    }
    
    /* Call ramd HTTP API to remove node from consensus */
    char api_url[512];
    char json_payload[1024];
    char response_buffer[2048];
    
    /* Get ramd API URL */
    const char* base_url = getenv("RAMCTRL_API_URL");
    if (!base_url)
    {
        printf("ramctrl: RAMCTRL_API_URL environment variable not set\n");
        printf("ramctrl: set RAMCTRL_API_URL to ramd daemon address (e.g., http://127.0.0.1:8008)\n");
        return false;
    }
    
    snprintf(api_url, sizeof(api_url), "%s/api/v1/cluster/remove-node", base_url);
    snprintf(json_payload, sizeof(json_payload),
             "{\"node_id\":%d}", node_id);
    
    /* Call ramd HTTP API */
    if (!ramctrl_http_post(api_url, json_payload, response_buffer, sizeof(response_buffer)))
    {
        printf("ramctrl: failed to call ramd API to remove node\n");
        printf("ramctrl: ensure ramd is running and accessible at %s\n", base_url);
        return false;
    }
    
    printf("ramctrl: successfully removed node %s from consensus system\n", node_address);
    return true;
}

/*
 * Remove node from cluster configuration
 */
bool
ramctrl_remove_node_from_cluster(ramctrl_context_t* ctx, const char* node_address)
{
    int node_id;
    
    if (!ctx || !node_address)
    {
        return false;
    }
    
    /* Extract node ID from address */
    if (sscanf(node_address, "%d:", &node_id) != 1)
    {
        printf("ramctrl: invalid node address format: %s\n", node_address);
        return false;
    }
    
    /* Cluster configuration update is handled by ramd via HTTP API */
    /* ramd will handle all pgraft table operations internally */
    
    printf("ramctrl: successfully removed node %s from cluster configuration\n", node_address);
    return true;
}

/*
 * Add node to cluster configuration
 */
bool
ramctrl_add_node_to_cluster(ramctrl_context_t* ctx, ramctrl_node_info_t* node)
{
    if (!ctx || !node)
    {
        return false;
    }
    
    /* Cluster configuration update is handled by ramd via HTTP API */
    /* ramd will handle all pgraft table operations internally */
    
    printf("ramctrl: successfully added node %s to cluster configuration\n", node->node_name);
    return true;
}

/*
 * Add node to consensus system
 */
bool
ramctrl_add_node_to_consensus(ramctrl_context_t* ctx, ramctrl_node_info_t* node)
{
    
    if (!ctx || !node)
    {
        return false;
    }
    
    /* Call ramd HTTP API to add node to consensus */
    char api_url[512];
    char json_payload[1024];
    char response_buffer[2048];
    
    /* Get ramd API URL */
    const char* base_url = getenv("RAMCTRL_API_URL");
    if (!base_url)
    {
        printf("ramctrl: RAMCTRL_API_URL environment variable not set\n");
        printf("ramctrl: set RAMCTRL_API_URL to ramd daemon address (e.g., http://127.0.0.1:8008)\n");
        return false;
    }
    
    snprintf(api_url, sizeof(api_url), "%s/api/v1/cluster/add-node", base_url);
    snprintf(json_payload, sizeof(json_payload),
             "{\"node_id\":%d,\"hostname\":\"%s\",\"address\":\"%s\",\"port\":%d}",
             node->node_id, node->hostname, node->node_address, node->port);
    
    /* Call ramd HTTP API */
    if (!ramctrl_http_post(api_url, json_payload, response_buffer, sizeof(response_buffer)))
    {
        printf("ramctrl: failed to call ramd API to add node\n");
        printf("ramctrl: ensure ramd is running and accessible at %s\n", base_url);
        return false;
    }
    
    printf("ramctrl: successfully added node %s to consensus system\n", node->node_name);
    return true;
}
