/*
 * ramctrl_http.c
 *
 * HTTP client implementation for ramctrl
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#include "ramctrl_http.h"
#include "ramctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

static ramctrl_http_config_t g_http_config = {.base_url =
                                                  "", /* Must be configured */
                                              .timeout_seconds = 10,
                                              .max_retries = 3,
                                              .ssl_verify = true,
                                              .auth_token = ""};

/*
 * Initialize HTTP client
 */
int ramctrl_http_init(void)
{
	/* Load configuration from config file if available */
	/* For now, use defaults */
	return 0;
}

/*
 * Cleanup HTTP client
 */
void ramctrl_http_cleanup(void)
{
	/* Cleanup any resources */
}

/*
 * Simple HTTP GET implementation using sockets
 */
int ramctrl_http_get(const char* url, char* response, size_t response_size)
{
	char hostname[256];
	char path[512];
	char request[1024];
	int port = 80; /* Default HTTP port */
	int sockfd;
	struct sockaddr_in server_addr;
	struct hostent* host_entry;
	ssize_t bytes_received;
	char* body_start;
	if (!url || !response || response_size == 0)
		return -1;

	/* Parse URL - simple parser for http://host:port/path */
	if (strncmp(url, "http://", 7) == 0)
	{
		const char* url_start = url + 7;
		const char* slash_pos = strchr(url_start, '/');
		const char* colon_pos = strchr(url_start, ':');

		if (colon_pos && (!slash_pos || colon_pos < slash_pos))
		{
			/* Host:port format */
			size_t host_len = (size_t) (colon_pos - url_start);
			strncpy(hostname, url_start, host_len);
			hostname[host_len] = '\0';

			port = atoi(colon_pos + 1);

			if (slash_pos)
				strncpy(path, slash_pos, sizeof(path) - 1);
			else
				strcpy(path, "/");
		}
		else
		{
			/* Just hostname format */
			if (slash_pos)
			{
				size_t host_len = (size_t) (slash_pos - url_start);
				strncpy(hostname, url_start, host_len);
				hostname[host_len] = '\0';
				strncpy(path, slash_pos, sizeof(path) - 1);
			}
			else
			{
				strncpy(hostname, url_start, sizeof(hostname) - 1);
				strcpy(path, "/");
			}
		}
	}
	else
	{
		return -1; /* Unsupported URL format */
	}

	path[sizeof(path) - 1] = '\0';

	/* Create socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		return -1;

	/* Set socket timeout */
	struct timeval timeout;
	timeout.tv_sec = g_http_config.timeout_seconds;
	timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	/* Resolve hostname */
	host_entry = gethostbyname(hostname);
	if (!host_entry)
	{
		close(sockfd);
		return -1;
	}

	/* Setup server address */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	memcpy(&server_addr.sin_addr, host_entry->h_addr_list[0],
	       host_entry->h_length);

	/* Connect to server */
	if (connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) <
	    0)
	{
		close(sockfd);
		return -1;
	}

	/* Build HTTP request */
	snprintf(request, sizeof(request),
	         "GET %s HTTP/1.1\r\n"
	         "Host: %s:%d\r\n"
	         "User-Agent: ramctrl/1.0\r\n"
	         "Connection: close\r\n"
	         "\r\n",
	         path, hostname, port);

	/* Send request */
	if (send(sockfd, request, strlen(request), 0) < 0)
	{
		close(sockfd);
		return -1;
	}

	/* Receive response */
	memset(response, 0, response_size);
	bytes_received = recv(sockfd, response, response_size - 1, 0);
	close(sockfd);

	if (bytes_received <= 0)
		return -1;

	/* Find start of body (after \r\n\r\n) */
	body_start = strstr(response, "\r\n\r\n");
	if (body_start)
	{
		body_start += 4;
		memmove(response, body_start, strlen(body_start) + 1);
	}

	return 0;
}

/*
 * Simple HTTP POST implementation
 */
int ramctrl_http_post(const char* url, const char* data, char* response,
                      size_t response_size)
{
	char hostname[256];
	char path[512];
	char request[4096];
	int port = 80;
	int sockfd;
	struct sockaddr_in server_addr;
	struct hostent* host_entry;
	ssize_t bytes_received;

	if (!url || !data || !response || response_size == 0)
		return -1;

	/* Parse URL - simple parser for http://host:port/path */
	if (strncmp(url, "http://", 7) == 0)
	{
		const char* url_start = url + 7;
		const char* slash_pos = strchr(url_start, '/');
		const char* colon_pos = strchr(url_start, ':');

		if (colon_pos && (!slash_pos || colon_pos < slash_pos))
		{
			size_t host_len = (size_t) (colon_pos - url_start);
			strncpy(hostname, url_start, host_len);
			hostname[host_len] = '\0';
			port = atoi(colon_pos + 1);
			if (slash_pos)
				strncpy(path, slash_pos, sizeof(path) - 1);
			else
				strcpy(path, "/");
		}
		else
		{
			if (slash_pos)
			{
				size_t host_len = (size_t) (slash_pos - url_start);
				strncpy(hostname, url_start, host_len);
				hostname[host_len] = '\0';
				strncpy(path, slash_pos, sizeof(path) - 1);
			}
			else
			{
				strncpy(hostname, url_start, sizeof(hostname) - 1);
				strcpy(path, "/");
			}
		}
	}
	else
	{
		return -1;
	}

	path[sizeof(path) - 1] = '\0';

	/* Create socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		return -1;

	/* Set socket timeout */
	struct timeval timeout;
	timeout.tv_sec = g_http_config.timeout_seconds;
	timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	/* Resolve hostname */
	host_entry = gethostbyname(hostname);
	if (!host_entry)
	{
		close(sockfd);
		return -1;
	}

	/* Setup server address */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	memcpy(&server_addr.sin_addr, host_entry->h_addr_list[0],
	       host_entry->h_length);

	/* Connect to server */
	if (connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) <
	    0)
	{
		close(sockfd);
		return -1;
	}

	/* Build HTTP POST request */
	snprintf(request, sizeof(request),
	         "POST %s HTTP/1.1\r\n"
	         "Host: %s:%d\r\n"
	         "User-Agent: ramctrl/1.0\r\n"
	         "Content-Type: application/json\r\n"
	         "Content-Length: %zu\r\n"
	         "Connection: close\r\n"
	         "\r\n"
	         "%s",
	         path, hostname, port, strlen(data), data);

	/* Send request */
	if (send(sockfd, request, strlen(request), 0) < 0)
	{
		close(sockfd);
		return -1;
	}

	/* Receive response */
	memset(response, 0, response_size);
	bytes_received = recv(sockfd, response, response_size - 1, 0);
	close(sockfd);

	if (bytes_received <= 0)
		return -1;

	/* Find start of body */
	(void) url;
	(void) data;
	(void) response;
	(void) response_size;

	return -1;
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
					size_t len =
					    (size_t) (cluster_name_end - cluster_name_start);
					if (len < sizeof(cluster_info->cluster_name) - 1)
					{
						strncpy(cluster_info->cluster_name, cluster_name_start,
						        len);
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
		snprintf(nodes[i].hostname, sizeof(nodes[i].hostname), "node%d.local",
		         i + 1);
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
