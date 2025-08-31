/*-------------------------------------------------------------------------
 *
 * ramctrl_database.h
 *		PostgreSQL RAM Control Utility - Database Operations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_DATABASE_H
#define RAMCTRL_DATABASE_H

#include "ramctrl.h"

/* Database connection structure */
typedef struct ramctrl_database_connection
{
	char				host[RAMCTRL_MAX_HOSTNAME_LENGTH];
	int32_t				port;
	char				database[64];
	char				user[64];
	char				password[64];
	void			   *connection;	/* PGconn * */
	bool				is_connected;
	time_t				last_activity;
} ramctrl_database_connection_t;

/* Note: ramctrl_cluster_info_t and ramctrl_node_info_t are defined in ramctrl.h */

/* Database connection management */
extern bool ramctrl_database_connect(ramctrl_database_connection_t *conn,
									  const char *host, int32_t port,
									  const char *database, const char *user,
									  const char *password);
extern void ramctrl_database_disconnect(ramctrl_database_connection_t *conn);
extern bool ramctrl_database_is_connected(const ramctrl_database_connection_t *conn);
extern bool ramctrl_database_reconnect(ramctrl_database_connection_t *conn);

/* pg_ram extension queries */
extern bool ramctrl_database_get_cluster_info(ramctrl_database_connection_t *conn,
											   ramctrl_cluster_info_t *cluster_info);
extern bool ramctrl_database_get_node_info(ramctrl_database_connection_t *conn,
											int32_t node_id,
											ramctrl_node_info_t *node_info);
extern bool ramctrl_database_get_all_nodes(ramctrl_database_connection_t *conn,
											ramctrl_node_info_t nodes[],
											int32_t *node_count,
											int32_t max_nodes);

/* Control operations via pg_ram extension */
extern bool ramctrl_database_promote_node(ramctrl_database_connection_t *conn,
										   int32_t node_id);
extern bool ramctrl_database_demote_node(ramctrl_database_connection_t *conn,
										  int32_t node_id);
extern bool ramctrl_database_trigger_failover(ramctrl_database_connection_t *conn,
											   int32_t target_node_id);
extern bool ramctrl_database_add_node(ramctrl_database_connection_t *conn,
									   int32_t node_id, const char *hostname,
									   int32_t port);
extern bool ramctrl_database_remove_node(ramctrl_database_connection_t *conn,
										  int32_t node_id);
extern bool ramctrl_database_enable_maintenance(ramctrl_database_connection_t *conn,
												 int32_t node_id);
extern bool ramctrl_database_disable_maintenance(ramctrl_database_connection_t *conn,
												  int32_t node_id);

/* Monitoring and health checks */
extern bool ramctrl_database_check_extension_installed(ramctrl_database_connection_t *conn);
extern bool ramctrl_database_get_health_status(ramctrl_database_connection_t *conn,
												float *health_score);
extern bool ramctrl_database_get_monitoring_data(ramctrl_database_connection_t *conn,
												  char *result, size_t result_size);

/* Utility functions */
extern bool ramctrl_database_execute_query(ramctrl_database_connection_t *conn,
											const char *query,
											char *result, size_t result_size);
extern bool ramctrl_database_execute_function(ramctrl_database_connection_t *conn,
											   const char *function_name,
											   const char *args,
											   char *result, size_t result_size);

#endif							/* RAMCTRL_DATABASE_H */
