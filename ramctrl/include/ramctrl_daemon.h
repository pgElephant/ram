/*-------------------------------------------------------------------------
 *
 * ramctrl_daemon.h
 *		PostgreSQL RAM Control Utility - Daemon Control
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_DAEMON_H
#define RAMCTRL_DAEMON_H

#include "ramctrl.h"

/* Daemon status */
typedef struct ramctrl_daemon_status_t {
    bool			is_running;
    pid_t			pid;
    time_t			start_time;
    char			config_file[RAMCTRL_MAX_PATH_LENGTH];
    char			log_file[RAMCTRL_MAX_PATH_LENGTH];
    char			pid_file[RAMCTRL_MAX_PATH_LENGTH];
    int32_t			node_id;
    char			cluster_name[64];
} ramctrl_daemon_status_t;

/* Daemon control functions */
bool ramctrl_daemon_start(ramctrl_context_t *ctx, const char *config_file);
bool ramctrl_daemon_stop(ramctrl_context_t *ctx);
bool ramctrl_daemon_restart(ramctrl_context_t *ctx, const char *config_file);
bool ramctrl_daemon_reload(ramctrl_context_t *ctx);

/* Daemon status checking */
bool ramctrl_daemon_is_running(ramctrl_context_t *ctx);
bool ramctrl_daemon_get_status(ramctrl_context_t *ctx, 
                              ramctrl_daemon_status_t *status);
pid_t ramctrl_daemon_get_pid(ramctrl_context_t *ctx);

/* Process management */
bool ramctrl_daemon_send_signal(ramctrl_context_t *ctx, int signal);
bool ramctrl_daemon_wait_for_startup(ramctrl_context_t *ctx, int timeout_seconds);
bool ramctrl_daemon_wait_for_shutdown(ramctrl_context_t *ctx, int timeout_seconds);

/* Log management */
bool ramctrl_daemon_get_logs(ramctrl_context_t *ctx, 
                            char *output, size_t output_size,
                            int num_lines);
bool ramctrl_daemon_follow_logs(ramctrl_context_t *ctx);

/* Configuration management */
bool ramctrl_daemon_validate_config(ramctrl_context_t *ctx, 
                                   const char *config_file);
bool ramctrl_daemon_generate_config(ramctrl_context_t *ctx,
                                   const char *config_file,
                                   int32_t node_id,
                                   const char *hostname,
                                   int32_t port);

#endif /* RAMCTRL_DAEMON_H */
