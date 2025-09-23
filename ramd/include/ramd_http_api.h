/*-------------------------------------------------------------------------
 *
 * ramd_http_api.h
 *		PostgreSQL Auto-Failover Daemon - HTTP REST API Interface
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_HTTP_API_H
#define RAMD_HTTP_API_H

#include "ramd.h"
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* HTTP API Configuration */
#include "ramd_defaults.h"
#define RAMD_HTTP_DEFAULT_PORT RAMD_DEFAULT_HTTP_PORT
#define RAMD_HTTP_MAX_REQUEST_SIZE RAMD_MAX_COMMAND_LENGTH
#define RAMD_HTTP_MAX_RESPONSE_SIZE (RAMD_MAX_COMMAND_LENGTH * 2)

/* HTTP Methods */
typedef enum
{
	RAMD_HTTP_GET,
	RAMD_HTTP_POST,
	RAMD_HTTP_PUT,
	RAMD_HTTP_DELETE,
	RAMD_HTTP_PATCH
} ramd_http_method_t;

/* HTTP status codes */
typedef enum
{
	RAMD_HTTP_200_OK = 200,
	RAMD_HTTP_400_BAD_REQUEST = 400,
	RAMD_HTTP_401_UNAUTHORIZED = 401,
	RAMD_HTTP_404_NOT_FOUND = 404,
	RAMD_HTTP_405_METHOD_NOT_ALLOWED = 405,
	RAMD_HTTP_409_CONFLICT = 409,
	RAMD_HTTP_500_INTERNAL_ERROR = 500,
	RAMD_HTTP_501_NOT_IMPLEMENTED = 501,
	RAMD_HTTP_503_SERVICE_UNAVAILABLE = 503
} ramd_http_status_code_t;

/* HTTP Request Structure */
typedef struct ramd_http_request_t
{
	ramd_http_method_t method;
	char path[RAMD_MAX_PATH_LENGTH];
	char query_string[RAMD_MAX_COMMAND_LENGTH];
	char body[RAMD_HTTP_MAX_REQUEST_SIZE];
	size_t body_length;
	char headers[RAMD_MAX_COMMAND_LENGTH];
	char authorization[RAMD_MAX_HOSTNAME_LENGTH];
} ramd_http_request_t;

/* HTTP Response Structure */
typedef struct ramd_http_response_t
{
	ramd_http_status_code_t status;
	char content_type[RAMD_MAX_HOSTNAME_LENGTH];
	char body[RAMD_HTTP_MAX_RESPONSE_SIZE];
	size_t body_length;
	char headers[RAMD_MAX_COMMAND_LENGTH];
} ramd_http_response_t;

/* HTTP Server Context */
typedef struct ramd_http_server_t
{
	int listen_fd;
	int port;
	bool running;
	pthread_t server_thread;
	pthread_mutex_t mutex;
	char bind_address[RAMD_MAX_HOSTNAME_LENGTH];
	bool auth_enabled;
	char auth_token[RAMD_MAX_COMMAND_LENGTH];
} ramd_http_server_t;

/* HTTP Client Connection */
typedef struct ramd_http_connection_t
{
	int client_fd;
	struct sockaddr_in client_addr;
	ramd_http_server_t* server;
} ramd_http_connection_t;

/* Function prototypes */
bool ramd_http_server_init(ramd_http_server_t* server, const char* bind_address,
                           int port);
bool ramd_http_server_start(ramd_http_server_t* server);
void ramd_http_server_stop(ramd_http_server_t* server);
void ramd_http_server_cleanup(ramd_http_server_t* server);

bool ramd_http_parse_request(const char* raw_request,
                             ramd_http_request_t* request);
void ramd_http_handle_request(ramd_http_request_t* request,
                              ramd_http_response_t* response);
bool ramd_http_send_response(int client_fd, ramd_http_response_t* response);

/* API Endpoint Handlers */
void ramd_http_handle_cluster_status(ramd_http_request_t* request,
                                     ramd_http_response_t* response);
void ramd_http_handle_nodes_list(ramd_http_request_t* request,
                                 ramd_http_response_t* response);
void ramd_http_handle_node_detail(ramd_http_request_t* request,
                                  ramd_http_response_t* response);
void ramd_http_handle_promote_node(ramd_http_request_t* request,
                                   ramd_http_response_t* response);
void ramd_http_handle_demote_node(ramd_http_request_t* request,
                                  ramd_http_response_t* response);
void ramd_http_handle_failover(ramd_http_request_t* request,
                               ramd_http_response_t* response);
void ramd_http_handle_maintenance_mode(ramd_http_request_t* request,
                                       ramd_http_response_t* response);
void ramd_http_handle_config_reload(ramd_http_request_t* request,
                                    ramd_http_response_t* response);
void ramd_http_handle_sync_replication(ramd_http_request_t* request,
                                       ramd_http_response_t* response);
void ramd_http_handle_bootstrap_primary(ramd_http_request_t* request,
                                        ramd_http_response_t* response);
void ramd_http_handle_add_replica(ramd_http_request_t* request,
                                  ramd_http_response_t* response);
void ramd_http_handle_metrics(ramd_http_request_t* request,
                              ramd_http_response_t* response);
void ramd_http_handle_prometheus_metrics(ramd_http_request_t* request,
                                         ramd_http_response_t* response);

/* Security handler functions */
void ramd_http_handle_security_status(ramd_http_request_t* request,
                                      ramd_http_response_t* response);
void ramd_http_handle_security_audit(ramd_http_request_t* request,
                                     ramd_http_response_t* response);
void ramd_http_handle_security_users(ramd_http_request_t* request,
                                     ramd_http_response_t* response);

/* Enhanced integration: New HTTP API handlers for ramctrl communication */
void ramd_http_handle_add_node(ramd_http_request_t* request, ramd_http_response_t* response);
void ramd_http_handle_remove_node(ramd_http_request_t* request, ramd_http_response_t* response);
void ramd_http_handle_cluster_health(ramd_http_request_t* request, ramd_http_response_t* response);
void ramd_http_handle_cluster_notify(ramd_http_request_t* request, ramd_http_response_t* response);

/* Utility functions */
char* ramd_http_get_query_param(const char* query_string,
                                const char* param_name);
bool ramd_http_authenticate(const char* authorization,
                            const char* required_token);
void ramd_http_set_json_response(ramd_http_response_t* response,
                                 ramd_http_status_code_t status,
                                 const char* json);
void ramd_http_set_error_response(ramd_http_response_t* response,
                                  ramd_http_status_code_t status,
                                  const char* message);

#endif /* RAMD_HTTP_API_H */
