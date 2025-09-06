/*-------------------------------------------------------------------------
 *
 * ramd_postgresql.c
 *		PostgreSQL Auto-Failover Daemon - PostgreSQL Operations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ramd_postgresql.h"
#include "ramd_logging.h"
#include "ramd_defaults.h"
#include <libpq-fe.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

bool ramd_postgresql_connect(ramd_postgresql_connection_t* conn,
                             const char* host, int32_t port,
                             const char* database, const char* user,
                             const char* password)
{
	char conninfo[1024];

	if (!conn || !host || !database || !user)
		return false;

	strncpy(conn->host, host, sizeof(conn->host) - 1);
	conn->port = port;
	strncpy(conn->database, database, sizeof(conn->database) - 1);
	strncpy(conn->user, user, sizeof(conn->user) - 1);
	if (password)
		strncpy(conn->password, password, sizeof(conn->password) - 1);

	/* Build connection string */
	if (password && strlen(password) > 0)
	{
		snprintf(
		    conninfo, sizeof(conninfo),
		    "host=%s port=%d dbname=%s user=%s password=%s connect_timeout=10",
		    host, port, database, user, password);
	}
	else
	{
		snprintf(conninfo, sizeof(conninfo),
		         "host=%s port=%d dbname=%s user=%s connect_timeout=10", host,
		         port, database, user);
	}

	ramd_log_debug("PostgreSQL connection string: %s", conninfo);
	conn->connection = PQconnectdb(conninfo);

	if (PQstatus((PGconn*) conn->connection) != CONNECTION_OK)
	{
		ramd_log_error("PostgreSQL connection failed: %s",
		               PQerrorMessage((PGconn*) conn->connection));
		PQfinish((PGconn*) conn->connection);
		conn->connection = NULL;
		conn->is_connected = false;
		return false;
	}

	conn->is_connected = true;
	conn->last_activity = time(NULL);

	ramd_log_info("Connected to PostgreSQL: %s:%d/%s", host, port, database);
	return true;
}


void ramd_postgresql_disconnect(ramd_postgresql_connection_t* conn)
{
	if (!conn)
		return;

	if (conn->connection)
	{
		PQfinish((PGconn*) conn->connection);
		conn->connection = NULL;
	}

	conn->is_connected = false;

	ramd_log_debug("Disconnected from PostgreSQL: %s:%d", conn->host,
	               conn->port);
}


bool ramd_postgresql_is_running(const ramd_config_t* config)
{
	char command[512];
	int status;

	if (!config)
		return false;

	snprintf(command, sizeof(command), "%s/pg_ctl status -D %s >/dev/null 2>&1",
	         config->postgresql_bin_dir, config->postgresql_data_dir);

	status = system(command);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		ramd_log_debug("PostgreSQL is running on node %d", config->node_id);
		return true;
	}

	ramd_log_debug("PostgreSQL is not running on node %d", config->node_id);
	return false;
}


bool ramd_postgresql_start(const ramd_config_t* config)
{
	char command[512];
	int status;

	if (!config)
		return false;

	ramd_log_info("Starting PostgreSQL on node %d", config->node_id);

	/* Execute pg_ctl start command */
	snprintf(command, sizeof(command),
	         "%s/pg_ctl start -D %s -l %s/postgresql.log -w -t 60",
	         config->postgresql_bin_dir, config->postgresql_data_dir,
	         config->postgresql_data_dir);

	status = system(command);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		ramd_log_info("PostgreSQL started successfully on node %d",
		              config->node_id);
		return true;
	}

	ramd_log_error("Failed to start PostgreSQL on node %d", config->node_id);
	return false;
}


bool ramd_postgresql_stop(const ramd_config_t* config)
{
	char command[512];
	int status;

	if (!config)
		return false;

	ramd_log_info("Stopping PostgreSQL on node %d", config->node_id);

	/* Execute pg_ctl stop command with fast mode */
	snprintf(command, sizeof(command), "%s/pg_ctl stop -D %s -m fast -w -t 60",
	         config->postgresql_bin_dir, config->postgresql_data_dir);

	status = system(command);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		ramd_log_info("PostgreSQL stopped successfully on node %d",
		              config->node_id);
		return true;
	}

	ramd_log_error("Failed to stop PostgreSQL on node %d", config->node_id);
	return false;
}


bool ramd_postgresql_promote(const ramd_config_t* config)
{
	char command[512];
	int status;

	if (!config)
		return false;

	ramd_log_info("Promoting PostgreSQL to primary on node %d",
	              config->node_id);

	/* Execute pg_ctl promote command */
	snprintf(command, sizeof(command), "%s/pg_ctl promote -D %s -w -t 60",
	         config->postgresql_bin_dir, config->postgresql_data_dir);

	status = system(command);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		ramd_log_info("PostgreSQL promoted to primary successfully on node %d",
		              config->node_id);
		return true;
	}

	ramd_log_error("Failed to promote PostgreSQL on node %d", config->node_id);
	return false;
}


bool ramd_postgresql_create_basebackup(const ramd_config_t* config,
                                       const char* primary_host,
                                       int32_t primary_port)
{
	char command[1024];
	int status;

	if (!config || !primary_host)
		return false;

	ramd_log_info("Taking base backup from %s:%d", primary_host, primary_port);

	/* Execute pg_basebackup command */
	snprintf(command, sizeof(command),
	         "%s/pg_basebackup -h %s -p %d -D %s -U %s -v -P -W -R",
	         config->postgresql_bin_dir, primary_host, primary_port,
	         config->postgresql_data_dir, config->replication_user);

	status = system(command);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		ramd_log_info("Base backup completed successfully from %s:%d",
		              primary_host, primary_port);
		return true;
	}

	ramd_log_error("Failed to create base backup from %s:%d", primary_host,
	               primary_port);
	return false;
}


/* Additional PostgreSQL utility functions */
bool ramd_postgresql_is_connected(const ramd_postgresql_connection_t* conn)
{
	return conn && conn->is_connected && conn->connection != NULL;
}


bool ramd_postgresql_reconnect(ramd_postgresql_connection_t* conn)
{
	if (!conn)
		return false;

	/* Disconnect first if connected */
	if (conn->is_connected)
		ramd_postgresql_disconnect(conn);

	/* Reconnect using stored connection info */
	return ramd_postgresql_connect(conn, conn->host, conn->port, conn->database,
	                               conn->user, conn->password);
}


bool ramd_postgresql_get_status(ramd_postgresql_connection_t* conn,
                                ramd_postgresql_status_t* status)
{
	PGresult* res;

	if (!conn || !status || !conn->is_connected)
		return false;

	/* Query PostgreSQL for recovery status */
	res = PQexec((PGconn*) conn->connection, "SELECT pg_is_in_recovery()");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		PQclear(res);
		return false;
	}

	status->is_in_recovery = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
	status->is_running =
	    true; /* If we can execute queries, PostgreSQL is running */
	status->is_primary = !status->is_in_recovery;
	status->accepts_connections =
	    true; /* If we connected and queried, it accepts connections */
	status->last_check = time(NULL);

	PQclear(res);
	return true;
}


bool ramd_postgresql_is_primary(ramd_postgresql_connection_t* conn)
{
	ramd_postgresql_status_t status;

	if (!ramd_postgresql_get_status(conn, &status))
		return false;

	return !status.is_in_recovery;
}


bool ramd_postgresql_is_standby(ramd_postgresql_connection_t* conn)
{
	ramd_postgresql_status_t status;

	if (!ramd_postgresql_get_status(conn, &status))
		return false;

	return status.is_in_recovery;
}


bool ramd_postgresql_accepts_connections(ramd_postgresql_connection_t* conn)
{
	PGresult* res;

	if (!conn || !conn->is_connected)
		return false;

	/* Test connection with a simple query */
	res = PQexec((PGconn*) conn->connection, "SELECT 1");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		PQclear(res);
		return false;
	}

	PQclear(res);
	return true;
}


bool ramd_postgresql_restart(const ramd_config_t* config)
{
	if (!config)
		return false;

	ramd_log_info("Restarting PostgreSQL on node %d", config->node_id);

	/* Stop PostgreSQL first */
	if (!ramd_postgresql_stop(config))
		return false;

	/* Wait a moment */
	sleep(2);

	/* Start PostgreSQL */
	return ramd_postgresql_start(config);
}


bool ramd_postgresql_reload(const ramd_config_t* config)
{
	char command[512];
	int status;

	if (!config)
		return false;

	ramd_log_info("Reloading PostgreSQL configuration on node %d",
	              config->node_id);

	/* Execute pg_ctl reload command */
	snprintf(command, sizeof(command), "%s/pg_ctl reload -D %s",
	         config->postgresql_bin_dir, config->postgresql_data_dir);

	status = system(command);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		ramd_log_info(
		    "PostgreSQL configuration reloaded successfully on node %d",
		    config->node_id);
		return true;
	}

	ramd_log_error("Failed to reload PostgreSQL configuration on node %d",
	               config->node_id);
	return false;
}


bool ramd_postgresql_demote_to_standby(const ramd_config_t* config,
                                       const char* primary_host,
                                       int32_t primary_port)
{
	if (!config || !primary_host)
		return false;

	ramd_log_info("Demoting PostgreSQL to standby, will follow %s:%d",
	              primary_host, primary_port);

	/* Stop PostgreSQL */
	if (!ramd_postgresql_stop(config))
		return false;

	/* Create recovery configuration */
	if (!ramd_postgresql_create_recovery_conf(config, primary_host,
	                                          primary_port))
		return false;

	/* Start as standby */
	return ramd_postgresql_start(config);
}


bool ramd_postgresql_setup_replication(const ramd_config_t* config,
                                       const char* primary_host,
                                       int32_t primary_port)
{
	if (!config || !primary_host)
		return false;

	ramd_log_info("Setting up replication from %s:%d", primary_host,
	              primary_port);

	/* Create base backup from primary */
	if (!ramd_postgresql_create_basebackup(config, primary_host, primary_port))
		return false;

	/* Create recovery configuration */
	return ramd_postgresql_create_recovery_conf(config, primary_host,
	                                            primary_port);
}


bool ramd_postgresql_create_recovery_conf(const ramd_config_t* config,
                                          const char* primary_host,
                                          int32_t primary_port)
{
	char recovery_conf_path[512];
	FILE* fp;

	if (!config || !primary_host)
		return false;

	snprintf(recovery_conf_path, sizeof(recovery_conf_path), "%s/recovery.conf",
	         config->postgresql_data_dir);

	fp = fopen(recovery_conf_path, "w");
	if (!fp)
	{
		ramd_log_error("Failed to create recovery.conf: %s", strerror(errno));
		return false;
	}

	fprintf(fp, "standby_mode = 'on'\n");
	fprintf(fp, "primary_conninfo = 'host=%s port=%d user=%s'\n", primary_host,
	        primary_port, config->replication_user);
	fprintf(fp, "recovery_target_timeline = 'latest'\n");

	fclose(fp);

	ramd_log_info("Created recovery.conf for replication from %s:%d",
	              primary_host, primary_port);
	return true;
}


bool ramd_postgresql_remove_recovery_conf(const ramd_config_t* config)
{
	char recovery_conf_path[512];

	if (!config)
		return false;

	snprintf(recovery_conf_path, sizeof(recovery_conf_path), "%s/recovery.conf",
	         config->postgresql_data_dir);

	if (unlink(recovery_conf_path) == 0)
	{
		ramd_log_info("Removed recovery.conf from node %d", config->node_id);
		return true;
	}

	if (errno == ENOENT)
	{
		/* File doesn't exist, that's fine */
		return true;
	}

	ramd_log_error("Failed to remove recovery.conf: %s", strerror(errno));
	return false;
}


bool ramd_postgresql_validate_data_directory(const ramd_config_t* config)
{
	char pg_version_path[512];
	struct stat st;

	if (!config)
		return false;

	/* Check if data directory exists */
	if (stat(config->postgresql_data_dir, &st) != 0)
	{
		ramd_log_error("PostgreSQL data directory does not exist: %s",
		               config->postgresql_data_dir);
		return false;
	}

	if (!S_ISDIR(st.st_mode))
	{
		ramd_log_error("PostgreSQL data directory is not a directory: %s",
		               config->postgresql_data_dir);
		return false;
	}

	/* Check for PG_VERSION file */
	snprintf(pg_version_path, sizeof(pg_version_path), "%s/PG_VERSION",
	         config->postgresql_data_dir);

	if (stat(pg_version_path, &st) != 0)
	{
		ramd_log_error("PostgreSQL data directory is not initialized: %s",
		               config->postgresql_data_dir);
		return false;
	}

	ramd_log_debug("PostgreSQL data directory validated: %s",
	               config->postgresql_data_dir);
	return true;
}


bool ramd_postgresql_update_config(const ramd_config_t* config,
                                   const char* parameter, const char* value)
{
	char postgresql_conf_path[512];
	char command[1024];
	int status;

	if (!config || !parameter || !value)
		return false;

	snprintf(postgresql_conf_path, sizeof(postgresql_conf_path),
	         "%s/postgresql.conf", config->postgresql_data_dir);

	/* Use sed to update the configuration parameter */
	snprintf(command, sizeof(command), "sed -i.bak 's/^#\\?%s.*$/%s = %s/' %s",
	         parameter, parameter, value, postgresql_conf_path);

	status = system(command);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		ramd_log_info("Updated PostgreSQL configuration: %s = %s", parameter,
		              value);
		return true;
	}

	ramd_log_error("Failed to update PostgreSQL configuration");
	return false;
}


bool ramd_postgresql_enable_archiving(const ramd_config_t* config)
{
	if (!config)
		return false;

	/* Enable WAL archiving */
	if (!ramd_postgresql_update_config(config, "archive_mode", "on"))
		return false;

	/* Configure archive command using environment variable or default */
	char archive_command[512];
	const char* pgarchive = getenv("PGARCHIVE");
	if (pgarchive && strlen(pgarchive) > 0)
	{
		snprintf(archive_command, sizeof(archive_command), "'cp %%p %s/%%f'",
		         pgarchive);
	}
	else
	{
		snprintf(archive_command, sizeof(archive_command), "'cp %%p %s/%%f'",
		         RAMD_DEFAULT_PG_ARCHIVE_DIR);
	}

	if (!ramd_postgresql_update_config(config, "archive_command",
	                                   archive_command))
		return false;

	ramd_log_info("Enabled WAL archiving on node %d", config->node_id);
	return true;
}


bool ramd_postgresql_configure_synchronous_replication(
    const ramd_config_t* config, const char* standby_names)
{
	char sync_names[512];

	if (!config || !standby_names)
		return false;

	snprintf(sync_names, sizeof(sync_names), "'%s'", standby_names);

	if (!ramd_postgresql_update_config(config, "synchronous_standby_names",
	                                   sync_names))
		return false;

	ramd_log_info("Configured synchronous replication with standbys: %s",
	              standby_names);
	return true;
}


bool ramd_postgresql_health_check(ramd_postgresql_connection_t* conn,
                                  float* health_score)
{
	PGresult* res;
	float score = 0.0f;

	if (!conn || !health_score)
		return false;

	/* Basic connection check */
	if (!ramd_postgresql_accepts_connections(conn))
	{
		*health_score = 0.0f;
		return false;
	}

	score += 50.0f; /* Base score for connectivity */

	/* Check if database is accepting writes */
	res = PQexec((PGconn*) conn->connection, "SELECT pg_is_in_recovery()");
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (strcmp(PQgetvalue(res, 0, 0), "f") == 0)
			score += 30.0f; /* Primary database bonus */
		else
			score += 20.0f; /* Standby database */
	}
	PQclear(res);

	/* Additional health metrics could be added here */
	/* Additional health metrics */

	/* Check WAL writing capability */
	res = PQexec((PGconn*) conn->connection, "SELECT pg_current_wal_lsn()");
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		score += 15.0f; /* WAL writing works */
	}
	PQclear(res);

	/* Check vacuum activity */
	res = PQexec((PGconn*) conn->connection,
	             "SELECT count(*) FROM pg_stat_activity WHERE query LIKE "
	             "'%autovacuum%'");
	if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
	{
		int vacuum_count = atoi(PQgetvalue(res, 0, 0));
		if (vacuum_count < 5) /* Not too many vacuum processes */
			score += 5.0f;
	}
	PQclear(res);

	*health_score = score;
	return true;
}


bool ramd_postgresql_check_connectivity(const ramd_config_t* config)
{
	ramd_postgresql_connection_t conn;
	bool result;

	if (!config)
		return false;

	/* Try to connect to PostgreSQL */
	result = ramd_postgresql_connect(&conn, config->hostname,
	                                 config->postgresql_port, "postgres",
	                                 config->postgresql_user, NULL);

	if (result)
		ramd_postgresql_disconnect(&conn);

	return result;
}


bool ramd_postgresql_check_replication_lag(ramd_postgresql_connection_t* conn,
                                           float* lag_seconds)
{
	PGresult* res;
	const char* lag_str;

	if (!conn || !lag_seconds || !conn->is_connected)
		return false;

	/* Query replication lag */
	res = PQexec(
	    (PGconn*) conn->connection,
	    "SELECT EXTRACT(EPOCH FROM (now() - pg_last_xact_replay_timestamp()))");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		PQclear(res);
		return false;
	}

	lag_str = PQgetvalue(res, 0, 0);
	if (lag_str && strlen(lag_str) > 0)
		*lag_seconds = (float) atof(lag_str);
	else
		*lag_seconds = 0.0f;

	PQclear(res);
	return true;
}


bool ramd_postgresql_execute_query(ramd_postgresql_connection_t* conn,
                                   const char* query, char* result,
                                   size_t result_size)
{
	PGresult* res;
	const char* value;

	if (!conn || !query || !result || result_size == 0)
		return false;

	if (!conn->is_connected)
		return false;

	res = PQexec((PGconn*) conn->connection, query);

	/* Accept both PGRES_TUPLES_OK (for SELECT) and PGRES_COMMAND_OK (for DDL)
	 */
	if (PQresultStatus(res) != PGRES_TUPLES_OK &&
	    PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		strncpy(result, PQerrorMessage((PGconn*) conn->connection),
		        result_size - 1);
		result[result_size - 1] = '\0';
		PQclear(res);
		return false;
	}

	if (PQntuples(res) > 0 && PQnfields(res) > 0)
	{
		value = PQgetvalue(res, 0, 0);
		strncpy(result, value ? value : "", result_size - 1);
		result[result_size - 1] = '\0';
	}
	else
	{
		result[0] = '\0';
	}

	PQclear(res);
	return true;
}


bool ramd_postgresql_wait_for_startup(const ramd_config_t* config,
                                      int32_t timeout_seconds)
{
	int elapsed = 0;

	if (!config)
		return false;

	ramd_log_info("Waiting for PostgreSQL startup (timeout: %d seconds)",
	              timeout_seconds);

	while (elapsed < timeout_seconds)
	{
		if (ramd_postgresql_check_connectivity(config))
		{
			ramd_log_info("PostgreSQL is ready after %d seconds", elapsed);
			return true;
		}

		sleep(1);
		elapsed++;
	}

	ramd_log_error("PostgreSQL startup timeout after %d seconds",
	               timeout_seconds);
	return false;
}


bool ramd_postgresql_wait_for_shutdown(const ramd_config_t* config,
                                       int32_t timeout_seconds)
{
	int elapsed = 0;

	if (!config)
		return false;

	ramd_log_info("Waiting for PostgreSQL shutdown (timeout: %d seconds)",
	              timeout_seconds);

	while (elapsed < timeout_seconds)
	{
		if (!ramd_postgresql_is_running(config))
		{
			ramd_log_info("PostgreSQL shutdown completed after %d seconds",
			              elapsed);
			return true;
		}

		sleep(1);
		elapsed++;
	}

	ramd_log_error("PostgreSQL shutdown timeout after %d seconds",
	               timeout_seconds);
	return false;
}


const char* ramd_postgresql_get_data_directory(void)
{
	/* Get actual data directory from configuration or detect installation */

	static char data_dir[512];

	/* Use configured data directory or environment variable */
	char* env_pgdata = getenv("PGDATA");
	if (env_pgdata && strlen(env_pgdata) > 0)
	{
		strcpy(data_dir, env_pgdata);
	}
	else if (access(RAMD_DEFAULT_PG_DATA_DIR, F_OK) == 0)
	{
		strcpy(data_dir, RAMD_DEFAULT_PG_DATA_DIR);
	}
	else
	{
		/* Final fallback */
		strcpy(data_dir, RAMD_FALLBACK_DATA_DIR);
	}

	ramd_log_debug("Using PostgreSQL data directory: %s", data_dir);

	return data_dir;
}


bool ramd_postgresql_update_recovery_conf(const char* path,
                                          const char* conninfo,
                                          const char* slot)
{
	FILE* f;

	if (!path || !conninfo)
	{
		ramd_log_error(
		    "Invalid parameters for ramd_postgresql_update_recovery_conf");
		return false;
	}

	/* Create recovery configuration for standby setup */
	ramd_log_info("Creating recovery configuration at %s", path);

	f = fopen(path, "w");
	if (!f)
	{
		ramd_log_error("Failed to create recovery.conf: %s", strerror(errno));
		return false;
	}

	/* Write recovery configuration */
	fprintf(f, "# Generated by RAMD - PostgreSQL Auto-Failover Daemon\n\n");
	fprintf(f, "standby_mode = 'on'\n");
	fprintf(f, "primary_conninfo = '%s'\n", conninfo);

	if (slot && strlen(slot) > 0)
	{
		fprintf(f, "primary_slot_name = '%s'\n", slot);
	}

	/* Use trigger file in the same directory as recovery.conf */
	char trigger_path[512];
	snprintf(trigger_path, sizeof(trigger_path), "%s/../postgresql.trigger",
	         path);
	fprintf(f, "trigger_file = '%s'\n", trigger_path);
	/* Use configurable archive directory */
	char archive_dir[512];
	char* env_archive_dir = getenv("PGARCHIVE");
	if (env_archive_dir && strlen(env_archive_dir) > 0)
	{
		strcpy(archive_dir, env_archive_dir);
	}
	else
	{
		/* Extract directory from path parameter and use archive subdirectory */
		char base_dir[512];
		char* last_slash = strrchr(path, '/');
		if (last_slash)
		{
			size_t dir_len = last_slash - path;
			strncpy(base_dir, path, dir_len);
			base_dir[dir_len] = '\0';
			snprintf(archive_dir, sizeof(archive_dir), "%s/archive", base_dir);
		}
		else
		{
			strcpy(archive_dir, RAMD_FALLBACK_ARCHIVE_DIR);
		}
	}

	fprintf(f, "restore_command = 'cp %s/%%f %%p'\n", archive_dir);
	fprintf(f, "archive_cleanup_command = 'pg_archivecleanup %s %%r'\n",
	        archive_dir);

	fclose(f);

	ramd_log_info("Recovery configuration written successfully");
	return true;
}


float ramd_postgresql_get_health_score(const ramd_config_t* config)
{
	if (!config)
		return 0.0f;

	/* Calculate comprehensive health score based on multiple metrics */
	float health = 0.0f;

	/* Basic health checks */
	if (ramd_postgresql_is_running(config))
		health += 0.5f;

	if (ramd_postgresql_validate_data_directory(config))
		health += 0.3f;

	/* Add comprehensive health metrics including connection availability,
	 * replication lag, disk space, and performance */
	/* Additional comprehensive health checks via database connection */
	PGconn* conn = NULL;
	PGresult* res = NULL;
	char conninfo[512];

	/* Create connection for health checks */
	snprintf(conninfo, sizeof(conninfo), "host=%s port=%d dbname=%s user=%s",
	         config->hostname, config->postgresql_port, config->database_name,
	         config->database_user);

	conn = PQconnectdb(conninfo);
	if (PQstatus(conn) == CONNECTION_OK)
	{
		/* Check replication lag if this is a standby */
		res = PQexec(
		    conn, "SELECT CASE WHEN pg_is_in_recovery() THEN EXTRACT(EPOCH "
		          "FROM (now() - pg_last_xact_replay_timestamp())) ELSE 0 END");
		if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
		{
			float lag_seconds = (float) atof(PQgetvalue(res, 0, 0));
			if (lag_seconds < 60.0f) /* Less than 1 minute lag */
				health += 0.2f;
			else if (lag_seconds > 300.0f) /* More than 5 minutes lag */
				health -= 0.3f;
		}
		PQclear(res);

		/* Check for long-running transactions */
		res = PQexec(conn,
		             "SELECT count(*) FROM pg_stat_activity WHERE state = "
		             "'active' AND query_start < now() - interval '1 hour'");
		if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
		{
			int long_queries = atoi(PQgetvalue(res, 0, 0));
			if (long_queries == 0)
				health += 0.1f;
			else
				health -= (long_queries * 0.05f); /* Penalty for long queries */
		}
		PQclear(res);

		PQfinish(conn);
	}
	else
	{
		/* Connection failed, subtract from health score */
		health -= 0.2f;
		if (conn)
			PQfinish(conn);
	}

	ramd_log_debug("PostgreSQL health score: %.2f", health);

	return health;
}


bool ramd_postgresql_create_pg_ram_extension(const ramd_config_t* config)
{
	ramd_postgresql_connection_t conn;
	bool result = false;

	if (!config)
		return false;

	ramd_log_info("Creating pg_ram extension in PostgreSQL database");

	/* Connect to PostgreSQL */
	if (!ramd_postgresql_connect(&conn, config->hostname,
	                             config->postgresql_port, "postgres",
	                             config->postgresql_user, NULL))
	{
		ramd_log_error(
		    "Failed to connect to PostgreSQL to create pg_ram extension");
		return false;
	}

	/* Create the extension */
	char query_result[256];
	if (ramd_postgresql_execute_query(&conn,
	                                  "CREATE EXTENSION IF NOT EXISTS pg_ram;",
	                                  query_result, sizeof(query_result)))
	{
		ramd_log_info("pg_ram extension created successfully");
		result = true;
	}
	else
	{
		ramd_log_error("Failed to create pg_ram extension: %s", query_result);
	}

	ramd_postgresql_disconnect(&conn);
	return result;
}


bool ramd_postgresql_query_pgram_cluster_status(const ramd_config_t* config,
                                                int32_t* node_count,
                                                bool* is_leader,
                                                int32_t* leader_id,
                                                bool* has_quorum)
{
	ramd_postgresql_connection_t conn;
	bool result = false;
	PGresult* res;

	if (!config || !node_count || !is_leader || !leader_id || !has_quorum)
		return false;

	/* Connect to PostgreSQL */
	if (!ramd_postgresql_connect(&conn, config->hostname,
	                             config->postgresql_port, "postgres",
	                             config->postgresql_user, NULL))
	{
		ramd_log_error("Failed to connect to PostgreSQL for pg_ram query");
		return false;
	}

	/* Query pg_ram cluster status */
	res = PQexec((PGconn*) conn.connection,
	             "SELECT node_count, is_leader, leader_id, has_quorum FROM "
	             "pgram.cluster_status;");

	if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
	{
		*node_count = atoi(PQgetvalue(res, 0, 0));
		*is_leader = (PQgetvalue(res, 0, 1)[0] == 't');
		*leader_id = atoi(PQgetvalue(res, 0, 2));
		*has_quorum = (PQgetvalue(res, 0, 3)[0] == 't');
		result = true;
		ramd_log_debug("pg_ram cluster status: nodes=%d, leader=%s, "
		               "leader_id=%d, quorum=%s",
		               *node_count, *is_leader ? "true" : "false", *leader_id,
		               *has_quorum ? "true" : "false");
	}
	else
	{
		ramd_log_error("Failed to query pg_ram cluster status: %s",
		               PQerrorMessage((PGconn*) conn.connection));
	}

	PQclear(res);
	ramd_postgresql_disconnect(&conn);
	return result;
}


bool ramd_postgresql_query_pgram_is_leader(const ramd_config_t* config)
{
	int32_t node_count, leader_id;
	bool is_leader, has_quorum;

	if (ramd_postgresql_query_pgram_cluster_status(
	        config, &node_count, &is_leader, &leader_id, &has_quorum))
		return is_leader;

	return false;
}


int32_t ramd_postgresql_query_pgram_node_count(const ramd_config_t* config)
{
	int32_t node_count, leader_id;
	bool is_leader, has_quorum;

	if (ramd_postgresql_query_pgram_cluster_status(
	        config, &node_count, &is_leader, &leader_id, &has_quorum))
		return node_count;

	return 0;
}


bool ramd_postgresql_query_pgram_has_quorum(const ramd_config_t* config)
{
	int32_t node_count, leader_id;
	bool is_leader, has_quorum;

	if (ramd_postgresql_query_pgram_cluster_status(
	        config, &node_count, &is_leader, &leader_id, &has_quorum))
		return has_quorum;

	return false;
}
