/*
 * ramctrl_http.h
 *
 * HTTP client utilities for ramctrl
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#ifndef RAMCTRL_HTTP_H
#define RAMCTRL_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include "ramctrl.h"

/* HTTP response structure */
typedef struct ramctrl_http_response
{
	int status_code;
	char* body;
	size_t body_size;
	char error_message[256];
} ramctrl_http_response_t;

/* HTTP configuration */
typedef struct ramctrl_http_config
{
	char base_url[256];
	int timeout_seconds;
	int max_retries;
	bool ssl_verify;
	char auth_token[512];
} ramctrl_http_config_t;

/* Function declarations */
extern int ramctrl_http_init(void);
extern void ramctrl_http_cleanup(void);
extern int ramctrl_http_get(const char* url, char* response,
                            size_t response_size);
extern int ramctrl_http_post(const char* url, const char* data, char* response,
                             size_t response_size);
extern int ramctrl_parse_cluster_status(const char* json,
                                        ramctrl_cluster_info_t* cluster_info);
extern int ramctrl_parse_nodes_info(const char* json,
                                    ramctrl_node_info_t* nodes,
                                    int* node_count);
extern void
ramctrl_set_fallback_cluster_info(ramctrl_cluster_info_t* cluster_info);
extern void ramctrl_set_fallback_nodes_data(ramctrl_node_info_t* nodes,
                                            int* node_count);

#endif /* RAMCTRL_HTTP_H */
