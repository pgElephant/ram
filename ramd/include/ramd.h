/*-------------------------------------------------------------------------
 *
 * ramd.h
 *		PostgreSQL Auto-Failover Daemon - Main header
 *
 * ramd is a daemon that monitors PostgreSQL instances in a cluster
 * and performs automatic failover when the primary fails.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_H
#define RAMD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <libpq-fe.h>

/* Version information */
#define RAMD_VERSION_MAJOR 1
#define RAMD_VERSION_MINOR 0
#define RAMD_VERSION_PATCH 0
#define RAMD_VERSION_STRING "1.0.0"

/* Use centralized defaults from ramd_defaults.h */
#include "ramd_defaults.h"

/* Node states */
typedef enum ramd_node_state
{
	RAMD_NODE_STATE_UNKNOWN = 0,
	RAMD_NODE_STATE_PRIMARY,
	RAMD_NODE_STATE_STANDBY,
	RAMD_NODE_STATE_FAILED,
	RAMD_NODE_STATE_RECOVERING,
	RAMD_NODE_STATE_LEADER,
	RAMD_NODE_STATE_FOLLOWER
} ramd_node_state_t;

/* Role types */
typedef enum ramd_role
{
	RAMD_ROLE_UNKNOWN = 0,
	RAMD_ROLE_PRIMARY,
	RAMD_ROLE_STANDBY
} ramd_role_t;

/* Forward declarations */
typedef struct ramd_config ramd_config_t;
typedef struct ramd_node_t ramd_node_t;
typedef struct ramd_cluster_t ramd_cluster_t;
typedef struct ramd_daemon_t ramd_daemon_t;

/* Global daemon instance */
extern ramd_daemon_t* g_ramd_daemon;

/* Function prototypes */

/* Core daemon functions */
extern bool ramd_init(const char* config_file);
extern void ramd_cleanup(void);
extern void ramd_run(void);
extern void ramd_stop(void);
extern bool ramd_is_running(void);
extern PGconn* ramd_get_postgres_connection(void);

/* Signal handling */
extern void ramd_setup_signals(void);
extern void ramd_handle_signal(int sig);

/* Process management */
extern bool ramd_daemonize(void);
extern bool ramd_write_pidfile(const char* pidfile_path);
extern void ramd_remove_pidfile(const char* pidfile_path);

#endif /* RAMD_H */
