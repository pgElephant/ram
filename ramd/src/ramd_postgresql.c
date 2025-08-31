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
#include <libpq-fe.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

bool
ramd_postgresql_connect(ramd_postgresql_connection_t *conn,
                       const char *host, int32_t port,
                       const char *database, const char *user,
                       const char *password)
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
    snprintf(conninfo, sizeof(conninfo), 
             "host=%s port=%d dbname=%s user=%s%s%s connect_timeout=10",
             host, port, database, user,
             password ? " password=" : "",
             password ? password : "");
    
    /* Connect to PostgreSQL */
    conn->connection = PQconnectdb(conninfo);
    
    if (PQstatus((PGconn*)conn->connection) != CONNECTION_OK)
    {
        ramd_log_error("PostgreSQL connection failed: %s", 
                      PQerrorMessage((PGconn*)conn->connection));
        PQfinish((PGconn*)conn->connection);
        conn->connection = NULL;
        conn->is_connected = false;
        return false;
    }
    
    conn->is_connected = true;
    conn->last_activity = time(NULL);
    
    ramd_log_info("Connected to PostgreSQL: %s:%d/%s", host, port, database);
    return true;
}

void
ramd_postgresql_disconnect(ramd_postgresql_connection_t *conn)
{
    if (!conn)
        return;
        
    if (conn->connection)
    {
        PQfinish((PGconn*)conn->connection);
        conn->connection = NULL;
    }
    
    conn->is_connected = false;
    
    ramd_log_debug("Disconnected from PostgreSQL: %s:%d", conn->host, conn->port);
}

bool
ramd_postgresql_is_running(const ramd_config_t *config)
{
    char command[512];
    int status;
    
    if (!config)
        return false;
    
    /* Use pg_ctl status to check if PostgreSQL is running */
    snprintf(command, sizeof(command), 
             "%s/pg_ctl status -D %s >/dev/null 2>&1",
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

bool
ramd_postgresql_start(const ramd_config_t *config)
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
        ramd_log_info("PostgreSQL started successfully on node %d", config->node_id);
        return true;
    }
    
    ramd_log_error("Failed to start PostgreSQL on node %d", config->node_id);
    return false;
}

bool
ramd_postgresql_stop(const ramd_config_t *config)
{
    char command[512];
    int status;
    
    if (!config)
        return false;
        
    ramd_log_info("Stopping PostgreSQL on node %d", config->node_id);
    
    /* Execute pg_ctl stop command with fast mode */
    snprintf(command, sizeof(command), 
             "%s/pg_ctl stop -D %s -m fast -w -t 60",
             config->postgresql_bin_dir, config->postgresql_data_dir);
    
    status = system(command);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        ramd_log_info("PostgreSQL stopped successfully on node %d", config->node_id);
        return true;
    }
    
    ramd_log_error("Failed to stop PostgreSQL on node %d", config->node_id);
    return false;
}

bool
ramd_postgresql_promote(const ramd_config_t *config)
{
    char command[512];
    int status;
    
    if (!config)
        return false;
        
    ramd_log_info("Promoting PostgreSQL to primary on node %d", config->node_id);
    
    /* Execute pg_ctl promote command */
    snprintf(command, sizeof(command), 
             "%s/pg_ctl promote -D %s -w -t 60",
             config->postgresql_bin_dir, config->postgresql_data_dir);
    
    status = system(command);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        ramd_log_info("PostgreSQL promoted to primary successfully on node %d", config->node_id);
        return true;
    }
    
    ramd_log_error("Failed to promote PostgreSQL on node %d", config->node_id);
    return false;
}

bool
ramd_postgresql_create_basebackup(const ramd_config_t *config,
                                 const char *primary_host,
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
    
    ramd_log_error("Failed to create base backup from %s:%d", 
                  primary_host, primary_port);
    return false;
}

/* Additional PostgreSQL utility functions */
bool 
ramd_postgresql_is_connected(const ramd_postgresql_connection_t *conn) 
{ 
    return conn && conn->is_connected && conn->connection != NULL; 
}

bool 
ramd_postgresql_reconnect(ramd_postgresql_connection_t *conn) 
{
    if (!conn)
        return false;
        
    /* Disconnect first if connected */
    if (conn->is_connected)
        ramd_postgresql_disconnect(conn);
        
    /* Reconnect using stored connection info */
    return ramd_postgresql_connect(conn, conn->host, conn->port, 
                                  conn->database, conn->user, conn->password);
}

bool 
ramd_postgresql_get_status(ramd_postgresql_connection_t *conn, ramd_postgresql_status_t *status) 
{
    PGresult *res;
    
    if (!conn || !status || !conn->is_connected)
        return false;
        
    /* Query PostgreSQL for recovery status */
    res = PQexec((PGconn*)conn->connection, "SELECT pg_is_in_recovery()");
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        return false;
    }
    
    status->is_in_recovery = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
    status->last_check = time(NULL);
    
    PQclear(res);
    return true;
}

bool 
ramd_postgresql_is_primary(ramd_postgresql_connection_t *conn) 
{
    ramd_postgresql_status_t status;
    
    if (!ramd_postgresql_get_status(conn, &status))
        return false;
        
    return !status.is_in_recovery;
}

bool 
ramd_postgresql_is_standby(ramd_postgresql_connection_t *conn) 
{
    ramd_postgresql_status_t status;
    
    if (!ramd_postgresql_get_status(conn, &status))
        return false;
        
    return status.is_in_recovery;
}

bool 
ramd_postgresql_accepts_connections(ramd_postgresql_connection_t *conn) 
{
    PGresult *res;
    
    if (!conn || !conn->is_connected)
        return false;
        
    /* Test connection with a simple query */
    res = PQexec((PGconn*)conn->connection, "SELECT 1");
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool 
ramd_postgresql_restart(const ramd_config_t *config) 
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

bool 
ramd_postgresql_reload(const ramd_config_t *config) 
{
    char command[512];
    int status;
    
    if (!config)
        return false;
        
    ramd_log_info("Reloading PostgreSQL configuration on node %d", config->node_id);
    
    /* Execute pg_ctl reload command */
    snprintf(command, sizeof(command), 
             "%s/pg_ctl reload -D %s",
             config->postgresql_bin_dir, config->postgresql_data_dir);
    
    status = system(command);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        ramd_log_info("PostgreSQL configuration reloaded successfully on node %d", config->node_id);
        return true;
    }
    
    ramd_log_error("Failed to reload PostgreSQL configuration on node %d", config->node_id);
    return false;
}

bool 
ramd_postgresql_demote_to_standby(const ramd_config_t *config, const char *primary_host, int32_t primary_port) 
{
    if (!config || !primary_host)
        return false;
        
    ramd_log_info("Demoting PostgreSQL to standby, will follow %s:%d", primary_host, primary_port);
    
    /* Stop PostgreSQL */
    if (!ramd_postgresql_stop(config))
        return false;
        
    /* Create recovery configuration */
    if (!ramd_postgresql_create_recovery_conf(config, primary_host, primary_port))
        return false;
        
    /* Start as standby */
    return ramd_postgresql_start(config);
}

bool 
ramd_postgresql_setup_replication(const ramd_config_t *config, const char *primary_host, int32_t primary_port) 
{
    if (!config || !primary_host)
        return false;
        
    ramd_log_info("Setting up replication from %s:%d", primary_host, primary_port);
    
    /* Create base backup from primary */
    if (!ramd_postgresql_create_basebackup(config, primary_host, primary_port))
        return false;
        
    /* Create recovery configuration */
    return ramd_postgresql_create_recovery_conf(config, primary_host, primary_port);
}

bool 
ramd_postgresql_create_recovery_conf(const ramd_config_t *config, const char *primary_host, int32_t primary_port) 
{
    char recovery_conf_path[512];
    FILE *fp;
    
    if (!config || !primary_host)
        return false;
        
    snprintf(recovery_conf_path, sizeof(recovery_conf_path), 
             "%s/recovery.conf", config->postgresql_data_dir);
    
    fp = fopen(recovery_conf_path, "w");
    if (!fp)
    {
        ramd_log_error("Failed to create recovery.conf: %s", strerror(errno));
        return false;
    }
    
    fprintf(fp, "standby_mode = 'on'\n");
    fprintf(fp, "primary_conninfo = 'host=%s port=%d user=%s'\n", 
            primary_host, primary_port, config->replication_user);
    fprintf(fp, "recovery_target_timeline = 'latest'\n");
    
    fclose(fp);
    
    ramd_log_info("Created recovery.conf for replication from %s:%d", primary_host, primary_port);
    return true;
}

bool 
ramd_postgresql_remove_recovery_conf(const ramd_config_t *config) 
{
    char recovery_conf_path[512];
    
    if (!config)
        return false;
        
    snprintf(recovery_conf_path, sizeof(recovery_conf_path), 
             "%s/recovery.conf", config->postgresql_data_dir);
    
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

bool 
ramd_postgresql_validate_data_directory(const ramd_config_t *config) 
{
    char pg_version_path[512];
    struct stat st;
    
    if (!config)
        return false;
        
    /* Check if data directory exists */
    if (stat(config->postgresql_data_dir, &st) != 0)
    {
        ramd_log_error("PostgreSQL data directory does not exist: %s", config->postgresql_data_dir);
        return false;
    }
    
    if (!S_ISDIR(st.st_mode))
    {
        ramd_log_error("PostgreSQL data directory is not a directory: %s", config->postgresql_data_dir);
        return false;
    }
    
    /* Check for PG_VERSION file */
    snprintf(pg_version_path, sizeof(pg_version_path), 
             "%s/PG_VERSION", config->postgresql_data_dir);
    
    if (stat(pg_version_path, &st) != 0)
    {
        ramd_log_error("PostgreSQL data directory is not initialized: %s", config->postgresql_data_dir);
        return false;
    }
    
    ramd_log_debug("PostgreSQL data directory validated: %s", config->postgresql_data_dir);
    return true;
}

bool 
ramd_postgresql_update_config(const ramd_config_t *config, const char *parameter, const char *value) 
{
    char postgresql_conf_path[512];
    char command[1024];
    int status;
    
    if (!config || !parameter || !value)
        return false;
        
    snprintf(postgresql_conf_path, sizeof(postgresql_conf_path), 
             "%s/postgresql.conf", config->postgresql_data_dir);
    
    /* Use sed to update the configuration parameter */
    snprintf(command, sizeof(command), 
             "sed -i.bak 's/^#\\?%s.*$/%s = %s/' %s",
             parameter, parameter, value, postgresql_conf_path);
    
    status = system(command);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        ramd_log_info("Updated PostgreSQL configuration: %s = %s", parameter, value);
        return true;
    }
    
    ramd_log_error("Failed to update PostgreSQL configuration");
    return false;
}

bool 
ramd_postgresql_enable_archiving(const ramd_config_t *config) 
{
    if (!config)
        return false;
        
    /* Enable WAL archiving */
    if (!ramd_postgresql_update_config(config, "archive_mode", "on"))
        return false;
        
    if (!ramd_postgresql_update_config(config, "archive_command", "'cp %p /var/lib/postgresql/archive/%f'"))
        return false;
        
    ramd_log_info("Enabled WAL archiving on node %d", config->node_id);
    return true;
}

bool 
ramd_postgresql_configure_synchronous_replication(const ramd_config_t *config, const char *standby_names) 
{
    char sync_names[512];
    
    if (!config || !standby_names)
        return false;
        
    snprintf(sync_names, sizeof(sync_names), "'%s'", standby_names);
    
    if (!ramd_postgresql_update_config(config, "synchronous_standby_names", sync_names))
        return false;
        
    ramd_log_info("Configured synchronous replication with standbys: %s", standby_names);
    return true;
}

bool 
ramd_postgresql_health_check(ramd_postgresql_connection_t *conn, float *health_score) 
{
    PGresult *res;
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
    res = PQexec((PGconn*)conn->connection, "SELECT pg_is_in_recovery()");
    if (PQresultStatus(res) == PGRES_TUPLES_OK)
    {
        if (strcmp(PQgetvalue(res, 0, 0), "f") == 0)
            score += 30.0f; /* Primary database bonus */
        else
            score += 20.0f; /* Standby database */
    }
    PQclear(res);
    
    /* Additional health metrics could be added here */
    score += 20.0f; /* Placeholder for additional checks */
    
    *health_score = score;
    return true;
}

bool 
ramd_postgresql_check_connectivity(const ramd_config_t *config) 
{
    ramd_postgresql_connection_t conn;
    bool result;
    
    if (!config)
        return false;
        
    /* Try to connect to PostgreSQL */
    result = ramd_postgresql_connect(&conn, "localhost", config->postgresql_port,
                                   "postgres", config->postgresql_user, NULL);
    
    if (result)
        ramd_postgresql_disconnect(&conn);
        
    return result;
}

bool 
ramd_postgresql_check_replication_lag(ramd_postgresql_connection_t *conn, float *lag_seconds) 
{
    PGresult *res;
    const char *lag_str;
    
    if (!conn || !lag_seconds || !conn->is_connected)
        return false;
        
    /* Query replication lag */
    res = PQexec((PGconn*)conn->connection, 
                 "SELECT EXTRACT(EPOCH FROM (now() - pg_last_xact_replay_timestamp()))");
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        return false;
    }
    
    lag_str = PQgetvalue(res, 0, 0);
    if (lag_str && strlen(lag_str) > 0)
        *lag_seconds = (float)atof(lag_str);
    else
        *lag_seconds = 0.0f;
        
    PQclear(res);
    return true;
}

bool 
ramd_postgresql_execute_query(ramd_postgresql_connection_t *conn, const char *query, char *result, size_t result_size) 
{
    PGresult *res;
    const char *value;
    
    if (!conn || !query || !result || result_size == 0)
        return false;
        
    if (!conn->is_connected)
        return false;
        
    res = PQexec((PGconn*)conn->connection, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
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

bool 
ramd_postgresql_wait_for_startup(const ramd_config_t *config, int32_t timeout_seconds) 
{
    int elapsed = 0;
    
    if (!config)
        return false;
        
    ramd_log_info("Waiting for PostgreSQL startup (timeout: %d seconds)", timeout_seconds);
    
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
    
    ramd_log_error("PostgreSQL startup timeout after %d seconds", timeout_seconds);
    return false;
}

bool 
ramd_postgresql_wait_for_shutdown(const ramd_config_t *config, int32_t timeout_seconds) 
{
    int elapsed = 0;
    
    if (!config)
        return false;
        
    ramd_log_info("Waiting for PostgreSQL shutdown (timeout: %d seconds)", timeout_seconds);
    
    while (elapsed < timeout_seconds)
    {
        if (!ramd_postgresql_is_running(config))
        {
            ramd_log_info("PostgreSQL shutdown completed after %d seconds", elapsed);
            return true;
        }
        
        sleep(1);
        elapsed++;
    }
    
    ramd_log_error("PostgreSQL shutdown timeout after %d seconds", timeout_seconds);
    return false;
}
