/*-------------------------------------------------------------------------
 *
 * ramd_postgresql.h
 *		PostgreSQL Auto-Failover Daemon - PostgreSQL Operations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_POSTGRESQL_H
#define RAMD_POSTGRESQL_H

#include "ramd.h"
#include "ramd_config.h"
#include "ramd_cluster.h"

/* PostgreSQL connection information */
typedef struct ramd_postgresql_connection_t
{
	char host[RAMD_MAX_HOSTNAME_LENGTH];
	int32_t port;
	char database[RAMD_MAX_HOSTNAME_LENGTH];
	char user[RAMD_MAX_HOSTNAME_LENGTH];
	char password[RAMD_MAX_HOSTNAME_LENGTH];
	void* connection; /* PGconn * */
	bool is_connected;
	time_t last_activity;
} ramd_postgresql_connection_t;

/* PostgreSQL status information */
typedef struct ramd_postgresql_status_t
{
	bool is_running;
	bool is_in_recovery;
	bool is_primary;
	bool accepts_connections;
	int64_t current_wal_lsn;
	int64_t received_wal_lsn;
	int64_t replayed_wal_lsn;
	int32_t active_connections;
	int32_t max_connections;
	time_t postmaster_start_time;
	float replication_lag_seconds;
	time_t last_check;
} ramd_postgresql_status_t;

/* PostgreSQL connection management */
bool ramd_postgresql_connect(ramd_postgresql_connection_t* conn,
                             const char* host, int32_t port,
                             const char* database, const char* user,
                             const char* password);
void ramd_postgresql_disconnect(ramd_postgresql_connection_t* conn);
bool ramd_postgresql_is_connected(const ramd_postgresql_connection_t* conn);
bool ramd_postgresql_reconnect(ramd_postgresql_connection_t* conn);

/* PostgreSQL status queries */
bool ramd_postgresql_get_status(ramd_postgresql_connection_t* conn,
                                ramd_postgresql_status_t* status);
bool ramd_postgresql_is_running(const ramd_config_t* config);
bool ramd_postgresql_is_primary(ramd_postgresql_connection_t* conn);
bool ramd_postgresql_is_standby(ramd_postgresql_connection_t* conn);
bool ramd_postgresql_accepts_connections(ramd_postgresql_connection_t* conn);

/* PostgreSQL control operations */
bool ramd_postgresql_start(const ramd_config_t* config);
bool ramd_postgresql_stop(const ramd_config_t* config);
bool ramd_postgresql_restart(const ramd_config_t* config);
bool ramd_postgresql_reload(const ramd_config_t* config);

/* PostgreSQL promotion and demotion */
bool ramd_postgresql_promote(const ramd_config_t* config);
bool ramd_postgresql_demote_to_standby(const ramd_config_t* config,
                                       const char* primary_host,
                                       int32_t primary_port);

/* PostgreSQL replication management */
bool ramd_postgresql_setup_replication(const ramd_config_t* config,
                                       const char* primary_host,
                                       int32_t primary_port);
bool ramd_postgresql_create_recovery_conf(const ramd_config_t* config,
                                          const char* primary_host,
                                          int32_t primary_port);
bool ramd_postgresql_remove_recovery_conf(const ramd_config_t* config);

/* PostgreSQL basebackup operations */
bool ramd_postgresql_create_basebackup(const ramd_config_t* config,
                                       const char* primary_host,
                                       int32_t primary_port);
bool ramd_postgresql_validate_data_directory(const ramd_config_t* config);

/* PostgreSQL configuration management */
bool ramd_postgresql_update_config(const ramd_config_t* config,
                                   const char* parameter, const char* value);
bool ramd_postgresql_enable_archiving(const ramd_config_t* config);
bool ramd_postgresql_configure_synchronous_replication(
    const ramd_config_t* config, const char* standby_names);

/* PostgreSQL health checks */
bool ramd_postgresql_health_check(ramd_postgresql_connection_t* conn,
                                  float* health_score);
bool ramd_postgresql_check_connectivity(const ramd_config_t* config);
bool ramd_postgresql_check_replication_lag(ramd_postgresql_connection_t* conn,
                                           float* lag_seconds);

/* PostgreSQL extension management */
bool ramd_postgresql_create_pgraft_extension(const ramd_config_t* config);

/* pgraft integration functions */
bool ramd_postgresql_query_pgraft_cluster_status(const ramd_config_t* config,
                                                int32_t* node_count,
                                                bool* is_leader,
                                                int32_t* leader_id,
                                                bool* has_quorum);
bool ramd_postgresql_query_pgraft_is_leader(const ramd_config_t* config);
int32_t ramd_postgresql_query_pgraft_node_count(const ramd_config_t* config);
bool ramd_postgresql_query_pgraft_has_quorum(const ramd_config_t* config);

/* Utility functions */
bool ramd_postgresql_execute_query(ramd_postgresql_connection_t* conn,
                                   const char* query, char* result,
                                   size_t result_size);
bool ramd_postgresql_wait_for_startup(const ramd_config_t* config,
                                      int32_t timeout_seconds);
bool ramd_postgresql_wait_for_shutdown(const ramd_config_t* config,
                                       int32_t timeout_seconds);

#endif /* RAMD_POSTGRESQL_H */
