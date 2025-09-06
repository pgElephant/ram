/*-------------------------------------------------------------------------
 *
 * ramd_config.c
 *		PostgreSQL Auto-Failover Daemon - Configuration Management
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ramd_config.h"
#include "ramd_logging.h"
#include "ramd_defaults.h"

bool ramd_config_init(ramd_config_t* config)
{
	if (!config)
		return false;

	memset(config, 0, sizeof(ramd_config_t));
	ramd_config_set_defaults(config);

	return true;
}


void ramd_config_set_defaults(ramd_config_t* config)
{
	if (!config)
		return;

	/* Node identification defaults */
	config->node_id = 1;
	config->hostname[0] = '\0'; /* No default hostname - must be configured */
	config->postgresql_port = RAMD_DEFAULT_PORT;
	config->rale_port = RAMD_DEFAULT_RALE_PORT;
	config->dstore_port = RAMD_DEFAULT_DSTORE_PORT;

	/* PostgreSQL defaults - use environment variables or standard paths */
	char* pgbin = getenv("PGBIN");
	char* pgdata = getenv("PGDATA");
	char* pglog = getenv("PGLOG");

	strncpy(config->postgresql_bin_dir, pgbin ? pgbin : RAMD_DEFAULT_PG_BIN_DIR,
	        sizeof(config->postgresql_bin_dir) - 1);
	strncpy(config->postgresql_data_dir,
	        pgdata ? pgdata : RAMD_DEFAULT_PG_DATA_DIR,
	        sizeof(config->postgresql_data_dir) - 1);
	strncpy(config->postgresql_log_dir, pglog ? pglog : RAMD_DEFAULT_PG_LOG_DIR,
	        sizeof(config->postgresql_log_dir) - 1);
	/* Database defaults - should be configured */
	const char* pgdb = getenv("PGDATABASE");
	const char* pguser = getenv("PGUSER");
	strncpy(config->database_name, pgdb ? pgdb : RAMD_DEFAULT_PG_DATABASE,
	        sizeof(config->database_name) - 1);
	strncpy(config->database_user, pguser ? pguser : RAMD_DEFAULT_PG_USER,
	        sizeof(config->database_user) - 1);

	/* Cluster defaults */
	strncpy(config->cluster_name, RAMD_DEFAULT_CLUSTER_NAME,
	        sizeof(config->cluster_name) - 1);
	config->cluster_size = RAMD_DEFAULT_CLUSTER_SIZE;
	config->auto_failover_enabled = true;
	config->synchronous_replication = false;

	/* Monitoring defaults */
	config->monitor_interval_ms = RAMD_MONITOR_INTERVAL_MS;
	config->health_check_timeout_ms = RAMD_HEALTH_CHECK_TIMEOUT_MS;
	config->failover_timeout_ms = RAMD_FAILOVER_TIMEOUT_MS;
	config->recovery_timeout_ms = 300000; /* 5 minutes */

	/* Logging defaults - use stderr if not configured */
	config->log_file[0] = '\0'; /* No default log file - use stderr */
	config->log_level = RAMD_LOG_LEVEL_INFO;
	config->log_to_syslog = false;
	config->log_to_console = true;

	/* HTTP API defaults */
	config->http_api_enabled = true;
	/* Temporary default for testing - should be configured for security */
	strncpy(config->http_bind_address, "127.0.0.1", sizeof(config->http_bind_address) - 1);
	config->http_port = RAMD_DEFAULT_HTTP_PORT; /* Use default port for testing */
	config->http_auth_enabled = false;
	config->http_auth_token[0] = '\0';

	/* Synchronous replication defaults */
	config->sync_standby_names[0] = '\0';
	config->num_sync_standbys = 1;
	config->sync_timeout_ms = 10000;
	config->enforce_sync_standbys = true;

	/* Maintenance mode defaults */
	config->maintenance_mode_enabled = true;
	config->maintenance_drain_timeout_ms = 30000;
	config->maintenance_backup_before = false;

	/* Daemon defaults */
	config->pid_file[0] = '\0';
	config->daemonize = false;
	config->user[0] = '\0';
	config->group[0] = '\0';
}


bool ramd_config_load_file(ramd_config_t* config, const char* config_file)
{
	FILE* fp;
	char line[1024];
	int line_number = 0;

	if (!config || !config_file)
		return false;

	fp = fopen(config_file, "r");
	if (!fp)
	{
		ramd_log_error("Failed to open configuration file: %s", config_file);
		return false;
	}

	while (fgets(line, sizeof(line), fp))
	{
		line_number++;

		if (line[0] == '\0' || line[0] == '\n' || line[0] == '#')
			continue;

		if (!ramd_config_parse_line(config, line))
		{
			ramd_log_warning("Invalid configuration line %d: %s", line_number,
			                 line);
		}
	}

	fclose(fp);

	if (!ramd_config_validate(config))
	{
		ramd_log_error("Configuration validation failed");
		return false;
	}

	ramd_log_info("Configuration loaded from: %s", config_file);
	return true;
}


bool ramd_config_parse_line(ramd_config_t* config, const char* line)
{
	char *key, *value, *line_copy;
	char* equals_pos;

	if (!config || !line)
		return false;

	line_copy = strdup(line);
	if (!line_copy)
		return false;

	char* newline = strchr(line_copy, '\n');
	if (newline)
		*newline = '\0';

	equals_pos = strchr(line_copy, '=');
	if (!equals_pos)
	{
		free(line_copy);
		return false;
	}

	*equals_pos = '\0';
	key = line_copy;
	value = equals_pos + 1;
	while (*key == ' ' || *key == '\t')
		key++;
	while (*value == ' ' || *value == '\t')
		value++;

	bool result = ramd_config_parse_key_value(config, key, value);

	free(line_copy);
	return result;
}


bool ramd_config_parse_key_value(ramd_config_t* config, const char* key,
                                 const char* value)
{
	if (!config || !key || !value)
		return false;

	if (strcmp(key, "node_id") == 0)
		config->node_id = atoi(value);
	else if (strcmp(key, "hostname") == 0)
		strncpy(config->hostname, value, sizeof(config->hostname) - 1);
	else if (strcmp(key, "postgresql_port") == 0)
		config->postgresql_port = atoi(value);
	else if (strcmp(key, "rale_port") == 0)
		config->rale_port = atoi(value);
	else if (strcmp(key, "dstore_port") == 0)
		config->dstore_port = atoi(value);
	else if (strcmp(key, "postgresql_bin_dir") == 0)
		strncpy(config->postgresql_bin_dir, value,
		        sizeof(config->postgresql_bin_dir) - 1);
	else if (strcmp(key, "postgresql_data_dir") == 0)
		strncpy(config->postgresql_data_dir, value,
		        sizeof(config->postgresql_data_dir) - 1);
	else if (strcmp(key, "database_name") == 0)
		strncpy(config->database_name, value,
		        sizeof(config->database_name) - 1);
	else if (strcmp(key, "database_user") == 0)
		strncpy(config->database_user, value,
		        sizeof(config->database_user) - 1);
	else if (strcmp(key, "database_password") == 0)
		strncpy(config->database_password, value,
		        sizeof(config->database_password) - 1);
	else if (strcmp(key, "cluster_name") == 0)
		strncpy(config->cluster_name, value, sizeof(config->cluster_name) - 1);
	else if (strcmp(key, "cluster_size") == 0)
		config->cluster_size = atoi(value);
	else if (strcmp(key, "auto_failover_enabled") == 0)
		config->auto_failover_enabled =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "synchronous_replication") == 0)
		config->synchronous_replication =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "monitor_interval_ms") == 0)
		config->monitor_interval_ms = atoi(value);
	else if (strcmp(key, "health_check_timeout_ms") == 0)
		config->health_check_timeout_ms = atoi(value);
	else if (strcmp(key, "failover_timeout_ms") == 0)
		config->failover_timeout_ms = atoi(value);
	else if (strcmp(key, "log_file") == 0)
		strncpy(config->log_file, value, sizeof(config->log_file) - 1);
	else if (strcmp(key, "log_level") == 0)
		config->log_level = ramd_logging_string_to_level(value);
	else if (strcmp(key, "log_to_syslog") == 0)
		config->log_to_syslog =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "log_to_console") == 0)
		config->log_to_console =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "http_api_enabled") == 0)
		config->http_api_enabled =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "http_bind_address") == 0)
		strncpy(config->http_bind_address, value,
		        sizeof(config->http_bind_address) - 1);
	else if (strcmp(key, "http_port") == 0)
		config->http_port = atoi(value);
	else if (strcmp(key, "http_auth_enabled") == 0)
		config->http_auth_enabled =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "http_auth_token") == 0)
		strncpy(config->http_auth_token, value,
		        sizeof(config->http_auth_token) - 1);
	else if (strcmp(key, "sync_standby_names") == 0)
		strncpy(config->sync_standby_names, value,
		        sizeof(config->sync_standby_names) - 1);
	else if (strcmp(key, "num_sync_standbys") == 0)
		config->num_sync_standbys = atoi(value);
	else if (strcmp(key, "sync_timeout_ms") == 0)
		config->sync_timeout_ms = atoi(value);
	else if (strcmp(key, "enforce_sync_standbys") == 0)
		config->enforce_sync_standbys =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "maintenance_mode_enabled") == 0)
		config->maintenance_mode_enabled =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "maintenance_drain_timeout_ms") == 0)
		config->maintenance_drain_timeout_ms = atoi(value);
	else if (strcmp(key, "maintenance_backup_before") == 0)
		config->maintenance_backup_before =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else if (strcmp(key, "pid_file") == 0)
		strncpy(config->pid_file, value, sizeof(config->pid_file) - 1);
	else if (strcmp(key, "daemonize") == 0)
		config->daemonize =
		    (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
	else
		return false; /* Unknown key */

	return true;
}


bool ramd_config_validate(const ramd_config_t* config)
{
	if (!config)
		return false;

	/* Validate node ID */
	if (config->node_id <= 0 || config->node_id > RAMD_MAX_NODES)
	{
		ramd_log_error("Invalid node_id: %d (must be 1-%d)", config->node_id,
		               RAMD_MAX_NODES);
		return false;
	}

	/* Validate hostname */
	if (strlen(config->hostname) == 0)
	{
		ramd_log_error("hostname cannot be empty");
		return false;
	}

	/* Validate ports */
	if (config->postgresql_port <= 0 || config->postgresql_port > 65535)
	{
		ramd_log_error("Invalid postgresql_port: %d", config->postgresql_port);
		return false;
	}

	/* Validate directories */
	if (strlen(config->postgresql_data_dir) == 0)
	{
		ramd_log_error("postgresql_data_dir cannot be empty");
		return false;
	}

	/* Validate cluster settings */
	if (config->cluster_size < 1 || config->cluster_size > RAMD_MAX_NODES)
	{
		ramd_log_error("Invalid cluster_size: %d (must be 1-%d)",
		               config->cluster_size, RAMD_MAX_NODES);
		return false;
	}

	/* Validate timeouts */
	if (config->monitor_interval_ms <= 0)
	{
		ramd_log_error("monitor_interval_ms must be positive");
		return false;
	}

	return true;
}


void ramd_config_cleanup(ramd_config_t* config)
{
	if (!config)
		return;

	/* Clear sensitive information */
	memset(config->database_password, 0, sizeof(config->database_password));
}


void ramd_config_print(const ramd_config_t* config)
{
	if (!config)
		return;

	ramd_log_info("Configuration:");
	ramd_log_info("  node_id: %d", config->node_id);
	ramd_log_info("  hostname: %s", config->hostname);
	ramd_log_info("  postgresql_port: %d", config->postgresql_port);
	ramd_log_info("  cluster_name: %s", config->cluster_name);
	ramd_log_info("  cluster_size: %d", config->cluster_size);
	ramd_log_info("  auto_failover_enabled: %s",
	              config->auto_failover_enabled ? "true" : "false");
	ramd_log_info("  monitor_interval_ms: %d", config->monitor_interval_ms);
	ramd_log_info("  postgresql_data_dir: %s", config->postgresql_data_dir);
}


bool ramd_config_save_to_file(const char* config_file,
                              const ramd_config_t* config)
{
	FILE* f;

	if (!config_file || !config)
	{
		ramd_log_error("Invalid parameters for ramd_config_save_to_file");
		return false;
	}

	f = fopen(config_file, "w");
	if (!f)
	{
		ramd_log_error("Failed to open configuration file for writing: %s",
		               config_file);
		return false;
	}

	/* Write configuration in structured format */
	fprintf(f, "# RAMD Configuration File\n");
	fprintf(f, "# Generated automatically - edit with care\n\n");

	fprintf(f, "[daemon]\n");
	fprintf(f, "node_id = %d\n", config->node_id);
	fprintf(f, "hostname = %s\n", config->hostname);
	fprintf(f, "cluster_name = %s\n", config->cluster_name);
	fprintf(f, "cluster_size = %d\n", config->cluster_size);
	fprintf(f, "\n");

	fprintf(f, "[postgresql]\n");
	fprintf(f, "postgresql_port = %d\n", config->postgresql_port);
	fprintf(f, "postgresql_data_dir = %s\n", config->postgresql_data_dir);
	fprintf(f, "\n");

	fprintf(f, "[monitoring]\n");
	fprintf(f, "monitor_interval_ms = %d\n", config->monitor_interval_ms);
	fprintf(f, "auto_failover_enabled = %s\n",
	        config->auto_failover_enabled ? "true" : "false");
	fprintf(f, "\n");

	fclose(f);

	ramd_log_info("Configuration saved to: %s", config_file);
	return true;
}


void ramd_config_load_from_environment(ramd_config_t* config)
{
	const char* env_value;

	if (!config)
		return;

	/* Load configuration from environment variables */
	if ((env_value = getenv("RAMD_NODE_ID")) != NULL)
	{
		config->node_id = atoi(env_value);
		ramd_log_debug("Set node_id from environment: %d", config->node_id);
	}

	if ((env_value = getenv("RAMD_CLUSTER_NAME")) != NULL)
	{
		strncpy(config->cluster_name, env_value,
		        sizeof(config->cluster_name) - 1);
		ramd_log_debug("Set cluster_name from environment: %s",
		               config->cluster_name);
	}

	if ((env_value = getenv("RAMD_PG_PORT")) != NULL)
	{
		config->postgresql_port = atoi(env_value);
		ramd_log_debug("Set postgresql_port from environment: %d",
		               config->postgresql_port);
	}

	if ((env_value = getenv("RAMD_PG_DATA_DIR")) != NULL)
	{
		strncpy(config->postgresql_data_dir, env_value,
		        sizeof(config->postgresql_data_dir) - 1);
		ramd_log_debug("Set postgresql_data_dir from environment: %s",
		               config->postgresql_data_dir);
	}

	ramd_log_debug("Environment configuration loaded");
}
