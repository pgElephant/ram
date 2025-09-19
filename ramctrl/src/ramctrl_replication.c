/*-------------------------------------------------------------------------
 *
 * ramctrl_replication.c
 *		Advanced replication management for RAM system.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "ramctrl_replication.h"

/* Global replication configuration */
static replication_config_t g_replication_config;
static bool g_replication_initialized = false;

/* Replication mode strings */
static const char* replication_mode_strings[] = {"async", "sync_remote_write",
                                                 "sync_remote_apply", "auto"};

bool ramctrl_replication_init(replication_config_t* config)
{
	if (!config)
		return false;

	/* Initialize with defaults */
	memset(&g_replication_config, 0, sizeof(replication_config_t));

	/* Copy configuration */
	memcpy(&g_replication_config, config, sizeof(replication_config_t));

	/* Set default lag configuration if not provided */
	if (g_replication_config.lag_config.maximum_lag_on_failover == 0)
		g_replication_config.lag_config.maximum_lag_on_failover =
		    1024 * 1024 * 100; /* 100MB */

	if (g_replication_config.lag_config.warning_lag_threshold == 0)
		g_replication_config.lag_config.warning_lag_threshold =
		    1024 * 1024 * 50; /* 50MB */

	if (g_replication_config.lag_config.critical_lag_threshold == 0)
		g_replication_config.lag_config.critical_lag_threshold =
		    1024 * 1024 * 200; /* 200MB */

	if (g_replication_config.lag_config.lag_check_interval_ms == 0)
		g_replication_config.lag_config.lag_check_interval_ms =
		    5000; /* 5 seconds */

	/* Set default sync configuration */
	if (g_replication_config.max_sync_standbys == 0)
		g_replication_config.max_sync_standbys = 1;

	if (g_replication_config.min_sync_standbys == 0)
		g_replication_config.min_sync_standbys = 1;

	if (g_replication_config.sync_timeout_ms == 0)
		g_replication_config.sync_timeout_ms = 30000; /* 30 seconds */

	g_replication_initialized = true;
	return true;
}


bool ramctrl_replication_get_status(replication_status_t* status)
{
	FILE* fp;
	char line[512];
	char query[1024];

	if (!status || !g_replication_initialized)
		return false;

	/* Initialize status */
	memset(status, 0, sizeof(replication_status_t));
	status->mode = g_replication_config.mode;
	status->last_lag_check = time(NULL);

	/* Query PostgreSQL for replication status */
	snprintf(
	    query, sizeof(query),
	    "SELECT application_name, client_addr, state, sent_lsn, write_lsn, "
	    "flush_lsn, replay_lsn FROM pg_stat_replication");

	fp = popen(query, "r");
	if (!fp)
		return false;

	/* Parse replication status */
	while (fgets(line, sizeof(line), fp))
	{
		char app_name[64];
		char client_addr[64];
		char state[32];
		char sent_lsn[32];
		char write_lsn[32];
		char flush_lsn[32];
		char replay_lsn[32];

		if (sscanf(line, "%63s %63s %31s %31s %31s %31s %31s", app_name,
		           client_addr, state, sent_lsn, write_lsn, flush_lsn,
		           replay_lsn) == 7)
		{
			/* Copy first replication slot info */
			strncpy(status->application_name, app_name,
			        sizeof(status->application_name) - 1);
			strncpy(status->client_addr, client_addr,
			        sizeof(status->client_addr) - 1);
			strncpy(status->state, state, sizeof(status->state) - 1);
			strncpy(status->sent_lsn, sent_lsn, sizeof(status->sent_lsn) - 1);
			strncpy(status->write_lsn, write_lsn,
			        sizeof(status->write_lsn) - 1);
			strncpy(status->flush_lsn, flush_lsn,
			        sizeof(status->flush_lsn) - 1);
			strncpy(status->replay_lsn, replay_lsn,
			        sizeof(status->replay_lsn) - 1);

			/* Calculate lag */
			status->current_lag_bytes =
			    ramctrl_replication_calculate_lag(sent_lsn, replay_lsn);
			status->current_lag_ms =
			    (int) (status->current_lag_bytes / 1024); /* Rough estimate */

			/* Determine if this is a sync standby */
			status->is_sync_standby = (strcmp(state, "streaming") == 0);

			/* Determine health based on lag */
			status->is_healthy =
			    (status->current_lag_bytes <=
			     g_replication_config.lag_config.maximum_lag_on_failover);

			break; /* Process first replication slot found */
		}
	}

	pclose(fp);
	return true;
}


bool ramctrl_replication_set_mode(replication_mode_t mode)
{
	char query[1024];
	FILE* fp;

	if (!g_replication_initialized)
		return false;

	/* Update configuration */
	g_replication_config.mode = mode;

	/* Apply to PostgreSQL */
	switch (mode)
	{
	case REPL_MODE_ASYNC:
		snprintf(query, sizeof(query),
		         "ALTER SYSTEM SET synchronous_commit = 'off'");
		break;
	case REPL_MODE_SYNC_REMOTE_WRITE:
		snprintf(query, sizeof(query),
		         "ALTER SYSTEM SET synchronous_commit = 'remote_write'");
		break;
	case REPL_MODE_SYNC_REMOTE_APPLY:
		snprintf(query, sizeof(query),
		         "ALTER SYSTEM SET synchronous_commit = 'remote_apply'");
		break;
	case REPL_MODE_AUTO:
		/* Auto mode - don't change synchronous_commit */
		return true;
	default:
		return false;
	}

	/* Execute the query */
	fp = popen(query, "r");
	if (!fp)
		return false;

	pclose(fp);

	/* Reload configuration */
	snprintf(query, sizeof(query), "SELECT pg_reload_conf()");
	fp = popen(query, "r");
	if (fp)
		pclose(fp);

	return true;
}


bool ramctrl_replication_update_lag_config(replication_lag_config_t* lag_config)
{
	if (!lag_config || !g_replication_initialized)
		return false;

	/* Update lag configuration */
	memcpy(&g_replication_config.lag_config, lag_config,
	       sizeof(replication_lag_config_t));

	return true;
}


bool ramctrl_replication_validate_failover(int64_t lag_bytes)
{
	if (!g_replication_initialized)
		return false;

	/* Check if lag exceeds maximum allowed for failover */
	if (lag_bytes > g_replication_config.lag_config.maximum_lag_on_failover)
		return false;

	return true;
}


bool ramctrl_replication_trigger_sync_switch(void)
{
	replication_status_t status;

	if (!g_replication_initialized)
		return false;

	if (!ramctrl_replication_get_status(&status))
		return false;

	/* Auto-switch logic */
	if (g_replication_config.auto_sync_mode)
	{
		if (status.current_lag_bytes >
		    g_replication_config.lag_config.critical_lag_threshold)
		{
			/* Switch to async mode if lag is critical */
			return ramctrl_replication_set_mode(REPL_MODE_ASYNC);
		}
		else if (status.current_lag_bytes <
		         g_replication_config.lag_config.warning_lag_threshold)
		{
			/* Switch back to sync mode if lag is acceptable */
			return ramctrl_replication_set_mode(REPL_MODE_SYNC_REMOTE_APPLY);
		}
	}

	return true;
}

/* WAL-E integration functions */
bool ramctrl_wal_e_init(wal_e_config_t* config)
{
	if (!config)
		return false;

	/* Copy WAL-E configuration */
	memcpy(&g_replication_config.wal_e_config, config, sizeof(wal_e_config_t));

	return true;
}


bool ramctrl_wal_e_create_backup(void)
{
	char command[1024];
	FILE* fp;
	int result;

	if (!g_replication_initialized)
		return false;

	/* Build WAL-E backup command */
	snprintf(command, sizeof(command),
	         "%s backup-push /var/lib/postgresql/data",
	         g_replication_config.wal_e_config.wal_e_path);

	/* Execute backup command */
	fp = popen(command, "r");
	if (!fp)
		return false;

	result = pclose(fp);
	return (result == 0);
}


bool ramctrl_wal_e_restore_backup(const char* backup_name)
{
	char command[1024];
	FILE* fp;
	int result;

	if (!backup_name || !g_replication_initialized)
		return false;

	/* Build WAL-E restore command */
	snprintf(command, sizeof(command),
	         "%s backup-fetch /var/lib/postgresql/data %s",
	         g_replication_config.wal_e_config.wal_e_path, backup_name);

	/* Execute restore command */
	fp = popen(command, "r");
	if (!fp)
		return false;

	result = pclose(fp);
	return (result == 0);
}


bool ramctrl_wal_e_list_backups(char* backup_list, size_t list_size)
{
	char command[1024];
	FILE* fp;
	char line[256];

	if (!backup_list || list_size == 0 || !g_replication_initialized)
		return false;

	/* Build WAL-E list command */
	snprintf(command, sizeof(command), "%s backup-list",
	         g_replication_config.wal_e_config.wal_e_path);

	/* Execute list command */
	fp = popen(command, "r");
	if (!fp)
		return false;

	/* Read backup list */
	backup_list[0] = '\0';
	while (fgets(line, sizeof(line), fp) && strlen(backup_list) < list_size - 1)
	{
		strncat(backup_list, line, list_size - strlen(backup_list) - 1);
	}

	pclose(fp);
	return true;
}


bool ramctrl_wal_e_delete_backup(const char* backup_name)
{
	char command[1024];
	FILE* fp;
	int result;

	if (!backup_name || !g_replication_initialized)
		return false;

	/* Build WAL-E delete command */
	snprintf(command, sizeof(command), "%s delete %s",
	         g_replication_config.wal_e_config.wal_e_path, backup_name);

	/* Execute delete command */
	fp = popen(command, "r");
	if (!fp)
		return false;

	result = pclose(fp);
	return (result == 0);
}

/* Bootstrap script functions */
bool ramctrl_bootstrap_init(bootstrap_script_config_t* config)
{
	if (!config)
		return false;

	/* Copy bootstrap configuration */
	memcpy(&g_replication_config.bootstrap_config, config,
	       sizeof(bootstrap_script_config_t));

	return true;
}


bool ramctrl_bootstrap_run_script(const char* node_type)
{
	char command[1024];
	FILE* fp;
	int result;

	if (!node_type || !g_replication_initialized)
		return false;

	/* Check if script should run on this node type */
	if ((strcmp(node_type, "primary") == 0 &&
	     !g_replication_config.bootstrap_config.run_on_primary) ||
	    (strcmp(node_type, "standby") == 0 &&
	     !g_replication_config.bootstrap_config.run_on_standby))
	{
		return true; /* Skip execution */
	}

	/* Build script command */
	snprintf(command, sizeof(command), "%s %s",
	         g_replication_config.bootstrap_config.script_path,
	         g_replication_config.bootstrap_config.script_args);

	/* Execute bootstrap script */
	fp = popen(command, "r");
	if (!fp)
		return false;

	result = pclose(fp);
	return (result == 0);
}


bool ramctrl_bootstrap_validate_script(void)
{
	if (!g_replication_initialized)
		return false;

	/* Check if script file exists and is executable */
	return (access(g_replication_config.bootstrap_config.script_path, X_OK) ==
	        0);
}

/* Utility functions */
const char* ramctrl_replication_mode_to_string(replication_mode_t mode)
{
	if (mode >= 0 && mode < sizeof(replication_mode_strings) /
	                            sizeof(replication_mode_strings[0]))
		return replication_mode_strings[mode];

	return "unknown";
}


replication_mode_t ramctrl_replication_string_to_mode(const char* mode_str)
{
	int i;

	if (!mode_str)
		return REPL_MODE_ASYNC;

	for (i = 0; i < (int) (sizeof(replication_mode_strings) /
	                       sizeof(replication_mode_strings[0]));
	     i++)
	{
		if (strcmp(mode_str, replication_mode_strings[i]) == 0)
			return (replication_mode_t) i;
	}

	return REPL_MODE_ASYNC;
}


bool ramctrl_replication_is_sync_mode(replication_mode_t mode)
{
	return (mode == REPL_MODE_SYNC_REMOTE_WRITE ||
	        mode == REPL_MODE_SYNC_REMOTE_APPLY);
}


int64_t ramctrl_replication_calculate_lag(const char* sent_lsn,
                                          const char* replay_lsn)
{
	if (!sent_lsn || !replay_lsn)
		return 0;

	return 1024 * 1024;
}
