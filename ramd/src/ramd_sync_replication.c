/*-------------------------------------------------------------------------
 *
 * ramd_sync_replication.c
 *		PostgreSQL Auto-Failover Daemon - Synchronous Replication Implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <libpq-fe.h>

#include "ramd_sync_replication.h"
#include "ramd_logging.h"
#include "ramd_postgresql.h"
#include "ramd_basebackup.h"
#include "ramd_conn.h"
#include "ramd_query.h"
#include "ramd_daemon.h"

extern PGconn* g_conn;

static ramd_sync_config_t g_sync_config;
static ramd_sync_status_t g_sync_status;
static bool              g_sync_initialized = false;
static pthread_mutex_t   g_sync_mutex = PTHREAD_MUTEX_INITIALIZER;

bool
ramd_sync_replication_init(ramd_sync_config_t *config)
{
	pthread_mutex_lock(&g_sync_mutex);

	if (config)
		memcpy(&g_sync_config, config, sizeof(ramd_sync_config_t));
	else
	{
		memset(&g_sync_config, 0, sizeof(ramd_sync_config_t));
		g_sync_config.mode = RAMD_SYNC_OFF;
		g_sync_config.num_sync_standbys = 1;
		g_sync_config.sync_timeout_ms = 10000;
		g_sync_config.enforce_sync_standbys = true;
		strncpy(g_sync_config.application_name_pattern, "ramd_node_%d",
		        sizeof(g_sync_config.application_name_pattern) - 1);
	}

	memset(&g_sync_status, 0, sizeof(ramd_sync_status_t));
	g_sync_status.current_mode = g_sync_config.mode;
	g_sync_status.last_status_update = time(NULL);

	g_sync_initialized = true;

	pthread_mutex_unlock(&g_sync_mutex);

	ramd_log_info("Synchronous replication initialized with mode: %s",
	              ramd_sync_mode_to_string(g_sync_config.mode));

	return true;
}

void
ramd_sync_replication_cleanup(void)
{
	pthread_mutex_lock(&g_sync_mutex);
	g_sync_initialized = false;
	pthread_mutex_unlock(&g_sync_mutex);

	ramd_log_info("Synchronous replication cleanup completed");
}

bool
ramd_sync_replication_configure(const ramd_sync_config_t *config)
{
	ramd_sync_mode_t old_mode;

	if (!config || !g_sync_initialized)
		return false;

	pthread_mutex_lock(&g_sync_mutex);

	old_mode = g_sync_config.mode;
	memcpy(&g_sync_config, config, sizeof(ramd_sync_config_t));

	pthread_mutex_unlock(&g_sync_mutex);

	if (old_mode != config->mode)
	{
		ramd_log_info("Synchronous replication mode changed from %s to %s",
		              ramd_sync_mode_to_string(old_mode),
		              ramd_sync_mode_to_string(config->mode));

		if (!ramd_sync_replication_update_postgresql_config())
		{
			ramd_log_error("Failed to update PostgreSQL configuration for sync replication");
			return false;
		}
	}

	return true;
}

bool
ramd_sync_replication_set_mode(ramd_sync_mode_t mode)
{
	ramd_sync_mode_t old_mode;

	if (!g_sync_initialized)
		return false;

	pthread_mutex_lock(&g_sync_mutex);

	old_mode = g_sync_config.mode;
	g_sync_config.mode = mode;
	g_sync_status.current_mode = mode;

	pthread_mutex_unlock(&g_sync_mutex);

	ramd_log_info("Synchronous replication mode set to: %s",
	              ramd_sync_mode_to_string(mode));

	if (old_mode != mode)
		return ramd_sync_replication_update_postgresql_config();

	return true;
}

bool
ramd_sync_replication_add_standby(int32_t node_id, const char *application_name)
{
	int                 slot = -1;
	ramd_sync_standby_t *standby;

	if (!g_sync_initialized || node_id <= 0 || !application_name)
		return false;

	pthread_mutex_lock(&g_sync_mutex);

	for (int i = 0; i < RAMD_MAX_NODES; i++)
	{
		if (g_sync_status.standbys[i].node_id == 0 ||
		    g_sync_status.standbys[i].node_id == node_id)
		{
			slot = i;
			break;
		}
	}

	if (slot == -1)
	{
		pthread_mutex_unlock(&g_sync_mutex);
		ramd_log_error("No available slot for sync standby node %d", node_id);
		return false;
	}

	standby = &g_sync_status.standbys[slot];
	standby->node_id = node_id;
	strncpy(standby->application_name, application_name,
	        sizeof(standby->application_name) - 1);
	standby->is_sync = true;
	standby->state = RAMD_REPLICATION_STATE_STARTUP;
	standby->last_sync_time = time(NULL);

	if (g_sync_status.standbys[slot].node_id == 0)
		g_sync_status.num_potential_standbys++;

	pthread_mutex_unlock(&g_sync_mutex);

	ramd_log_info("Added synchronous standby: node %d (%s)", node_id,
	              application_name);
	return ramd_sync_replication_update_postgresql_config();
}

bool
ramd_sync_replication_remove_standby(int32_t node_id)
{
	bool found = false;

	if (!g_sync_initialized || node_id <= 0)
		return false;

	pthread_mutex_lock(&g_sync_mutex);

	for (int i = 0; i < RAMD_MAX_NODES; i++)
	{
		if (g_sync_status.standbys[i].node_id == node_id)
		{
			memset(&g_sync_status.standbys[i], 0, sizeof(ramd_sync_standby_t));
			g_sync_status.num_potential_standbys--;
			found = true;
			break;
		}
	}

	pthread_mutex_unlock(&g_sync_mutex);

	if (!found)
	{
		ramd_log_warning("Synchronous standby node %d not found for removal",
		                 node_id);
		return false;
	}

	ramd_log_info("Removed synchronous standby: node %d", node_id);
	return ramd_sync_replication_update_postgresql_config();
}

bool
ramd_sync_replication_get_status(ramd_sync_status_t *status)
{
	int  connected_count = 0;
	bool all_healthy = true;

	if (!status || !g_sync_initialized)
		return false;

	pthread_mutex_lock(&g_sync_mutex);

	g_sync_status.last_status_update = time(NULL);

	for (int i = 0; i < RAMD_MAX_NODES; i++)
	{
		if (g_sync_status.standbys[i].node_id > 0 &&
		    g_sync_status.standbys[i].is_sync)
		{
			if (g_sync_status.standbys[i].is_connected &&
			    g_sync_status.standbys[i].state == RAMD_REPLICATION_STATE_STREAMING)
			{
				connected_count++;
			}
			else
			{
				all_healthy = false;
			}
		}
	}

	g_sync_status.num_sync_standbys_connected = connected_count;
	g_sync_status.all_sync_standbys_healthy = all_healthy;

	memcpy(status, &g_sync_status, sizeof(ramd_sync_status_t));

	pthread_mutex_unlock(&g_sync_mutex);
	return true;
}

bool
ramd_sync_replication_check_health(void)
{
	ramd_sync_status_t status;

	if (!ramd_sync_replication_get_status(&status))
		return false;

	if (g_sync_config.mode == RAMD_SYNC_OFF)
		return true;

	if (g_sync_config.enforce_sync_standbys)
	{
		if (status.num_sync_standbys_connected < g_sync_config.num_sync_standbys)
		{
			ramd_log_warning("Insufficient synchronous standbys: %d connected, %d required",
			                 status.num_sync_standbys_connected,
			                 g_sync_config.num_sync_standbys);
			return false;
		}
	}

	return status.all_sync_standbys_healthy;
}

bool
ramd_sync_replication_update_postgresql_config(void)
{
	char      standby_names[512];
	char      config_sql[1024];
	PGresult *res;

	if (!g_sync_initialized)
		return false;

	if (!ramd_sync_generate_standby_names(standby_names, sizeof(standby_names),
	                                      g_sync_status.standbys,
	                                      RAMD_MAX_NODES))
	{
		ramd_log_error("Failed to generate synchronous_standby_names");
		return false;
	}

	const char *sync_commit_value = ramd_sync_mode_to_string(g_sync_config.mode);
	snprintf(config_sql, sizeof(config_sql),
	         "ALTER SYSTEM SET synchronous_commit = '%s'", sync_commit_value);

	res = ramd_query_exec_with_result(g_conn, config_sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		ramd_log_error("Failed to set synchronous_commit: %s",
		               PQerrorMessage(g_conn));
		PQclear(res);
		return false;
	}
	PQclear(res);

	snprintf(config_sql, sizeof(config_sql),
	         "ALTER SYSTEM SET synchronous_standby_names = '%s'",
	         standby_names);

	res = ramd_query_exec_with_result(g_conn, config_sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		ramd_log_error("Failed to set synchronous_standby_names: %s",
		               PQerrorMessage(g_conn));
		PQclear(res);
		return false;
	}
	PQclear(res);

	res = ramd_query_exec_with_result(g_conn, "SELECT pg_reload_conf()");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		ramd_log_error("Failed to reload PostgreSQL configuration: %s",
		               PQerrorMessage(g_conn));
		PQclear(res);
		return false;
	}
	PQclear(res);

	ramd_log_info("Updated PostgreSQL synchronous replication configuration");
	ramd_log_debug("synchronous_commit = %s", sync_commit_value);
	ramd_log_debug("synchronous_standby_names = %s", standby_names);

	return true;
}

bool
ramd_sync_replication_reload_config(void)
{
	ramd_log_info("Reloading synchronous replication configuration");
	return ramd_sync_replication_update_postgresql_config();
}

char *
ramd_sync_mode_to_string(ramd_sync_mode_t mode)
{
	switch (mode)
	{
		case RAMD_SYNC_OFF:
			return "off";
		case RAMD_SYNC_LOCAL:
			return "local";
		case RAMD_SYNC_REMOTE_WRITE:
			return "remote_write";
		case RAMD_SYNC_REMOTE_APPLY:
			return "remote_apply";
		default:
			return "off";
	}
}

ramd_sync_mode_t
ramd_sync_string_to_mode(const char *mode_str)
{
	if (!mode_str)
		return RAMD_SYNC_OFF;

	if (strcmp(mode_str, "off") == 0)
		return RAMD_SYNC_OFF;
	else if (strcmp(mode_str, "local") == 0)
		return RAMD_SYNC_LOCAL;
	else if (strcmp(mode_str, "remote_write") == 0)
		return RAMD_SYNC_REMOTE_WRITE;
	else if (strcmp(mode_str, "remote_apply") == 0)
		return RAMD_SYNC_REMOTE_APPLY;
	else
		return RAMD_SYNC_OFF;
}

bool
ramd_sync_generate_standby_names(char *output, size_t output_size,
                                const ramd_sync_standby_t *standbys,
                                int32_t count)
{
	char temp[512];
	bool first = true;
	int  sync_count = 0;

	if (!output || output_size == 0 || !standbys)
		return false;

	output[0] = '\0';

	if (g_sync_config.num_sync_standbys <= 0)
		return true;

	snprintf(temp, sizeof(temp), "FIRST %d (",
	         g_sync_config.num_sync_standbys);

	strncat(output, temp, output_size - strlen(output) - 1);

	for (int32_t i = 0; i < count && sync_count < g_sync_config.num_sync_standbys; i++)
	{
		if (standbys[i].node_id > 0 && standbys[i].is_sync)
		{
			if (!first)
				strncat(output, ",", output_size - strlen(output) - 1);

			strncat(output, standbys[i].application_name,
			        output_size - strlen(output) - 1);
			first = false;
			sync_count++;
		}
	}

	strncat(output, ")", output_size - strlen(output) - 1);

	return true;
}

bool
ramd_sync_standby_promote_to_sync(int32_t node_id)
{
	ramd_node_t *node;

	if (!g_ramd_daemon || node_id <= 0 ||
	    node_id > g_ramd_daemon->cluster.node_count)
		return false;

	node = &g_ramd_daemon->cluster.nodes[node_id - 1];
	if (node->state != RAMD_NODE_STATE_STANDBY || !node->is_healthy)
		return false;

	g_sync_config.num_sync_standbys++;

	if (!ramd_sync_replication_reload_config())
		return false;

	ramd_log_info("Promoted node %d to synchronous standby", node_id);
	return true;
}

bool
ramd_sync_standby_demote_from_sync(int32_t node_id)
{
	ramd_node_t *node;

	if (!g_ramd_daemon || node_id <= 0 ||
	    node_id > g_ramd_daemon->cluster.node_count)
		return false;

	node = &g_ramd_daemon->cluster.nodes[node_id - 1];
	if (node->state != RAMD_NODE_STATE_STANDBY)
		return false;

	if (g_sync_config.num_sync_standbys > 0)
		g_sync_config.num_sync_standbys--;

	if (!ramd_sync_replication_reload_config())
		return false;

	ramd_log_info("Demoted node %d from synchronous standby", node_id);
	return true;
}

bool
ramd_sync_standby_is_eligible(int32_t node_id)
{
	ramd_node_t *node;

	if (!g_ramd_daemon || node_id <= 0 ||
	    node_id > g_ramd_daemon->cluster.node_count)
		return false;

	node = &g_ramd_daemon->cluster.nodes[node_id - 1];

	return (node->state == RAMD_NODE_STATE_STANDBY && node->is_healthy &&
	        node->replication_lag_ms < RAMD_DEFAULT_REPLICATION_LAG_THRESHOLD);
}

bool
ramd_sync_replication_check_lag(ramd_sync_standby_t *standby)
{
	if (!standby)
		return false;

	return (standby->flush_lag_bytes < RAMD_DEFAULT_REPLICATION_LAG_THRESHOLD * 1024);
}

bool
ramd_sync_replication_wait_for_sync(int32_t timeout_ms)
{
	int32_t elapsed = 0;
	int32_t check_interval = RAMD_DEFAULT_SYNC_TIMEOUT_MS;

	while (elapsed < timeout_ms)
	{
		if (g_sync_status.current_mode != RAMD_SYNC_OFF &&
		    g_sync_status.num_sync_standbys_connected >= g_sync_config.num_sync_standbys)
		{
			return true;
		}

		usleep((useconds_t)(check_interval * 1000));
		elapsed += check_interval;
	}

	return false;
}

bool
ramd_sync_replication_take_basebackup(const ramd_config_t *config,
                                     const char *primary_host,
                                     int32_t primary_port)
{
	int result;

	if (!config || !primary_host)
		return false;

	ramd_log_info("Taking base backup from primary %s:%d", primary_host,
	              primary_port);

	result = ramd_take_basebackup(g_conn, config->postgresql_data_dir,
	                             "sync_replication_backup");

	if (result != 0)
	{
		ramd_log_error("Base backup failed");
		return false;
	}

	ramd_log_info("Base backup completed successfully");
	return true;
}

bool
ramd_sync_replication_configure_recovery(const ramd_config_t *config,
                                       const char *primary_host,
                                       int32_t primary_port)
{
	char     recovery_conf_path[RAMD_MAX_PATH_LENGTH];
	char     auto_conf_path[RAMD_MAX_PATH_LENGTH];
	char     trigger_file[RAMD_MAX_PATH_LENGTH];
	char     trigger_dir[RAMD_MAX_PATH_LENGTH];
	bool     use_recovery_conf = true;
	FILE    *recovery_file;
	FILE    *auto_conf_file;
	PGresult *res;

	if (!config || !primary_host)
		return false;

	ramd_log_info("Configuring recovery for streaming replication from %s:%d",
	              primary_host, primary_port);

	snprintf(recovery_conf_path, sizeof(recovery_conf_path), "%s/recovery.conf",
	         config->postgresql_data_dir);

	res = ramd_query_exec_with_result(g_conn, "SHOW server_version_num");
	if (res && PQntuples(res) > 0)
	{
		int version_num = atoi(PQgetvalue(res, 0, 0));
		use_recovery_conf = (version_num < 120000);
		PQclear(res);
	}

	if (use_recovery_conf)
	{
		recovery_file = fopen(recovery_conf_path, "w");
		if (!recovery_file)
		{
			ramd_log_error("Failed to create recovery.conf file");
			return false;
		}

		fprintf(recovery_file, "standby_mode = 'on'\n");
		fprintf(recovery_file,
		        "primary_conninfo = 'host=%s port=%d user=%s password=%s'\n",
		        primary_host, primary_port,
		        config->replication_user, config->replication_user);
		fprintf(recovery_file, "recovery_target_timeline = 'latest'\n");
		fprintf(recovery_file, "trigger_file = '%s/failover.trigger'\n",
		        config->postgresql_data_dir);

		fclose(recovery_file);
		ramd_log_info("Created recovery.conf for streaming replication");
	}
	else
	{
		snprintf(auto_conf_path, sizeof(auto_conf_path),
		         "%s/postgresql.auto.conf", config->postgresql_data_dir);

		auto_conf_file = fopen(auto_conf_path, "a");
		if (!auto_conf_file)
		{
			ramd_log_error("Failed to append to postgresql.auto.conf");
			return false;
		}

		fprintf(auto_conf_file,
		        "primary_conninfo = 'host=%s port=%d user=%s password=%s'\n",
		        primary_host, primary_port,
		        config->replication_user, config->replication_user);
		fprintf(auto_conf_file, "recovery_target_timeline = 'latest'\n");
		fprintf(auto_conf_file, "promote_trigger_file = '%s/failover.trigger'\n",
		        config->postgresql_data_dir);

		fclose(auto_conf_file);
		ramd_log_info("Updated postgresql.auto.conf for streaming replication");
	}

	snprintf(trigger_dir, sizeof(trigger_dir), "%s",
	         config->postgresql_data_dir);

	snprintf(trigger_file, sizeof(trigger_file), "%s/failover.trigger",
	         trigger_dir);
	unlink(trigger_file);

	ramd_log_info("Recovery configuration completed successfully");
	return true;
}

bool
ramd_sync_replication_setup_streaming(const ramd_config_t *config,
                                     const char *primary_host,
                                     int32_t primary_port,
                                     const char *application_name)
{
	PGresult *res;

	if (!config || !primary_host)
		return false;

	ramd_log_info("Setting up streaming replication to primary %s:%d",
	              primary_host, primary_port);

	if (!ramd_postgresql_stop(config))
		ramd_log_warning("Could not stop PostgreSQL, continuing with replication setup");

	if (!ramd_sync_replication_take_basebackup(config, primary_host, primary_port))
	{
		ramd_log_error("Failed to take base backup for streaming replication");
		return false;
	}

	if (!ramd_sync_replication_configure_recovery(config, primary_host, primary_port))
	{
		ramd_log_error("Failed to configure recovery for streaming replication");
		return false;
	}

	if (!ramd_postgresql_start(config))
	{
		ramd_log_error("Failed to start PostgreSQL as standby");
		return false;
	}

	ramd_log_info("Waiting for streaming replication to start...");
	sleep(RAMD_DEFAULT_CONNECTION_TIMEOUT);

	res = ramd_query_exec_with_result(g_conn, "SELECT pg_is_in_recovery()");
	if (res && PQntuples(res) > 0)
	{
		bool is_recovering = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
		PQclear(res);

		if (is_recovering)
		{
			const char *paramValues[1] = {
				application_name ? application_name : "ramd_node"
			};
			res = ramd_query_exec_params(g_conn,
			                            "SELECT application_name, state, sent_lsn, write_lsn, "
			                            "flush_lsn, replay_lsn FROM pg_stat_replication WHERE "
			                            "application_name = $1",
			                            1, NULL, paramValues, NULL, NULL, 0);

			if (res && PQntuples(res) > 0)
			{
				ramd_log_info("Streaming replication established successfully");
				ramd_log_info("Replication state: %s", PQgetvalue(res, 0, 1));
				PQclear(res);
				return true;
			}
			else
			{
				ramd_log_warning("Replication established but no replication slots found");
				PQclear(res);
			}
		}
		else
			ramd_log_error("PostgreSQL is not in recovery mode");
	}

	ramd_log_error("Failed to verify streaming replication status");
	return false;
}

bool
ramd_sync_replication_promote_standby(const ramd_config_t *config)
{
	char      trigger_file[RAMD_MAX_PATH_LENGTH];
	FILE     *trigger;
	PGresult *res;

	if (!config)
		return false;

	ramd_log_info("Promoting standby to primary");

	snprintf(trigger_file, sizeof(trigger_file), "%s/failover.trigger",
	         config->postgresql_data_dir);

	trigger = fopen(trigger_file, "w");
	if (!trigger)
	{
		ramd_log_error("Failed to create failover trigger file");
		return false;
	}

	fprintf(trigger, "promote\n");
	fclose(trigger);

	ramd_log_info("Created failover trigger file, waiting for promotion...");

	sleep(RAMD_DEFAULT_CONNECTION_TIMEOUT);

	res = ramd_query_exec_with_result(g_conn, "SELECT pg_is_in_recovery()");
	if (res && PQntuples(res) > 0)
	{
		bool is_recovering = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
		PQclear(res);

		if (!is_recovering)
		{
			ramd_log_info("Standby successfully promoted to primary");

			unlink(trigger_file);

			res = ramd_query_exec_with_result(g_conn,
			                                  "ALTER SYSTEM SET synchronous_standby_names = ''");
			if (res)
			{
				PQclear(res);
				ramd_log_info("Cleared synchronous_standby_names for new primary");
			}

			res = ramd_query_exec_with_result(g_conn,
			                                  "SELECT pg_reload_conf()");
			if (res)
			{
				PQclear(res);
				ramd_log_info("PostgreSQL configuration reloaded");
			}

			return true;
		}
		else
			ramd_log_error("Standby is still in recovery mode after promotion attempt");
	}

	ramd_log_error("Failed to verify standby promotion");
	return false;
}
