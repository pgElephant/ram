/*-------------------------------------------------------------------------
 *
 * ramd_daemon.h
 *		PostgreSQL Auto-Failover Daemon - Daemon Structure Definition
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_DAEMON_H
#define RAMD_DAEMON_H

#include <pthread.h>
#include <stdbool.h>

#include "ramd_config.h"
#include "ramd_cluster.h"
#include "ramd_monitor.h"
#include "ramd_failover.h"
#include "ramd_http_api.h"

/* Main daemon structure */
struct ramd_daemon_t
{
	/* Runtime state */
	bool running;
	bool shutdown_requested;
	bool maintenance_mode;
	pthread_mutex_t mutex;

	/* Configuration */
	ramd_config_t config;
	char* config_file;

	/* Core components */
	ramd_cluster_t cluster;
	ramd_monitor_t monitor;
	ramd_failover_context_t failover_context;

	/* HTTP API server */
	ramd_http_server_t http_server;
};

/* Global daemon instance */
extern struct ramd_daemon_t* g_ramd_daemon;

#endif /* RAMD_DAEMON_H */
