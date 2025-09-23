/*-------------------------------------------------------------------------
 *
 * ramd_config_reload.c
 *		PostgreSQL Auto-Failover Daemon - Dynamic Configuration Reload
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "ramd_config_reload.h"
#include "ramd_daemon.h"
#include "ramd_logging.h"
#include "ramd_sync_replication.h"
#include "ramd_http_api.h"
#include "ramd_maintenance.h"
#include "ramd_postgresql.h"

static bool g_config_reload_initialized = false;
static ramd_config_t g_current_config;
static pthread_mutex_t g_config_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_config_file_path[RAMD_MAX_PATH_LENGTH];

extern ramd_daemon_t* g_ramd_daemon;
extern PGconn* g_conn;

bool ramd_config_reload_init(void)
{
	pthread_mutex_lock(&g_config_mutex);

	if (g_ramd_daemon && g_ramd_daemon->config_file)
	{
		strncpy(g_config_file_path, g_ramd_daemon->config_file,
		        sizeof(g_config_file_path) - 1);
		memcpy(&g_current_config, &g_ramd_daemon->config,
		       sizeof(ramd_config_t));
	}

	g_config_reload_initialized = true;

	pthread_mutex_unlock(&g_config_mutex);

	ramd_config_reload_signal_setup();

	ramd_log_info("Configuration reload system initialized");
	return true;
}

void ramd_config_reload_cleanup(void)
{
	pthread_mutex_lock(&g_config_mutex);
	g_config_reload_initialized = false;
	pthread_mutex_unlock(&g_config_mutex);

	ramd_log_info("Configuration reload system cleaned up");
}

bool ramd_config_reload_from_file(const char* config_file,
                                  ramd_config_reload_result_t* result)
{
	ramd_config_t               new_config;
	ramd_config_t               old_config;
	const char*                 file_to_load;
	bool                        all_applied = true;
	ramd_config_change_flags_t  applied_changes = RAMD_CONFIG_CHANGE_NONE;

	if (!result)
		return false;

	memset(result, 0, sizeof(ramd_config_reload_result_t));
	result->reload_time = time(NULL);

	if (!g_config_reload_initialized)
	{
		strncpy(result->error_message, "Configuration reload not initialized",
		        sizeof(result->error_message) - 1);
		result->status = RAMD_CONFIG_RELOAD_FAILED;
		return false;
	}

	pthread_mutex_lock(&g_config_mutex);

	file_to_load = config_file ? config_file : g_config_file_path;

	memcpy(&old_config, &g_current_config, sizeof(ramd_config_t));

	if (!ramd_config_init(&new_config))
	{
		strncpy(result->error_message, "Failed to initialize new configuration",
		        sizeof(result->error_message) - 1);
		result->status = RAMD_CONFIG_RELOAD_FAILED;
		pthread_mutex_unlock(&g_config_mutex);
		return false;
	}

	if (!ramd_config_load_file(&new_config, file_to_load))
	{
		snprintf(result->error_message, sizeof(result->error_message),
		         "Failed to load configuration from %s", file_to_load);
		result->status = RAMD_CONFIG_RELOAD_FAILED;
		pthread_mutex_unlock(&g_config_mutex);
		return false;
	}

	if (!ramd_config_validate_reload(&new_config, result->error_message,
	                                 sizeof(result->error_message)))
	{
		result->status = RAMD_CONFIG_RELOAD_FAILED;
		pthread_mutex_unlock(&g_config_mutex);
		return false;
	}

	result->changes_detected = ramd_config_compare(&old_config, &new_config);

	if (result->changes_detected == RAMD_CONFIG_CHANGE_NONE)
	{
		result->status = RAMD_CONFIG_RELOAD_NO_CHANGES;
		pthread_mutex_unlock(&g_config_mutex);
		ramd_log_info("Configuration reload: no changes detected");
		return true;
	}

	if (result->changes_detected & RAMD_CONFIG_CHANGE_LOGGING)
	{
		if (ramd_config_reload_logging(&old_config, &new_config))
			applied_changes |= RAMD_CONFIG_CHANGE_LOGGING;
		else
			all_applied = false;
	}

	if (result->changes_detected & RAMD_CONFIG_CHANGE_MONITORING)
	{
		if (ramd_config_reload_monitoring(&old_config, &new_config))
			applied_changes |= RAMD_CONFIG_CHANGE_MONITORING;
		else
			all_applied = false;
	}

	if (result->changes_detected & RAMD_CONFIG_CHANGE_FAILOVER)
	{
		if (ramd_config_reload_failover(&old_config, &new_config))
			applied_changes |= RAMD_CONFIG_CHANGE_FAILOVER;
		else
			all_applied = false;
	}

	if (result->changes_detected & RAMD_CONFIG_CHANGE_SYNCHRONOUS_REP)
	{
		if (ramd_config_reload_sync_replication(&old_config, &new_config))
			applied_changes |= RAMD_CONFIG_CHANGE_SYNCHRONOUS_REP;
		else
			all_applied = false;
	}

	if (result->changes_detected & RAMD_CONFIG_CHANGE_HTTP_API)
	{
		if (ramd_config_reload_http_api(&old_config, &new_config))
			applied_changes |= RAMD_CONFIG_CHANGE_HTTP_API;
		else
			all_applied = false;
	}

	memcpy(&g_current_config, &new_config, sizeof(ramd_config_t));

	if (g_ramd_daemon)
		memcpy(&g_ramd_daemon->config, &new_config, sizeof(ramd_config_t));

	result->changes_applied = applied_changes;
	result->status = all_applied ? RAMD_CONFIG_RELOAD_SUCCESS : RAMD_CONFIG_RELOAD_PARTIAL;

	pthread_mutex_unlock(&g_config_mutex);

	ramd_log_info("Configuration reloaded successfully (changes: 0x%x, applied: 0x%x)",
	    result->changes_detected, result->changes_applied);

	return true;
}

ramd_config_change_flags_t ramd_config_compare(const ramd_config_t* old_config,
                                               const ramd_config_t* new_config)
{
	ramd_config_change_flags_t changes = RAMD_CONFIG_CHANGE_NONE;

	if (!old_config || !new_config)
		return changes;

	if (old_config->log_level != new_config->log_level ||
	    strcmp(old_config->log_file, new_config->log_file) != 0 ||
	    old_config->log_to_syslog != new_config->log_to_syslog ||
	    old_config->log_to_console != new_config->log_to_console)
	{
		changes |= RAMD_CONFIG_CHANGE_LOGGING;
	}

	if (old_config->monitor_interval_ms != new_config->monitor_interval_ms ||
	    old_config->health_check_timeout_ms != new_config->health_check_timeout_ms)
	{
		changes |= RAMD_CONFIG_CHANGE_MONITORING;
	}

	if (old_config->auto_failover_enabled != new_config->auto_failover_enabled ||
	    old_config->failover_timeout_ms != new_config->failover_timeout_ms)
	{
		changes |= RAMD_CONFIG_CHANGE_FAILOVER;
	}

	if (old_config->postgresql_port != new_config->postgresql_port ||
	    strcmp(old_config->postgresql_data_dir, new_config->postgresql_data_dir) != 0 ||
	    strcmp(old_config->database_user, new_config->database_user) != 0)
	{
		changes |= RAMD_CONFIG_CHANGE_POSTGRESQL;
	}

	if (strcmp(old_config->cluster_name, new_config->cluster_name) != 0 ||
	    old_config->cluster_size != new_config->cluster_size)
	{
		changes |= RAMD_CONFIG_CHANGE_CLUSTER;
	}

	if (old_config->synchronous_replication != new_config->synchronous_replication)
	{
		changes |= RAMD_CONFIG_CHANGE_SYNCHRONOUS_REP;
	}

	return changes;
}

bool ramd_config_validate_reload(const ramd_config_t* new_config,
                                 char* error_msg, size_t error_size)
{
	if (!new_config)
	{
		if (error_msg)
			strncpy(error_msg, "New configuration is NULL", error_size - 1);
		return false;
	}

	if (!ramd_config_validate(new_config))
	{
		if (error_msg)
			strncpy(error_msg, "Configuration validation failed", error_size - 1);
		return false;
	}

	pthread_mutex_lock(&g_config_mutex);

	if (g_current_config.node_id != new_config->node_id)
	{
		if (error_msg)
			strncpy(error_msg, "Node ID cannot be changed during reload", error_size - 1);
		pthread_mutex_unlock(&g_config_mutex);
		return false;
	}

	if (strcmp(g_current_config.hostname, new_config->hostname) != 0)
	{
		if (error_msg)
			strncpy(error_msg, "Hostname cannot be changed during reload", error_size - 1);
		pthread_mutex_unlock(&g_config_mutex);
		return false;
	}

	pthread_mutex_unlock(&g_config_mutex);

	return true;
}

bool ramd_config_reload_logging(const ramd_config_t* old_config,
                                const ramd_config_t* new_config)
{
	(void) old_config;

	if (!ramd_logging_init(new_config->log_file, new_config->log_level,
	                       strlen(new_config->log_file) > 0,
	                       new_config->log_to_syslog,
	                       new_config->log_to_console))
	{
		ramd_log_error("Failed to reload logging configuration");
		return false;
	}

	ramd_log_info("Logging configuration reloaded successfully");
	return true;
}

bool ramd_config_reload_monitoring(const ramd_config_t* old_config,
                                   const ramd_config_t* new_config)
{
	if (!old_config || !new_config)
		return false;

	if (old_config->monitor_interval_ms != new_config->monitor_interval_ms)
	{
		ramd_log_info("Monitor interval changed from %d ms to %d ms",
		              old_config->monitor_interval_ms,
		              new_config->monitor_interval_ms);
	}

	if (old_config->health_check_timeout_ms != new_config->health_check_timeout_ms)
	{
		ramd_log_info("Health check timeout changed from %d ms to %d ms",
		              old_config->health_check_timeout_ms,
		              new_config->health_check_timeout_ms);
	}

	return true;
}

bool ramd_config_reload_failover(const ramd_config_t* old_config,
                                 const ramd_config_t* new_config)
{
	if (!old_config || !new_config)
		return false;

	if (old_config->auto_failover_enabled != new_config->auto_failover_enabled)
	{
		ramd_log_info("Auto-failover %s", new_config->auto_failover_enabled ? "enabled" : "disabled");
	}

	if (old_config->failover_timeout_ms != new_config->failover_timeout_ms)
	{
		ramd_log_info("Failover timeout changed from %d ms to %d ms",
		              old_config->failover_timeout_ms,
		              new_config->failover_timeout_ms);
	}

	return true;
}

bool ramd_config_reload_sync_replication(const ramd_config_t* old_config,
                                         const ramd_config_t* new_config)
{
	ramd_sync_mode_t new_mode;

	if (!old_config || !new_config)
		return false;

	if (old_config->synchronous_replication != new_config->synchronous_replication)
	{
		new_mode = new_config->synchronous_replication ? RAMD_SYNC_REMOTE_APPLY : RAMD_SYNC_OFF;

		if (!ramd_sync_replication_set_mode(new_mode))
		{
			ramd_log_error("Failed to update synchronous replication mode");
			return false;
		}

		ramd_log_info("Synchronous replication %s",
		              new_config->synchronous_replication ? "enabled" : "disabled");
	}

	return true;
}

void ramd_config_reload_signal_setup(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = ramd_config_reload_signal_sighup;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGHUP, &sa, NULL) == -1)
	{
		ramd_log_error("Failed to setup SIGHUP handler: %s", strerror(errno));
	}
	else
	{
		ramd_log_debug("SIGHUP signal handler installed for configuration reload");
	}
}

void ramd_config_reload_signal_sighup(int sig)
{
	ramd_config_reload_result_t result;

	(void) sig;

	ramd_log_info("Received SIGHUP signal, triggering configuration reload");

	if (!ramd_config_reload_from_file(NULL, &result))
	{
		ramd_log_error("Configuration reload failed: %s", result.error_message);
	}
}

const char* ramd_config_reload_status_to_string(ramd_config_reload_status_t status)
{
	switch (status)
	{
		case RAMD_CONFIG_RELOAD_SUCCESS:
			return "success";
		case RAMD_CONFIG_RELOAD_FAILED:
			return "failed";
		case RAMD_CONFIG_RELOAD_PARTIAL:
			return "partial";
		case RAMD_CONFIG_RELOAD_NO_CHANGES:
			return "no_changes";
		default:
			return "unknown";
	}
}

bool ramd_config_reload_postgresql(const ramd_config_t* old_config,
                                   const ramd_config_t* new_config)
{
	bool port_changed;
	bool data_dir_changed;

	if (!old_config || !new_config)
		return false;

	port_changed = (old_config->postgresql_port != new_config->postgresql_port);
	data_dir_changed = strcmp(old_config->postgresql_data_dir, new_config->postgresql_data_dir) != 0;

	if (port_changed || data_dir_changed)
	{
		ramd_log_info("PostgreSQL configuration changed - port: %d->%d, data_dir: %s->%s",
		    old_config->postgresql_port, new_config->postgresql_port,
		    old_config->postgresql_data_dir, new_config->postgresql_data_dir);

		if (PQstatus(g_conn) == CONNECTION_OK)
		{
			if (PQexec(g_conn, "SELECT pg_reload_conf()") != NULL)
			{
				ramd_log_info("PostgreSQL configuration reloaded successfully");
			}
			else
			{
				ramd_log_error("Failed to reload PostgreSQL configuration");
			}
		}
	}

	return true;
}

bool ramd_config_reload_cluster(const ramd_config_t* old_config,
                                const ramd_config_t* new_config)
{
	bool cluster_name_changed;
	bool node_count_changed;

	if (!old_config || !new_config)
		return false;

	cluster_name_changed = strcmp(old_config->cluster_name, new_config->cluster_name) != 0;
	node_count_changed = (old_config->cluster_size != new_config->cluster_size);

	if (cluster_name_changed || node_count_changed)
	{
		ramd_log_info("Cluster configuration changed - name: %s->%s, nodes: %d->%d",
		    old_config->cluster_name, new_config->cluster_name,
		    old_config->cluster_size, new_config->cluster_size);

		if (g_ramd_daemon)
		{
			strncpy(g_ramd_daemon->cluster.cluster_name, new_config->cluster_name, 
			        sizeof(g_ramd_daemon->cluster.cluster_name) - 1);
			g_ramd_daemon->cluster.cluster_name[sizeof(g_ramd_daemon->cluster.cluster_name) - 1] = '\0';
		}
	}

	return true;
}

bool ramd_config_reload_http_api(const ramd_config_t* old_config,
                                 const ramd_config_t* new_config)
{
	bool port_changed;
	bool bind_addr_changed;

	if (!old_config || !new_config)
		return false;

	port_changed = (old_config->http_port != new_config->http_port);
	bind_addr_changed = strcmp(old_config->http_bind_address, new_config->http_bind_address) != 0;

	if (port_changed || bind_addr_changed)
	{
		ramd_log_info("HTTP API configuration changed - port: %d->%d, bind_addr: %s->%s",
		    old_config->http_port, new_config->http_port,
		    old_config->http_bind_address, new_config->http_bind_address);

		ramd_log_warning("HTTP API port change requires daemon restart");
	}

	return true;
}

bool ramd_config_reload_maintenance(const ramd_config_t* old_config,
                                    const ramd_config_t* new_config)
{
	bool mode_changed;
	bool drain_timeout_changed;

	if (!old_config || !new_config)
		return false;

	mode_changed = (old_config->maintenance_mode_enabled != new_config->maintenance_mode_enabled);
	drain_timeout_changed = (old_config->maintenance_drain_timeout_ms != new_config->maintenance_drain_timeout_ms);

	if (mode_changed || drain_timeout_changed)
	{
		ramd_log_info("Maintenance configuration changed - mode: %s->%s, drain_timeout: %d->%d ms",
		    old_config->maintenance_mode_enabled ? "enabled" : "disabled",
		    new_config->maintenance_mode_enabled ? "enabled" : "disabled",
		    old_config->maintenance_drain_timeout_ms,
		    new_config->maintenance_drain_timeout_ms);
	}

	return true;
}
