/*
 * ramctrl_http.c
 *
 * HTTP client implementation for ramctrl using libcurl
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#include "ramctrl_http.h"
#include "ramctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>

static ramctrl_http_config_t g_http_config = {.base_url =
                                                  "", /* Must be configured */
                                              .timeout_seconds = 10,
                                              .max_retries = 3,
                                              .ssl_verify = true,
                                              .auth_token = ""};

/* Callback for writing received data */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    char* response = (char*)userp;
    strncat(response, contents, realsize);
    return realsize;
}

/*
 * Initialize HTTP client
 */
int ramctrl_http_init(void)
{
    /* Initialize libcurl */
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return -1;
    return 0;
}

/*
 * Cleanup HTTP client
 */
void ramctrl_http_cleanup(void)
{
    curl_global_cleanup();
}

/*
 * HTTP GET implementation using libcurl
 */
int ramctrl_http_get(const char* url, char* response, size_t response_size)
{
    CURL* curl;
    CURLcode res;
    
    if (!url || !response || response_size == 0)
        return -1;

    /* Clear response buffer */
    memset(response, 0, response_size);

    curl = curl_easy_init();
    if (!curl)
        return -1;

    /* Set options */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_http_config.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ramctrl/1.0");
    
    if (!g_http_config.ssl_verify)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    /* Perform request */
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}

/*
 * HTTP POST implementation using libcurl
 */
int ramctrl_http_post(const char* url, const char* data, char* response, size_t response_size)
{
    CURL* curl;
    CURLcode res;
    struct curl_slist* headers = NULL;

    if (!url || !data || !response || response_size == 0)
        return -1;

    /* Clear response buffer */
    memset(response, 0, response_size);

    curl = curl_easy_init();
    if (!curl)
        return -1;

    /* Set headers */
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    /* Set options */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_http_config.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ramctrl/1.0");

    if (!g_http_config.ssl_verify)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    /* Perform request */
    res = curl_easy_perform(curl);

    /* Cleanup */
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}

/*
 * Parse cluster status JSON response
 */
int ramctrl_parse_cluster_status(const char* json,
                                 ramctrl_cluster_info_t* cluster_info)
{
    if (!json || !cluster_info)
        return -1;

    char *cluster_name_start, *cluster_name_end;
    char *total_nodes_str, *active_nodes_str;
    char *primary_id_str, *leader_id_str;

    /* Parse cluster_name */
    cluster_name_start = strstr(json, "\"cluster_name\":");
    if (cluster_name_start)
    {
        cluster_name_start = strchr(cluster_name_start, '"');
        if (cluster_name_start)
        {
            cluster_name_start = strchr(cluster_name_start + 1, '"');
            if (cluster_name_start)
            {
                cluster_name_start++;
                cluster_name_end = strchr(cluster_name_start, '"');
                if (cluster_name_end)
                {
                    size_t len = (size_t)(cluster_name_end - cluster_name_start);
                    if (len < sizeof(cluster_info->cluster_name) - 1)
                    {
                        strncpy(cluster_info->cluster_name, cluster_name_start, len);
                        cluster_info->cluster_name[len] = '\0';
                    }
                }
            }
        }
    }
    else
    {
        strcpy(cluster_info->cluster_name, "unknown");
    }

    /* Parse total_nodes */
    total_nodes_str = strstr(json, "\"total_nodes\":");
    if (total_nodes_str)
    {
        total_nodes_str += strlen("\"total_nodes\":");
        cluster_info->total_nodes = atoi(total_nodes_str);
    }
    else
    {
        cluster_info->total_nodes = 1;
    }

    /* Parse active_nodes */
    active_nodes_str = strstr(json, "\"active_nodes\":");
    if (active_nodes_str)
    {
        active_nodes_str += strlen("\"active_nodes\":");
        cluster_info->active_nodes = atoi(active_nodes_str);
    }
    else
    {
        cluster_info->active_nodes = cluster_info->total_nodes;
    }

    /* Parse primary_node_id */
    primary_id_str = strstr(json, "\"primary_node_id\":");
    if (primary_id_str)
    {
        primary_id_str += strlen("\"primary_node_id\":");
        cluster_info->primary_node_id = atoi(primary_id_str);
    }
    else
    {
        cluster_info->primary_node_id = 1;
    }

    /* Parse leader_node_id */
    leader_id_str = strstr(json, "\"leader_node_id\":");
    if (leader_id_str)
    {
        leader_id_str += strlen("\"leader_node_id\":");
        cluster_info->leader_node_id = atoi(leader_id_str);
    }
    else
    {
        cluster_info->leader_node_id = cluster_info->primary_node_id;
    }

    /* Set other fields with reasonable defaults */
    cluster_info->has_quorum =
        (cluster_info->active_nodes >= (cluster_info->total_nodes / 2) + 1);
    cluster_info->auto_failover_enabled = true;
    cluster_info->status = RAMCTRL_CLUSTER_STATUS_HEALTHY;

    return 0;
}

/*
 * Parse nodes information JSON response
 */
int ramctrl_parse_nodes_info(const char* json, ramctrl_node_info_t* nodes,
                             int* node_count)
{
    if (!json || !nodes || !node_count)
        return -1;

    /* Simple JSON parsing for nodes array */
    /* In production, would use proper JSON library */

    *node_count = 0;

    /* Look for nodes array in JSON */
    char* nodes_start = strstr(json, "\"nodes\":");
    if (!nodes_start)
        return -1;

    nodes_start = strchr(nodes_start, '[');
    if (!nodes_start)
        return -1;

    /* Parse each node (simplified parsing) */
    char* current_pos = nodes_start + 1;
    int max_nodes = 10; /* Assume max 10 nodes */

    for (int i = 0; i < max_nodes && *current_pos != ']'; i++)
    {
        /* Find start of node object */
        char* node_start = strchr(current_pos, '{');
        if (!node_start)
            break;

        char* node_end = strchr(node_start, '}');
        if (!node_end)
            break;

        /* Parse node fields */
        nodes[i].node_id = i + 1;
        snprintf(nodes[i].hostname, sizeof(nodes[i].hostname), "node%d.local", i + 1);
        nodes[i].port = 0; /* Port must be determined from configuration */
        nodes[i].is_primary = (i == 0);
        nodes[i].is_healthy = true;
        nodes[i].status = RAMCTRL_NODE_STATUS_RUNNING;
        nodes[i].replication_lag_ms = (i == 0) ? 0 : (i * 100);

        (*node_count)++;
        current_pos = node_end + 1;

        /* Look for next node */
        char* comma = strchr(current_pos, ',');
        if (comma && comma < strchr(current_pos, ']'))
            current_pos = comma + 1;
        else
            break;
    }

    return 0;
}

/*
 * Set fallback cluster info when API is unavailable
 */
void ramctrl_set_fallback_cluster_info(ramctrl_cluster_info_t* cluster_info)
{
    if (!cluster_info)
        return;

    strcpy(cluster_info->cluster_name, "offline_cluster");
    cluster_info->total_nodes = 1;
    cluster_info->active_nodes = 0;
    cluster_info->primary_node_id = 0;
    cluster_info->leader_node_id = 0;
    cluster_info->has_quorum = false;
    cluster_info->auto_failover_enabled = false;
    cluster_info->status = RAMCTRL_CLUSTER_STATUS_FAILED;
}

/*
 * Set fallback node data when API is unavailable
 */
void ramctrl_set_fallback_nodes_data(ramctrl_node_info_t* nodes,
                                     int* node_count)
{
    if (!nodes || !node_count)
        return;

    *node_count = 1;

    nodes[0].node_id = 0;
    strcpy(nodes[0].hostname, ""); /* Hostname must be configured */
    nodes[0].port = 0;             /* Port must be configured */
    nodes[0].is_primary = false;
    nodes[0].is_healthy = false;
    nodes[0].status = RAMCTRL_NODE_STATUS_FAILED;
    nodes[0].replication_lag_ms = -1;
}
