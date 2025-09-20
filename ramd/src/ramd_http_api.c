/*-------------------------------------------------------------------------
 *
 * ramd_http_api.c
 *		PostgreSQL Auto-Failover Daemon - HTTP REST API Implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <libpq-fe.h>
#include <signal.h>

#include "ramd_http_api.h"
#include "ramd_logging.h"
#include "ramd_postgresql.h"
#include "ramd_cluster.h"
#include "ramd_conn.h"
#include "ramd_query.h"
#include "ramd_config_reload.h"
#include "ramd_sync_replication.h"
#include "ramd_maintenance.h"
#include "ramd_metrics.h"
#include "ramd_daemon.h"
#include "ramd_failover.h"

extern ramd_daemon_t *g_ramd_daemon;
extern PGconn *g_conn;

static ramd_http_server_t *g_http_server = NULL;

static void *ramd_http_server_thread(void *arg);
static void *ramd_http_connection_handler(void *arg);
static void ramd_http_route_request(ramd_http_request_t *request, ramd_http_response_t *response);
static int get_healthy_nodes_count(void);

bool
ramd_http_server_init(ramd_http_server_t *server, const char *bind_address, int port)
{
	if (!server)
		return false;

	memset(server, 0, sizeof(ramd_http_server_t));

	server->port = port > 0 ? port : g_ramd_daemon->config.http_port;
	if (bind_address && strlen(bind_address) > 0)
		strncpy(server->bind_address, bind_address, sizeof(server->bind_address) - 1);
	else
		server->bind_address[0] = '\0';

	server->listen_fd = -1;
	server->running = false;
	server->auth_enabled = false;

	if (pthread_mutex_init(&server->mutex, NULL) != 0)
	{
		ramd_log_error("Failed to initialize HTTP server mutex");
		return false;
	}

	ramd_log_info("HTTP API server initialized on %s:%d", server->bind_address, server->port);
	return true;
}

bool
ramd_http_server_start(ramd_http_server_t *server)
{
	struct sockaddr_in server_addr;
	int                opt = 1;

	if (!server)
		return false;

	server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server->listen_fd < 0)
	{
		ramd_log_error("Failed to create HTTP server socket: %s", strerror(errno));
		return false;
	}

	if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		ramd_log_warning("Failed to set SO_REUSEADDR: %s", strerror(errno));

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons((uint16_t) server->port);

	if (inet_pton(AF_INET, server->bind_address, &server_addr.sin_addr) <= 0)
	{
		ramd_log_error("Invalid bind address: %s", server->bind_address);
		close(server->listen_fd);
		return false;
	}

	if (bind(server->listen_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
	{
		ramd_log_error("Failed to bind HTTP server socket: %s", strerror(errno));
		close(server->listen_fd);
		return false;
	}

	if (listen(server->listen_fd, RAMD_HTTP_MAX_CONNECTIONS) < 0)
	{
		ramd_log_error("Failed to listen on HTTP server socket: %s", strerror(errno));
		close(server->listen_fd);
		return false;
	}

	server->running = true;
	if (pthread_create(&server->server_thread, NULL, ramd_http_server_thread, server) != 0)
	{
		ramd_log_error("Failed to create HTTP server thread");
		server->running = false;
		close(server->listen_fd);
		return false;
	}

	g_http_server = server;
	ramd_log_info("HTTP API server started on %s:%d", server->bind_address, server->port);
	return true;
}

void
ramd_http_server_stop(ramd_http_server_t *server)
{
	if (!server || !server->running)
		return;

	ramd_log_info("Stopping HTTP API server");

	pthread_mutex_lock(&server->mutex);
	server->running = false;
	pthread_mutex_unlock(&server->mutex);

	if (server->listen_fd >= 0)
	{
		close(server->listen_fd);
		server->listen_fd = -1;
	}

	pthread_join(server->server_thread, NULL);

	g_http_server = NULL;
	ramd_log_info("HTTP API server stopped");
}

void
ramd_http_server_cleanup(ramd_http_server_t *server)
{
	if (!server)
		return;

	ramd_http_server_stop(server);
	pthread_mutex_destroy(&server->mutex);
}

static void *
ramd_http_server_thread(void *arg)
{
	ramd_http_server_t     *server = (ramd_http_server_t *) arg;
	struct sockaddr_in      client_addr;
	socklen_t              client_len = sizeof(client_addr);
	int                    client_fd;
	pthread_t              connection_thread;
	ramd_http_connection_t *connection;

	ramd_log_debug("HTTP server thread started");

	while (server->running)
	{
		client_fd = accept(server->listen_fd, (struct sockaddr *) &client_addr, &client_len);
		if (client_fd < 0)
		{
			if (server->running && errno != EINTR)
				ramd_log_error("HTTP accept failed: %s", strerror(errno));
			continue;
		}

		connection = malloc(sizeof(ramd_http_connection_t));
		if (!connection)
		{
			ramd_log_error("Failed to allocate connection structure");
			close(client_fd);
			continue;
		}

		connection->client_fd = client_fd;
		connection->client_addr = client_addr;
		connection->server = server;

		if (pthread_create(&connection_thread, NULL, ramd_http_connection_handler, connection) != 0)
		{
			ramd_log_error("Failed to create connection handler thread");
			close(client_fd);
			free(connection);
			continue;
		}

		pthread_detach(connection_thread);
	}

	ramd_log_debug("HTTP server thread stopped");
	return NULL;
}

static void *
ramd_http_connection_handler(void *arg)
{
	ramd_http_connection_t *connection = (ramd_http_connection_t *) arg;
	char                   buffer[RAMD_MAX_COMMAND_LENGTH];
	ssize_t                bytes_read;
	ramd_http_request_t    request;
	ramd_http_response_t   response;

	bytes_read = recv(connection->client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read <= 0)
	{
		close(connection->client_fd);
		free(connection);
		return NULL;
	}

	buffer[bytes_read] = '\0';

	if (!ramd_http_parse_request(buffer, &request))
		ramd_http_set_error_response(&response, RAMD_HTTP_400_BAD_REQUEST, "Invalid HTTP request");
	else
		ramd_http_route_request(&request, &response);

	ramd_http_send_response(connection->client_fd, &response);

	close(connection->client_fd);
	free(connection);
	return NULL;
}

bool
ramd_http_parse_request(const char *raw_request, ramd_http_request_t *request)
{
	char *line;
	char *method_str;
	char *path_str;
	char *version_str __attribute__((unused));
	char *saveptr;
	char *request_copy;
	char *query;

	if (!raw_request || !request)
		return false;

	memset(request, 0, sizeof(ramd_http_request_t));

	request_copy = strdup(raw_request);
	if (!request_copy)
		return false;

	line = strtok_r(request_copy, "\r\n", &saveptr);
	if (!line)
	{
		free(request_copy);
		return false;
	}

	method_str = strtok(line, " ");
	path_str = strtok(NULL, " ");
	version_str = strtok(NULL, " ");

	if (!method_str || !path_str)
	{
		free(request_copy);
		return false;
	}

	if (strcmp(method_str, "GET") == 0)
		request->method = RAMD_HTTP_GET;
	else if (strcmp(method_str, "POST") == 0)
		request->method = RAMD_HTTP_POST;
	else if (strcmp(method_str, "PUT") == 0)
		request->method = RAMD_HTTP_PUT;
	else if (strcmp(method_str, "DELETE") == 0)
		request->method = RAMD_HTTP_DELETE;
	else if (strcmp(method_str, "PATCH") == 0)
		request->method = RAMD_HTTP_PATCH;
	else
	{
		free(request_copy);
		return false;
	}

	query = strchr(path_str, '?');
	if (query)
	{
		*query = '\0';
		query++;
		strncpy(request->query_string, query, sizeof(request->query_string) - 1);
	}

	strncpy(request->path, path_str, sizeof(request->path) - 1);

	while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL)
	{
		if (strlen(line) == 0)
			break;

		if (strncasecmp(line, "Authorization:", 14) == 0)
		{
			strncpy(request->authorization, line + 14, sizeof(request->authorization) - 1);
			char *auth = request->authorization;
			while (*auth == ' ' || *auth == '\t')
				auth++;
			memmove(request->authorization, auth, strlen(auth) + 1);
		}
	}

	if (line)
	{
		char *body_start = line + strlen(line) + 2;
		if (body_start < raw_request + strlen(raw_request))
		{
			size_t body_len = strlen(body_start);
			if (body_len < sizeof(request->body))
			{
				strncpy(request->body, body_start, body_len);
				request->body_length = body_len;
			}
		}
	}

	free(request_copy);
	return true;
}

static void
ramd_http_route_request(ramd_http_request_t *request, ramd_http_response_t *response)
{
	if (strcmp(request->path, "/api/v1/cluster/status") == 0)
		ramd_http_handle_cluster_status(request, response);
	else if (strcmp(request->path, "/api/v1/nodes") == 0)
		ramd_http_handle_nodes_list(request, response);
	else if (strncmp(request->path, "/api/v1/nodes/", 14) == 0)
		ramd_http_handle_node_detail(request, response);
	else if (strncmp(request->path, "/api/v1/promote/", 16) == 0)
		ramd_http_handle_promote_node(request, response);
	else if (strncmp(request->path, "/api/v1/demote/", 15) == 0)
		ramd_http_handle_demote_node(request, response);
	else if (strcmp(request->path, "/api/v1/failover") == 0)
		ramd_http_handle_failover(request, response);
	else if (strncmp(request->path, "/api/v1/maintenance/", 20) == 0)
		ramd_http_handle_maintenance_mode(request, response);
	else if (strcmp(request->path, "/api/v1/config/reload") == 0)
		ramd_http_handle_config_reload(request, response);
	else if (strcmp(request->path, "/api/v1/replication/sync") == 0)
		ramd_http_handle_sync_replication(request, response);
	else if (strcmp(request->path, "/api/v1/bootstrap/primary") == 0)
		ramd_http_handle_bootstrap_primary(request, response);
	else if (strcmp(request->path, "/api/v1/replica/add") == 0)
		ramd_http_handle_add_replica(request, response);
	else if (strcmp(request->path, "/metrics") == 0)
		ramd_http_handle_metrics(request, response);
	else
		ramd_http_set_error_response(response, RAMD_HTTP_404_NOT_FOUND, "Endpoint not found");
}

void
ramd_http_handle_cluster_status(ramd_http_request_t *request, ramd_http_response_t *response)
{
	char    json_buffer[RAMD_MAX_COMMAND_LENGTH];
	int32_t pgraft_node_count;
	int32_t pgraft_leader_id;
	bool    pgraft_is_leader;
	bool    pgraft_has_quorum;

	(void) request;

	if (!ramd_postgresql_query_pgraft_cluster_status(&g_ramd_daemon->config, &pgraft_node_count,
												   &pgraft_is_leader, &pgraft_leader_id,
												   &pgraft_has_quorum))
	{
		ramd_log_warning("Failed to query pgraft cluster status, falling back to internal state");

		ramd_cluster_t *cluster = &g_ramd_daemon->cluster;
		if (!cluster)
		{
			ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
									   "Cluster not initialized and pg_ram unavailable");
			return;
		}

		pgraft_node_count = cluster->node_count;
		pgraft_is_leader = (cluster->primary_node_id == g_ramd_daemon->config.node_id);
		pgraft_leader_id = cluster->primary_node_id;
		pgraft_has_quorum = ramd_cluster_has_quorum(cluster);
	}

	snprintf(json_buffer, sizeof(json_buffer),
			"{\n"
			"  \"cluster_name\": \"%s\",\n"
			"  \"status\": \"%s\",\n"
			"  \"primary_node_id\": %d,\n"
			"  \"node_count\": %d,\n"
			"  \"healthy_nodes\": %d,\n"
			"  \"has_quorum\": %s,\n"
			"  \"is_leader\": %s,\n"
			"  \"data_source\": \"pgraft\",\n"
			"  \"timestamp\": %ld,\n"
			"  \"failover_state\": \"%s\"\n"
			"}",
			g_ramd_daemon->cluster.cluster_name,
			pgraft_has_quorum ? "operational" : "degraded",
			pgraft_leader_id,
			pgraft_node_count,
			get_healthy_nodes_count(),
			pgraft_has_quorum ? "true" : "false",
			pgraft_is_leader ? "true" : "false",
			time(NULL),
			(g_ramd_daemon->failover_context.state == RAMD_FAILOVER_STATE_NORMAL) ? "normal" :
			(g_ramd_daemon->failover_context.state == RAMD_FAILOVER_STATE_DETECTING) ? "detecting" :
			(g_ramd_daemon->failover_context.state == RAMD_FAILOVER_STATE_PROMOTING) ? "promoting" :
			(g_ramd_daemon->failover_context.state == RAMD_FAILOVER_STATE_RECOVERING) ? "recovering" :
			(g_ramd_daemon->failover_context.state == RAMD_FAILOVER_STATE_COMPLETED) ? "completed" :
			"failed");

	ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
}

void
ramd_http_handle_config_reload(ramd_http_request_t *request, ramd_http_response_t *response)
{
	ramd_config_reload_result_t result;
	char                       json_buffer[RAMD_MAX_COMMAND_LENGTH];

	if (request->method != RAMD_HTTP_POST)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED, "Method not allowed");
		return;
	}

	if (!ramd_config_reload_from_file(NULL, &result))
	{
		snprintf(json_buffer, sizeof(json_buffer),
				"{\n"
				"  \"status\": \"failed\",\n"
				"  \"error\": \"%s\"\n"
				"}",
				result.error_message);
		ramd_http_set_json_response(response, RAMD_HTTP_500_INTERNAL_ERROR, json_buffer);
		return;
	}

	snprintf(json_buffer, sizeof(json_buffer),
			"{\n"
			"  \"status\": \"%s\",\n"
			"  \"reload_time\": %ld,\n"
			"  \"changes_detected\": %d,\n"
			"  \"changes_applied\": %d\n"
			"}",
			ramd_config_reload_status_to_string(result.status),
			result.reload_time,
			result.changes_detected,
			result.changes_applied);

	ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
}

bool
ramd_http_send_response(int client_fd, ramd_http_response_t *response)
{
	char response_buffer[RAMD_MAX_COMMAND_LENGTH * 2];
	int  response_len;

	if (!response)
		return false;

	response_len = snprintf(response_buffer, sizeof(response_buffer),
						   "HTTP/1.1 %d %s\r\n"
						   "Content-Type: %s\r\n"
						   "Content-Length: %zu\r\n"
						   "Server: ramd/1.0\r\n"
						   "Connection: close\r\n"
						   "%s"
						   "\r\n"
						   "%s",
						   response->status,
						   response->status == RAMD_HTTP_200_OK ? "OK" :
						   response->status == RAMD_HTTP_400_BAD_REQUEST ? "Bad Request" :
						   response->status == RAMD_HTTP_404_NOT_FOUND ? "Not Found" :
						   response->status == RAMD_HTTP_500_INTERNAL_ERROR ? "Internal Server Error" :
						   "Unknown",
						   strlen(response->content_type) > 0 ? response->content_type : "application/json",
						   response->body_length,
						   response->headers,
						   response->body);

	return send(client_fd, response_buffer, (size_t) response_len, 0) > 0;
}

void
ramd_http_set_json_response(ramd_http_response_t *response, ramd_http_status_code_t status,
						   const char *json)
{
	if (!response)
		return;

	response->status = status;
	strncpy(response->content_type, "application/json", sizeof(response->content_type) - 1);

	if (json)
	{
		strncpy(response->body, json, sizeof(response->body) - 1);
		response->body_length = strlen(response->body);
	}
	else
	{
		response->body[0] = '\0';
		response->body_length = 0;
	}

	response->headers[0] = '\0';
}

void
ramd_http_set_error_response(ramd_http_response_t *response, ramd_http_status_code_t status,
							const char *message)
{
	char json_buffer[RAMD_MAX_COMMAND_LENGTH];

	snprintf(json_buffer, sizeof(json_buffer),
			"{\n"
			"  \"error\": \"%s\",\n"
			"  \"status\": %d\n"
			"}",
			message ? message : "Unknown error",
			status);

	ramd_http_set_json_response(response, status, json_buffer);
}

void
ramd_http_handle_nodes_list(ramd_http_request_t *request, ramd_http_response_t *response)
{
	char           json_buffer[RAMD_MAX_COMMAND_LENGTH];
	ramd_cluster_t *cluster = &g_ramd_daemon->cluster;
	char           nodes_array[RAMD_MAX_COMMAND_LENGTH] = "";
	int            i;

	(void) request;

	if (!cluster)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR, "Cluster not initialized");
		return;
	}

	for (i = 0; i < cluster->node_count; i++)
	{
		const ramd_node_t *node = &cluster->nodes[i];
		char              node_json[RAMD_MAX_COMMAND_LENGTH];

		snprintf(node_json, sizeof(node_json),
				"%s{\n"
				"      \"node_id\": %d,\n"
				"      \"name\": \"%s\",\n"
				"      \"hostname\": \"%s\",\n"
				"      \"postgresql_port\": %d,\n"
				"      \"role\": \"%s\",\n"
				"      \"state\": \"%s\",\n"
				"      \"is_healthy\": %s,\n"
				"      \"is_primary\": %s\n"
				"    }",
				(i > 0) ? "," : "",
				node->node_id,
				node->hostname,
				node->hostname,
				node->postgresql_port,
				(node->role == RAMD_ROLE_PRIMARY) ? "primary" : "standby",
				(node->state == RAMD_NODE_STATE_UNKNOWN) ? "healthy" :
				(node->state == RAMD_NODE_STATE_FAILED) ? "failed" : "unknown",
				node->is_healthy ? "true" : "false",
				(node->node_id == cluster->primary_node_id) ? "true" : "false");

		strcat(nodes_array, node_json);
	}

	snprintf(json_buffer, sizeof(json_buffer),
			"{\n"
			"  \"status\": \"success\",\n"
			"  \"data\": {\n"
			"    \"nodes\": [\n"
			"      %s\n"
			"    ],\n"
			"    \"total_count\": %d\n"
			"  }\n"
			"}",
			nodes_array,
			cluster->node_count);

	ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
}

void
ramd_http_handle_failover(ramd_http_request_t *request, ramd_http_response_t *response)
{
	char                    json_buffer[RAMD_MAX_COMMAND_LENGTH];
	ramd_cluster_t         *cluster = &g_ramd_daemon->cluster;
	ramd_failover_context_t failover_context;

	if (request->method != RAMD_HTTP_POST)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED, "Method not allowed");
		return;
	}

	if (!cluster)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR, "Cluster not initialized");
		return;
	}

	if (!ramd_failover_should_trigger(cluster, &g_ramd_daemon->config))
	{
		snprintf(json_buffer, sizeof(json_buffer),
				"{\n"
				"  \"status\": \"no_action\",\n"
				"  \"message\": \"No failover conditions detected\"\n"
				"}");
		ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
		return;
	}

	ramd_failover_context_init(&failover_context);

	if (ramd_failover_execute(cluster, &g_ramd_daemon->config, &failover_context))
	{
		snprintf(json_buffer, sizeof(json_buffer),
				"{\n"
				"  \"status\": \"success\",\n"
				"  \"message\": \"Failover completed successfully\",\n"
				"  \"new_primary_id\": %d,\n"
				"  \"failover_time\": %ld\n"
				"}",
				failover_context.new_primary_node_id,
				failover_context.completed_at);
		ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
	}
	else
	{
		snprintf(json_buffer, sizeof(json_buffer),
				"{\n"
				"  \"status\": \"failed\",\n"
				"  \"message\": \"Failover execution failed\",\n"
				"  \"error\": \"%s\"\n"
				"}",
				failover_context.reason);
		ramd_http_set_json_response(response, RAMD_HTTP_500_INTERNAL_ERROR, json_buffer);
	}

	ramd_failover_context_cleanup(&failover_context);
}

void
ramd_http_handle_sync_replication(ramd_http_request_t *request, ramd_http_response_t *response)
{
	char               json_buffer[RAMD_MAX_COMMAND_LENGTH];
	ramd_sync_status_t sync_status;
	char              *body;
	char              *mode_pos;
	char              *num_pos;
	char              *colon;
	char              *start;
	char              *end;
	char               mode_str[32] = "";
	int                num_sync = -1;
	size_t             len;

	if (request->method == RAMD_HTTP_GET)
	{
		if (ramd_sync_replication_get_status(&sync_status))
		{
			snprintf(json_buffer, sizeof(json_buffer),
					"{\n"
					"  \"status\": \"success\",\n"
					"  \"data\": {\n"
					"    \"mode\": \"%s\",\n"
					"    \"num_sync_standbys_configured\": %d,\n"
					"    \"num_sync_standbys_connected\": %d,\n"
					"    \"all_sync_standbys_healthy\": %s,\n"
					"    \"last_status_update\": %ld\n"
					"  }\n"
					"}",
					ramd_sync_mode_to_string(sync_status.current_mode),
					sync_status.num_sync_standbys_configured,
					sync_status.num_sync_standbys_connected,
					sync_status.all_sync_standbys_healthy ? "true" : "false",
					sync_status.last_status_update);
			ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
		}
		else
			ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
									   "Failed to get replication status");
	}
	else if (request->method == RAMD_HTTP_POST)
	{
		if (request->body_length > 0)
		{
			body = request->body;
			mode_pos = strstr(body, "\"mode\"");
			if (mode_pos)
			{
				colon = strchr(mode_pos, ':');
				if (colon)
				{
					start = colon + 1;
					while (*start && (*start == ' ' || *start == '\t' || *start == '\n' ||
								   *start == '\r'))
						start++;
					if (*start == '"')
					{
						start++;
						end = strchr(start, '"');
						if (end)
						{
							len = (size_t)(end - start);
							if (len < sizeof(mode_str))
							{
								strncpy(mode_str, start, len);
								mode_str[len] = '\0';
							}
						}
					}
				}
			}

			num_pos = strstr(body, "\"num_sync_standbys\"");
			if (num_pos)
			{
				colon = strchr(num_pos, ':');
				if (colon)
					num_sync = atoi(colon + 1);
			}

			if (strlen(mode_str) > 0)
			{
				ramd_sync_mode_t new_mode = RAMD_SYNC_OFF;
				if (strcmp(mode_str, "off") == 0)
					new_mode = RAMD_SYNC_OFF;
				else if (strcmp(mode_str, "local") == 0)
					new_mode = RAMD_SYNC_LOCAL;
				else if (strcmp(mode_str, "remote_write") == 0)
					new_mode = RAMD_SYNC_REMOTE_WRITE;
				else if (strcmp(mode_str, "remote_apply") == 0)
					new_mode = RAMD_SYNC_REMOTE_APPLY;

				ramd_sync_replication_set_mode(new_mode);
			}

			if (num_sync >= 0)
			{
				/* For num_sync_standbys, we need to create a config and use
				 * configure */
				ramd_sync_config_t sync_config;
				memset(&sync_config, 0, sizeof(sync_config));
				sync_config.num_sync_standbys = num_sync;
				ramd_sync_replication_configure(&sync_config);
			}
		}

		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"success\",\n"
		         "  \"message\": \"Replication configuration updated\"\n"
		         "}");
		ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
	}
	else
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED,
		                             "Method not allowed");
	}
}


void ramd_http_handle_promote_node(ramd_http_request_t* request,
                                   ramd_http_response_t* response)
{
	char json_buffer[RAMD_MAX_COMMAND_LENGTH];

	if (request->method != RAMD_HTTP_POST)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED,
		                             "Method not allowed");
		return;
	}

	
	int32_t node_id = atoi(request->path + RAMD_API_PROMOTE_PATH_LEN); 

	if (node_id <= 0)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_400_BAD_REQUEST,
		                             "Invalid node ID");
		return;
	}

	
	ramd_cluster_t* cluster = &g_ramd_daemon->cluster;
	if (!cluster)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             "Cluster not initialized");
		return;
	}

	if (ramd_failover_promote_node(cluster, &g_ramd_daemon->config, node_id))
	{
		snprintf(
		    json_buffer, sizeof(json_buffer),
		    "{\n"
		    "  \"status\": \"success\",\n"
		    "  \"message\": \"Node %d promoted to primary successfully\",\n"
		    "  \"promoted_node_id\": %d\n"
		    "}",
		    node_id, node_id);
		ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
	}
	else
	{
		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"failed\",\n"
		         "  \"message\": \"Failed to promote node %d to primary\",\n"
		         "  \"node_id\": %d\n"
		         "}",
		         node_id, node_id);
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             json_buffer);
	}
}


void ramd_http_handle_demote_node(ramd_http_request_t* request,
                                  ramd_http_response_t* response)
{
	char json_buffer[RAMD_MAX_COMMAND_LENGTH];

	if (request->method != RAMD_HTTP_POST)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED,
		                             "Method not allowed");
		return;
	}

	
	int32_t node_id = atoi(request->path + RAMD_API_DEMOTE_PATH_LEN); 

	if (node_id <= 0)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_400_BAD_REQUEST,
		                             "Invalid node ID");
		return;
	}

	
	ramd_cluster_t* cluster = &g_ramd_daemon->cluster;
	if (!cluster)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             "Cluster not initialized");
		return;
	}

	if (ramd_failover_demote_failed_primary(cluster, &g_ramd_daemon->config,
	                                        node_id))
	{
		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"success\",\n"
		         "  \"message\": \"Node %d demoted successfully\",\n"
		         "  \"demoted_node_id\": %d\n"
		         "}",
		         node_id, node_id);
		ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
	}
	else
	{
		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"failed\",\n"
		         "  \"message\": \"Failed to demote node %d\",\n"
		         "  \"node_id\": %d\n"
		         "}",
		         node_id, node_id);
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             json_buffer);
	}
}


void ramd_http_handle_node_detail(ramd_http_request_t* request,
                                  ramd_http_response_t* response)
{
	char json_buffer[RAMD_MAX_COMMAND_LENGTH];

	if (request->method != RAMD_HTTP_GET)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED,
		                             "Method not allowed");
		return;
	}

	
	int32_t node_id = atoi(request->path + RAMD_API_NODES_PATH_LEN); 

	if (node_id <= 0)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_400_BAD_REQUEST,
		                             "Invalid node ID");
		return;
	}

	
	ramd_cluster_t* cluster = &g_ramd_daemon->cluster;
	if (!cluster)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             "Cluster not initialized");
		return;
	}

	const ramd_node_t* node = ramd_cluster_find_node(cluster, node_id);
	if (!node)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_404_NOT_FOUND,
		                             "Node not found");
		return;
	}

	snprintf(json_buffer, sizeof(json_buffer),
	         "{\n"
	         "  \"status\": \"success\",\n"
	         "  \"data\": {\n"
	         "    \"node_id\": %d,\n"
	         "    \"name\": \"%s\",\n"
	         "    \"hostname\": \"%s\",\n"
	         "    \"postgresql_port\": %d,\n"
	         "    \"role\": \"%s\",\n"
	         "    \"state\": \"%s\",\n"
	         "    \"is_healthy\": %s,\n"
	         "    \"is_primary\": %s\n"
	         "  }\n"
	         "}",
	         node->node_id, node->hostname, node->hostname,
	         node->postgresql_port,
	         (node->role == RAMD_ROLE_PRIMARY) ? "primary" : "standby",
	         (node->state == RAMD_NODE_STATE_UNKNOWN)  ? "healthy"
	         : (node->state == RAMD_NODE_STATE_FAILED) ? "failed"
	                                                   : "unknown",
	         node->is_healthy ? "true" : "false",
	         (node->node_id == cluster->primary_node_id) ? "true" : "false");

	ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
}


void ramd_http_handle_maintenance_mode(ramd_http_request_t* request,
                                       ramd_http_response_t* response)
{
	char json_buffer[RAMD_MAX_COMMAND_LENGTH];

	if (request->method == RAMD_HTTP_GET)
	{
		
		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"success\",\n"
		         "  \"data\": {\n"
		         "    \"maintenance_mode\": %s,\n"
		         "    \"message\": \"Maintenance mode status retrieved\"\n"
		         "  }\n"
		         "}",
		         g_ramd_daemon->maintenance_mode ? "true" : "false");
		ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
	}
	else if (request->method == RAMD_HTTP_POST)
	{
		
		
		bool new_maintenance_mode = false;
		if (request->body_length > 0)
		{
			char* body = request->body;
			char* enabled_pos = strstr(body, "\"enabled\"");
			if (enabled_pos)
			{
				char* colon = strchr(enabled_pos, ':');
				if (colon)
				{
					char* value = colon + 1;
					while (*value && (*value == ' ' || *value == '\t' ||
					                  *value == '\n' || *value == '\r'))
						value++;
					if (strncmp(value, "true", 4) == 0)
					{
						new_maintenance_mode = true;
					}
				}
			}
		}

		
		g_ramd_daemon->maintenance_mode = new_maintenance_mode;

		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"success\",\n"
		         "  \"message\": \"Maintenance mode %s\"\n"
		         "}",
		         new_maintenance_mode ? "enabled" : "disabled");
		ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
	}
	else
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED,
		                             "Method not allowed");
	}
}

void ramd_http_handle_bootstrap_primary(ramd_http_request_t* request,
                                        ramd_http_response_t* response)
{
	char json_buffer[RAMD_MAX_COMMAND_LENGTH];

	if (request->method != RAMD_HTTP_POST)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED,
		                             "Method not allowed");
		return;
	}

	ramd_cluster_t* cluster = &g_ramd_daemon->cluster;
	if (!cluster)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             "Cluster not initialized");
		return;
	}

	
	if (cluster->node_count > 0)
	{
		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"failed\",\n"
		         "  \"message\": \"Cluster already has %d nodes. Cannot "
		         "bootstrap.\",\n"
		         "  \"node_count\": %d\n"
		         "}",
		         cluster->node_count, cluster->node_count);
		ramd_http_set_error_response(response, RAMD_HTTP_409_CONFLICT,
		                             json_buffer);
		return;
	}

	
	if (ramd_cluster_bootstrap_primary(cluster, &g_ramd_daemon->config))
	{
		
		ramd_log_info("Initializing PostgreSQL database for primary node");

		
		char cluster_data_dir[512];
		snprintf(cluster_data_dir, sizeof(cluster_data_dir), "%s/%s",
		         g_ramd_daemon->config.postgresql_data_dir,
		         cluster->cluster_name);

		if (ramd_maintenance_bootstrap_primary_node(
		        &g_ramd_daemon->config, cluster->cluster_name,
		        g_ramd_daemon->config.hostname,
		        g_ramd_daemon->config.postgresql_port))
		{
			
			char original_data_dir[512];
			strncpy(original_data_dir,
			        g_ramd_daemon->config.postgresql_data_dir,
			        sizeof(original_data_dir) - 1);
			strncpy(g_ramd_daemon->config.postgresql_data_dir, cluster_data_dir,
			        sizeof(g_ramd_daemon->config.postgresql_data_dir) - 1);

			
			if (ramd_postgresql_start(&g_ramd_daemon->config))
			{
				
				ramd_log_info("PostgreSQL started successfully, creating "
				              "pgraft extension");

				if (!ramd_postgresql_create_pgraft_extension(
				        &g_ramd_daemon->config))
				{
					ramd_log_error("Failed to create pgraft extension");
					strncpy(g_ramd_daemon->config.postgresql_data_dir,
					        original_data_dir,
					        sizeof(g_ramd_daemon->config.postgresql_data_dir) -
					            1);
					snprintf(
					    json_buffer, sizeof(json_buffer),
					    "{\n"
					    "  \"status\": \"error\",\n"
					    "  \"message\": \"Failed to create pgraft extension\"\n"
					    "}");
					ramd_http_set_json_response(
					    response, RAMD_HTTP_500_INTERNAL_ERROR, json_buffer);
					return;
				}

				snprintf(json_buffer, sizeof(json_buffer),
				         "{\n"
				         "  \"status\": \"success\",\n"
				         "  \"message\": \"Primary node bootstrapped and "
				         "PostgreSQL initialized successfully\",\n"
				         "  \"node_id\": %d,\n"
				         "  \"hostname\": \"%s\",\n"
				         "  \"cluster_name\": \"%s\",\n"
				         "  \"postgresql_status\": \"running\"\n"
				         "}",
				         g_ramd_daemon->config.node_id,
				         g_ramd_daemon->config.hostname, cluster->cluster_name);
				ramd_http_set_json_response(response, RAMD_HTTP_200_OK,
				                            json_buffer);
				ramd_log_info("Primary node bootstrap completed successfully "
				              "via HTTP API");
			}
			else
			{
				snprintf(json_buffer, sizeof(json_buffer),
				         "{\n"
				         "  \"status\": \"failed\",\n"
				         "  \"message\": \"PostgreSQL initialization succeeded "
				         "but failed to start PostgreSQL\"\n"
				         "}");
				ramd_http_set_error_response(
				    response, RAMD_HTTP_500_INTERNAL_ERROR, json_buffer);
			}

			
			strncpy(g_ramd_daemon->config.postgresql_data_dir,
			        original_data_dir,
			        sizeof(g_ramd_daemon->config.postgresql_data_dir) - 1);
		}
		else
		{
			snprintf(
			    json_buffer, sizeof(json_buffer),
			    "{\n"
			    "  \"status\": \"failed\",\n"
			    "  \"message\": \"Failed to initialize PostgreSQL database\"\n"
			    "}");
			ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
			                             json_buffer);
		}
	}
	else
	{
		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"failed\",\n"
		         "  \"message\": \"Failed to bootstrap primary node\"\n"
		         "}");
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             json_buffer);
	}
}


char* ramd_http_get_query_param(const char* query_string,
                                const char* param_name)
{
	if (!query_string || !param_name)
	{
		return NULL;
	}
	
	char* query_copy = strdup(query_string);
	if (!query_copy)
	{
		return NULL;
	}
	
	char* token = strtok(query_copy, "&");
	while (token != NULL)
	{
		char* equals = strchr(token, '=');
		if (equals)
		{
			*equals = '\0';
			if (strcmp(token, param_name) == 0)
			{
				char* value = strdup(equals + 1);
				free(query_copy);
				return value;
			}
		}
		token = strtok(NULL, "&");
	}
	
	free(query_copy);
	return NULL;
}

bool ramd_http_authenticate(const char* authorization,
                            const char* required_token)
{
	if (!authorization || !required_token)
		return false;

	return strcmp(authorization, required_token) == 0;
}


void ramd_http_handle_add_replica(ramd_http_request_t* request,
                                  ramd_http_response_t* response)
{
	char json_buffer[RAMD_MAX_COMMAND_LENGTH];
	char hostname[RAMD_MAX_USERNAME_LENGTH] = {0};
	int32_t port = g_ramd_daemon->config.postgresql_port;
	int32_t new_node_id;

	if (request->method != RAMD_HTTP_POST)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED,
		                             "Method not allowed - use POST");
		return;
	}

	
	if (request->body[0] != '\0')
	{
		
		char* hostname_start = strstr(request->body, "\"hostname\":");
		char* port_start = strstr(request->body, "\"port\":");

		if (hostname_start)
		{
			hostname_start = strchr(hostname_start + 11, '"');
			if (hostname_start)
			{
				hostname_start++; 
				char* hostname_end = strchr(hostname_start, '"');
				if (hostname_end)
				{
					size_t len = (size_t)(hostname_end - hostname_start);
					if (len < sizeof(hostname) - 1)
					{
						strncpy(hostname, hostname_start, len);
						hostname[len] = '\0';
					}
				}
			}
		}

		if (port_start)
		{
			port_start += 7; 
			while (*port_start == ' ' || *port_start == ':')
				port_start++;
			port = atoi(port_start);
		}
	}

	if (strlen(hostname) == 0)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_400_BAD_REQUEST,
		                             "Missing required parameter: hostname");
		return;
	}

	if (port <= 0 || port > 65535)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_400_BAD_REQUEST,
		                             "Invalid port number");
		return;
	}

	ramd_log_info("Adding replica: %s:%d", hostname, port);

	
	ramd_cluster_t* cluster = &g_ramd_daemon->cluster;
	new_node_id = cluster->node_count + 1;

	
	if (!ramd_cluster_add_node(cluster, new_node_id, hostname, port,
	                           RAMD_DEFAULT_RALE_PORT + new_node_id, 
	                           RAMD_DEFAULT_DSTORE_PORT + new_node_id))
	{
		ramd_log_error("Failed to add node %d (%s:%d) to cluster", new_node_id,
		               hostname, port);
		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"error\",\n"
		         "  \"message\": \"Failed to add node to cluster\"\n"
		         "}");
		ramd_http_set_json_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                            json_buffer);
		return;
	}

	
	ramd_log_info("Setting up PostgreSQL replica for node %d", new_node_id);

	if (!ramd_maintenance_setup_replica(&g_ramd_daemon->config,
	                                    cluster->cluster_name, hostname, port,
	                                    new_node_id))
	{
		ramd_log_error("Failed to setup PostgreSQL replica for node %d",
		               new_node_id);
		
		ramd_cluster_remove_node(cluster, new_node_id);
		snprintf(json_buffer, sizeof(json_buffer),
		         "{\n"
		         "  \"status\": \"error\",\n"
		         "  \"message\": \"Failed to setup PostgreSQL replica\"\n"
		         "}");
		ramd_http_set_json_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                            json_buffer);
		return;
	}

	
	ramd_log_info("Installing pgraft extension on replica node %d",
	              new_node_id);
	
	
	
	
	
	char conn_string[RAMD_MAX_COMMAND_LENGTH];
	snprintf(conn_string, sizeof(conn_string), 
	         "host=%s port=%d dbname=%s user=%s", 
	         hostname, new_node_id + g_ramd_daemon->config.postgresql_port,
	         g_ramd_daemon->config.database_name, g_ramd_daemon->config.database_user);
	
	PGconn *conn = ramd_conn_get(hostname, new_node_id + g_ramd_daemon->config.postgresql_port, 
	                             g_ramd_daemon->config.database_name, g_ramd_daemon->config.database_user, "");
	if (!conn)
	{
		ramd_log_error("Failed to connect to replica PostgreSQL");
		return;
	}
	
	
	
	PGresult *res = ramd_query_exec_with_result(conn, "SELECT 1 FROM pg_extension WHERE extname = 'pgraft'");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		ramd_log_error("Failed to check pgraft extension: %s", PQerrorMessage(conn));
		PQclear(res);
		ramd_conn_close(conn);
		return;
	}
	
	bool extension_exists = (PQntuples(res) > 0);
	PQclear(res);
	
	
	
	if (!extension_exists)
	{
		res = ramd_query_exec_with_result(conn, "CREATE EXTENSION IF NOT EXISTS pgraft");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
			ramd_log_error("Failed to create pgraft extension: %s", PQerrorMessage(conn));
			PQclear(res);
			ramd_conn_close(conn);
			return;
		}
		PQclear(res);
		ramd_log_info("pgraft extension installed successfully on replica node %d", new_node_id);
	}
	else
	{
		ramd_log_info("pgraft extension already exists on replica node %d", new_node_id);
	}
	
	
	
		res = ramd_query_exec_with_result(conn, "ALTER SYSTEM SET pgraft.node_id = '1'");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		ramd_log_warning("Failed to set pgraft.node_id: %s", PQerrorMessage(conn));
	}
	PQclear(res);
	
		res = ramd_query_exec_with_result(conn, "ALTER SYSTEM SET pgraft.cluster_name = 'test_cluster'");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		ramd_log_warning("Failed to set pgraft.cluster_name: %s", PQerrorMessage(conn));
	}
	PQclear(res);
	
	
	
		res = ramd_query_exec_with_result(conn, "SELECT pgraft_get_version()");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		ramd_log_error("Failed to verify pgraft extension: %s", PQerrorMessage(conn));
		PQclear(res);
		ramd_conn_close(conn);
		return;
	}
	
	if (PQntuples(res) > 0)
	{
		ramd_log_info("pgraft extension verified on replica node %d, version: %s", 
		              new_node_id, PQgetvalue(res, 0, 0));
	}
	PQclear(res);
	PQfinish(conn);

	
	ramd_log_info("Adding node %d to pgraft consensus", new_node_id);
	
	
	
	
	
	PGconn *consensus_conn = ramd_conn_get(hostname, new_node_id + g_ramd_daemon->config.postgresql_port, 
	                                       g_ramd_daemon->config.database_name, g_ramd_daemon->config.database_user, "");
	if (!consensus_conn)
	{
		ramd_log_error("Failed to connect for consensus");
		return;
	}
	
	
	
	char pgraft_query[RAMD_MAX_COMMAND_LENGTH];
	snprintf(pgraft_query, sizeof(pgraft_query), "SELECT pgraft_add_node(1, '%s', %d)", 
	         g_ramd_daemon->config.hostname, g_ramd_daemon->config.postgresql_port);
	PGresult *consensus_res = ramd_query_exec_with_result(consensus_conn, pgraft_query);
	if (PQresultStatus(consensus_res) != PGRES_TUPLES_OK)
	{
		ramd_log_error("Failed to add node to consensus: %s", PQerrorMessage(consensus_conn));
		PQclear(consensus_res);
		ramd_conn_close(consensus_conn);
		return;
	}
	PQclear(consensus_res);
	
	
	
	consensus_res = ramd_query_exec_with_result(consensus_conn, "SELECT pgraft_update_cluster_membership()");
	if (PQresultStatus(consensus_res) != PGRES_TUPLES_OK)
	{
		ramd_log_warning("Failed to update cluster membership: %s", PQerrorMessage(consensus_conn));
	}
	PQclear(consensus_res);
	
	
	
	consensus_res = ramd_query_exec_with_result(consensus_conn, "SELECT pgraft_start_consensus()");
	if (PQresultStatus(consensus_res) != PGRES_TUPLES_OK)
	{
		ramd_log_warning("Failed to start consensus: %s", PQerrorMessage(consensus_conn));
	}
	PQclear(consensus_res);
	
	
	
	consensus_res = ramd_query_exec_with_result(consensus_conn, "SELECT pgraft_get_cluster_status()");
	if (PQresultStatus(consensus_res) == PGRES_TUPLES_OK && PQntuples(consensus_res) > 0)
	{
		ramd_log_info("Node %d successfully added to consensus, status: %s", 
		              new_node_id, PQgetvalue(consensus_res, 0, 0));
	}
	else
	{
		ramd_log_warning("Failed to verify consensus integration: %s", PQerrorMessage(consensus_conn));
	}
	PQclear(consensus_res);
	PQfinish(consensus_conn);

	
	snprintf(json_buffer, sizeof(json_buffer),
	         "{\n"
	         "  \"status\": \"success\",\n"
	         "  \"message\": \"Replica added successfully\",\n"
	         "  \"node_id\": %d,\n"
	         "  \"hostname\": \"%s\",\n"
	         "  \"port\": %d,\n"
	         "  \"role\": \"replica\"\n"
	         "}",
	         new_node_id, hostname, port);

	ramd_http_set_json_response(response, RAMD_HTTP_200_OK, json_buffer);
	ramd_log_info("Replica node %d (%s:%d) added successfully", new_node_id,
	              hostname, port);
}


void ramd_http_handle_metrics(ramd_http_request_t* request,
                              ramd_http_response_t* response)
{
	if (request->method != RAMD_HTTP_GET)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_405_METHOD_NOT_ALLOWED,
		                             "Method not allowed");
		return;
	}

	if (!g_ramd_daemon || !g_ramd_metrics)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_503_SERVICE_UNAVAILABLE,
		                             "Metrics not available");
		return;
	}

	
	ramd_metrics_collect(g_ramd_metrics, &g_ramd_daemon->cluster);

	
	char* prometheus_output = ramd_metrics_to_prometheus(g_ramd_metrics);
	if (!prometheus_output)
	{
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             "Failed to generate metrics");
		return;
	}

	
	strncpy(response->content_type, "text/plain; version=0.0.4; charset=utf-8",
	        sizeof(response->content_type) - 1);
	
	
	size_t output_len = strlen(prometheus_output);
	if (output_len < sizeof(response->body))
	{
		strncpy(response->body, prometheus_output, output_len);
		response->body_length = output_len;
		response->status = RAMD_HTTP_200_OK;
	}
	else
	{
		ramd_http_set_error_response(response, RAMD_HTTP_500_INTERNAL_ERROR,
		                             "Metrics output too large");
	}

	
	ramd_metrics_free_prometheus_output(prometheus_output);
}

/*
 * Get number of healthy nodes
 */
static int
get_healthy_nodes_count(void)
{
	
	if (g_ramd_daemon && g_ramd_daemon->cluster.node_count > 0)
	{
		return g_ramd_daemon->cluster.node_count;
	}
	return 1; 
}
