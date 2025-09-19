/*-------------------------------------------------------------------------
 *
 * ramd_failover.c
 *		PostgreSQL Auto-Failover Daemon - Enhanced Failover Logic
 *
 * Copyright (cc) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ramd_failover.h"
#include "ramd_logging.h"
#include "ramd_postgresql.h"
#include "ramd_cluster.h"
#include "ramd_sync_replication.h"
#include "ramd_basebackup.h"
#include "ramd_conn.h"

#include <libpq-fe.h>

void ramd_failover_context_init(ramd_failover_context_t* context)
{
	if (!context)
		return;

	memset(context, 0, sizeof(ramd_failover_context_t));
	context->state = RAMD_FAILOVER_STATE_NORMAL;
	context->failed_node_id = -1;
	context->new_primary_node_id = -1;
}


void ramd_failover_context_cleanup(ramd_failover_context_t* context)
{
	if (!context)
		return;

	memset(context, 0, sizeof(ramd_failover_context_t));
}


void ramd_failover_context_set_reason(ramd_failover_context_t* context,
                                      const char* reason)
{
	if (!context || !reason)
		return;

	strncpy(context->reason, reason, sizeof(context->reason) - 1);
}


bool ramd_failover_should_trigger(const ramd_cluster_t* cluster,
                                  const ramd_config_t* config)
{
	if (!cluster || !config)
		return false;

	if (!config->auto_failover_enabled)
		return false;

	/* Don't trigger failover on empty clusters */
	if (cluster->node_count == 0)
		return false;

	/* Don't trigger failover if we only have one node (single-node cluster) */
	if (cluster->node_count == 1)
		return false;

	/* Check if primary has failed and we have quorum */
	return ramd_failover_detect_primary_failure((ramd_cluster_t*) cluster) &&
	       ramd_cluster_has_quorum(cluster);
}


bool ramd_failover_execute(ramd_cluster_t* cluster, const ramd_config_t* config,
                           ramd_failover_context_t* context)
{
	if (!cluster || !config || !context)
		return false;

	ramd_log_warning("Initiating automated failover procedure for cluster '%s' "
	                 "due to primary node failure",
	                 cluster->cluster_name);

	context->state = RAMD_FAILOVER_STATE_DETECTING;
	context->started_at = time(NULL);

	/* Select new primary */
	if (!ramd_failover_select_new_primary(cluster,
	                                      &context->new_primary_node_id))
	{
		ramd_log_error("Critical error: Unable to identify suitable candidate "
		               "for primary node promotion");
		context->state = RAMD_FAILOVER_STATE_FAILED;
		return false;
	}

	/* Promote new primary */
	context->state = RAMD_FAILOVER_STATE_PROMOTING;
	if (!ramd_failover_promote_node(cluster, config,
	                                context->new_primary_node_id))
	{
		ramd_log_error("Primary promotion failed: Unable to promote node %d to "
		               "primary role",
		               context->new_primary_node_id);
		context->state = RAMD_FAILOVER_STATE_FAILED;
		return false;
	}

	/* Update cluster state */
	cluster->primary_node_id = context->new_primary_node_id;
	context->state = RAMD_FAILOVER_STATE_COMPLETED;
	context->completed_at = time(NULL);

	ramd_log_info("Automated failover procedure completed successfully: Node "
	              "%d has been promoted to primary role",
	              context->new_primary_node_id);
	return true;
}


bool ramd_failover_detect_primary_failure(ramd_cluster_t* cluster)
{
	if (!cluster)
		return false;

	ramd_node_t* primary = ramd_cluster_get_primary_node(cluster);
	if (!primary)
	{
		ramd_log_debug(
		    "Cluster status: No primary node currently active in cluster");
		return true;
	}

	/* Check if primary is responding */
	ramd_postgresql_connection_t conn;
	bool primary_accessible = false;

	if (ramd_postgresql_connect(&conn, primary->hostname,
	                            primary->postgresql_port, "postgres",
	                            "postgres", ""))
	{
		ramd_postgresql_status_t status;
		if (ramd_postgresql_get_status(&conn, &status))
		{
			primary_accessible =
			    status.is_running && status.accepts_connections;
		}
		ramd_postgresql_disconnect(&conn);
	}

	if (!primary_accessible)
	{
		ramd_log_warning("Primary node failure confirmed: Node %d (%s) is "
		                 "unresponsive and cannot accept connections",
		                 primary->node_id, primary->hostname);
		return true;
	}

	return false;
}


bool ramd_failover_select_new_primary(const ramd_cluster_t* cluster,
                                      int32_t* new_primary_id)
{
	if (!cluster || !new_primary_id)
		return false;

	ramd_node_t* best_candidate = NULL;
	int64_t highest_wal_lsn = 0;

	/* Find the standby with the most recent WAL position */
	for (int i = 0; i < cluster->node_count; i++)
	{
		const ramd_node_t* node = &cluster->nodes[i];

		/* Skip failed primary and non-standby nodes */
		if (node->role != RAMD_ROLE_STANDBY || !node->is_healthy)
			continue;

		/* Check PostgreSQL status */
		ramd_postgresql_connection_t conn;
		if (ramd_postgresql_connect(&conn, node->hostname,
		                            node->postgresql_port, "postgres",
		                            "postgres", ""))
		{
			ramd_postgresql_status_t status;
			if (ramd_postgresql_get_status(&conn, &status))
			{
				/* Prefer node with highest WAL LSN */
				if (status.current_wal_lsn > highest_wal_lsn)
				{
					highest_wal_lsn = status.current_wal_lsn;
					best_candidate = (ramd_node_t*) node;
				}
			}
			ramd_postgresql_disconnect(&conn);
		}
	}

	if (best_candidate)
	{
		*new_primary_id = best_candidate->node_id;
		ramd_log_info("Primary candidate selection: Node %d (%s) identified as "
		              "optimal candidate based on WAL LSN position",
		              best_candidate->node_id, best_candidate->hostname);
		return true;
	}

	ramd_log_error("Primary candidate selection failed: No eligible standby "
	               "nodes available for promotion to primary role");
	return false;
}


bool ramd_failover_promote_node(ramd_cluster_t* cluster,
                                const ramd_config_t* config, int32_t node_id)
{
	if (!cluster || !config)
		return false;

	ramd_node_t* node = ramd_cluster_find_node(cluster, node_id);
	if (!node)
	{
		ramd_log_error("Node resolution error: Specified node %d not found in "
		               "cluster configuration",
		               node_id);
		return false;
	}

	ramd_log_info("Initiating primary node promotion: Node %d (%s) will be "
	              "promoted to primary role",
	              node_id, node->hostname);

	/* Step 1: Stop replication on the target node */
	if (!ramd_failover_stop_replication_on_node(config, node_id))
	{
		ramd_log_warning(
		    "Failed to stop replication on node %d, continuing with promotion",
		    node_id);
	}

	/* Step 2: Promote PostgreSQL instance */
	bool promotion_success = ramd_postgresql_promote(config);

	if (!promotion_success)
	{
		ramd_log_error("PostgreSQL promotion failure: Unable to promote "
		               "PostgreSQL instance on node %d",
		               node_id);
		return false;
	}

	/* Step 3: Update cluster state */
	node->role = RAMD_ROLE_PRIMARY;
	cluster->primary_node_id = node_id;

	/* Step 4: Wait for promotion to complete and verify */
	ramd_log_info("Waiting for PostgreSQL promotion to complete...");
	sleep(3); /* Give PostgreSQL time to fully start as primary */

	if (!ramd_failover_validate_promotion(cluster, node_id))
	{
		ramd_log_error("Promotion validation failure: Node %d promotion could "
		               "not be verified as successful",
		               node_id);
		return false;
	}

	/* Step 5: Update synchronous replication configuration */
	if (!ramd_failover_update_sync_replication_config(cluster, node_id))
	{
		ramd_log_warning("Failed to update synchronous replication "
		                 "configuration, continuing");
	}

	ramd_log_info("Primary promotion completed successfully: Node %d is now "
	              "operational as the primary database server",
	              node_id);
	return true;
}


/* Enhanced function to stop replication on a node before promotion */
bool ramd_failover_stop_replication_on_node(const ramd_config_t* config,
                                            int32_t node_id)
{
	if (!config)
		return false;

	ramd_log_info("Stopping replication on node %d before promotion", node_id);

	/* Connect to the node and stop replication */
	ramd_postgresql_connection_t conn;
	if (ramd_postgresql_connect(&conn, config->hostname,
	                            config->postgresql_port, "postgres",
	                            config->postgresql_user, NULL))
	{
		/* Stop replication gracefully */
		PGresult* res =
		    PQexec((PGconn*) conn.connection, "SELECT pg_wal_replay_pause()");
		if (res)
		{
			PQclear(res);
			ramd_log_info("Replication paused on node %d", node_id);
		}

		/* Promote the node */
		res = PQexec((PGconn*) conn.connection, "SELECT pg_promote_node()");
		if (res)
		{
			PQclear(res);
			ramd_log_info("Node %d promoted successfully", node_id);
		}

		ramd_postgresql_disconnect(&conn);
		return true;
	}

	ramd_log_warning("Could not connect to node %d to stop replication",
	                 node_id);
	return false;
}


/* Enhanced function to update synchronous replication after failover */
bool ramd_failover_update_sync_replication_config(ramd_cluster_t* cluster,
                                                  int32_t new_primary_id)
{
	if (!cluster)
		return false;

	ramd_log_info(
	    "Updating synchronous replication configuration for new primary %d",
	    new_primary_id);

	/* Get the new primary node */
	ramd_node_t* new_primary = ramd_cluster_find_node(cluster, new_primary_id);
	if (!new_primary)
		return false;

	/* Build synchronous_standby_names string */
	char sync_standby_names[512] = "";
	int standby_count = 0;

	for (int i = 0; i < cluster->node_count; i++)
	{
		ramd_node_t* node = &cluster->nodes[i];

		if (node->node_id != new_primary_id &&
		    node->role == RAMD_ROLE_STANDBY && node->is_healthy)
		{
			if (standby_count > 0)
				strcat(sync_standby_names, ",");
			strcat(sync_standby_names, node->hostname);
			standby_count++;
		}
	}

	if (standby_count > 0)
	{
		/* Update PostgreSQL configuration */
		char config_update[1024];
		snprintf(config_update, sizeof(config_update),
		         "ALTER SYSTEM SET synchronous_standby_names = '%s'",
		         sync_standby_names);

		ramd_postgresql_connection_t conn;
		if (ramd_postgresql_connect(&conn, new_primary->hostname,
		                            new_primary->postgresql_port, "postgres",
		                            "postgres", ""))
		{
			PGresult* res = PQexec((PGconn*) conn.connection, config_update);
			if (res)
			{
				PQclear(res);
				ramd_log_info("Updated synchronous_standby_names to: %s",
				              sync_standby_names);
			}

			/* Reload configuration */
			res = PQexec((PGconn*) conn.connection, "SELECT pg_reload_conf()");
			if (res)
			{
				PQclear(res);
				ramd_log_info(
				    "PostgreSQL configuration reloaded on new primary");
			}

			ramd_postgresql_disconnect(&conn);
			return true;
		}
	}

	ramd_log_info("No healthy standby nodes found for synchronous replication "
	              "configuration");
	return true;
}


/* Enhanced function to rebuild failed replicas */
bool ramd_failover_rebuild_failed_replicas(ramd_cluster_t* cluster,
                                           const ramd_config_t* config)
{
	if (!cluster || !config)
		return false;

	ramd_log_info("Starting automatic rebuild of failed replica nodes");

	int rebuilt_count = 0;

	for (int i = 0; i < cluster->node_count; i++)
	{
		ramd_node_t* node = &cluster->nodes[i];

		if (node->state == RAMD_NODE_STATE_FAILED &&
		    node->role == RAMD_ROLE_STANDBY)
		{
			ramd_log_info("Rebuilding failed replica node %d (%s)",
			              node->node_id, node->hostname);

			if (ramd_failover_rebuild_replica_node(cluster, config,
			                                       node->node_id))
			{
				node->state = RAMD_NODE_STATE_UNKNOWN;
				node->is_healthy = true;
				rebuilt_count++;
				ramd_log_info("Successfully rebuilt replica node %d",
				              node->node_id);
			}
			else
			{
				ramd_log_error("Failed to rebuild replica node %d",
				               node->node_id);
			}
		}
	}

	ramd_log_info("Replica rebuild completed: %d nodes rebuilt successfully",
	              rebuilt_count);
	return rebuilt_count > 0;
}


/* New function to rebuild a single replica node */
bool ramd_failover_rebuild_replica_node(ramd_cluster_t* cluster,
                                        const ramd_config_t* config,
                                        int32_t node_id)
{
	if (!cluster || !config)
		return false;

	ramd_node_t* node = ramd_cluster_find_node(cluster, node_id);
	if (!node)
		return false;

	ramd_node_t* primary = ramd_cluster_get_primary_node(cluster);
	if (!primary)
	{
		ramd_log_error("Cannot rebuild replica: No primary node available");
		return false;
	}

	ramd_log_info("Rebuilding replica node %d from primary %d", node_id,
	              primary->node_id);

	/* Step 1: Stop PostgreSQL on the failed node */
	if (!ramd_postgresql_stop(config))
	{
		ramd_log_warning(
		    "Could not stop PostgreSQL on node %d, continuing with rebuild",
		    node_id);
	}

	/* Step 2: Remove old data directory */
	char rm_cmd[512];
	snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s/*",
	         config->postgresql_data_dir);
	if (system(rm_cmd) != 0)
	{
		ramd_log_warning("Could not clean data directory on node %d", node_id);
	}

	/* Step 3: Take base backup from primary */
	if (!ramd_failover_take_basebackup(config, primary->hostname,
	                                   primary->postgresql_port))
	{
		ramd_log_error("Failed to take base backup for node %d", node_id);
		return false;
	}

	/* Step 4: Configure recovery */
	if (!ramd_failover_configure_recovery(config, primary->hostname,
	                                      primary->postgresql_port))
	{
		ramd_log_error("Failed to configure recovery for node %d", node_id);
		return false;
	}

	/* Step 5: Start PostgreSQL as standby */
	if (!ramd_postgresql_start(config))
	{
		ramd_log_error("Failed to start PostgreSQL on rebuilt node %d",
		               node_id);
		return false;
	}

	/* Step 6: Verify the node is replicating */
	sleep(5); /* Wait for replication to start */

	ramd_postgresql_connection_t conn;
	if (ramd_postgresql_connect(&conn, node->hostname, node->postgresql_port,
	                            "postgres", "postgres", ""))
	{
		PGresult* res =
		    PQexec((PGconn*) conn.connection, "SELECT pg_is_in_recovery()");
		if (res && PQntuples(res) > 0)
		{
			bool is_recovering = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
			PQclear(res);

			if (is_recovering)
			{
				ramd_log_info("Replica node %d is successfully replicating",
				              node_id);
				ramd_postgresql_disconnect(&conn);
				return true;
			}
		}
		ramd_postgresql_disconnect(&conn);
	}

	ramd_log_error("Replica node %d is not replicating after rebuild", node_id);
	return false;
}


bool ramd_failover_demote_failed_primary(ramd_cluster_t* cluster,
                                         const ramd_config_t* config,
                                         int32_t failed_node_id)
{
	if (!cluster || !config)
		return false;

	ramd_node_t* failed_node = ramd_cluster_find_node(cluster, failed_node_id);
	if (!failed_node)
		return true; /* Node not found, already demoted */

	ramd_log_info("Demoting failed primary node %d (%s)", failed_node_id,
	              failed_node->hostname);

	/* Try to gracefully stop PostgreSQL on failed node */
	/* This may not work if the node is truly failed */
	ramd_postgresql_stop(config);

	/* Update cluster state */
	failed_node->role =
	    RAMD_ROLE_STANDBY; /* Mark as standby since it's failed */
	failed_node->state = RAMD_NODE_STATE_FAILED;
	failed_node->is_healthy = false;

	return true;
}


bool ramd_failover_update_standby_nodes(ramd_cluster_t* cluster,
                                        const ramd_config_t* config,
                                        int32_t new_primary_id)
{
	if (!cluster || !config)
		return false;

	ramd_node_t* new_primary = ramd_cluster_find_node(cluster, new_primary_id);
	if (!new_primary)
		return false;

	ramd_log_info("Updating standby nodes to replicate from new primary %d",
	              new_primary_id);

	/* Update all standby nodes to point to new primary */
	for (int i = 0; i < cluster->node_count; i++)
	{
		ramd_node_t* node = &cluster->nodes[i];

		/* Skip the new primary and failed nodes */
		if (node->node_id == new_primary_id ||
		    node->state == RAMD_NODE_STATE_FAILED)
			continue;

		if (node->role == RAMD_ROLE_STANDBY)
		{
			ramd_log_info(
			    "Reconfiguring standby node %d to replicate from new primary",
			    node->node_id);

			/* For each standby, we need to set up replication to the new
			 * primary */
			/* This would typically involve updating recovery.conf or
			 * postgresql.conf */
			ramd_postgresql_setup_replication(config, new_primary->hostname,
			                                  new_primary->postgresql_port);
		}
	}

	return true;
}


bool ramd_failover_validate_promotion(const ramd_cluster_t* cluster,
                                      int32_t promoted_node_id)
{
	if (!cluster)
		return false;

	ramd_node_t* promoted_node =
	    ramd_cluster_find_node((ramd_cluster_t*) cluster, promoted_node_id);
	if (!promoted_node)
		return false;

	/* Connect to promoted node and verify it's accepting writes */
	ramd_postgresql_connection_t conn;
	if (!ramd_postgresql_connect(&conn, promoted_node->hostname,
	                             promoted_node->postgresql_port, "postgres",
	                             "postgres", ""))
	{
		ramd_log_error("Failed to connect to promoted node %d for validation",
		               promoted_node_id);
		return false;
	}

	ramd_postgresql_status_t status;
	bool is_valid = false;

	if (ramd_postgresql_get_status(&conn, &status))
	{
		is_valid = status.is_running && status.is_primary &&
		           status.accepts_connections && !status.is_in_recovery;
	}

	ramd_postgresql_disconnect(&conn);

	if (is_valid)
	{
		ramd_log_info("Promotion validation successful for node %d",
		              promoted_node_id);
	}
	else
	{
		ramd_log_error("Promotion validation failed for node %d",
		               promoted_node_id);
	}

	return is_valid;
}


bool ramd_failover_validate_cluster_state(const ramd_cluster_t* cluster)
{
	if (!cluster)
		return false;

	/* Check that we have exactly one primary */
	int primary_count = 0;
	int healthy_standby_count = 0;

	for (int i = 0; i < cluster->node_count; i++)
	{
		const ramd_node_t* node = &cluster->nodes[i];

		if (node->role == RAMD_ROLE_PRIMARY)
			primary_count++;
		else if (node->role == RAMD_ROLE_STANDBY && node->is_healthy)
			healthy_standby_count++;
	}

	if (primary_count != 1)
	{
		ramd_log_error("Invalid cluster state: %d primary nodes (expected 1)",
		               primary_count);
		return false;
	}

	if (healthy_standby_count < 1)
	{
		ramd_log_warning("Cluster has no healthy standby nodes");
	}

	ramd_log_info(
	    "Cluster state validation passed: 1 primary, %d healthy standbys",
	    healthy_standby_count);
	return true;
}


bool ramd_failover_recover_failed_node(ramd_cluster_t* cluster,
                                       const ramd_config_t* config,
                                       int32_t failed_node_id)
{
	if (!cluster || !config)
		return false;

	ramd_node_t* failed_node = ramd_cluster_find_node(cluster, failed_node_id);
	if (!failed_node)
		return false;

	ramd_log_info("Attempting to recover failed node %d (%s)", failed_node_id,
	              failed_node->hostname);

	/* Find current primary for replication setup */
	ramd_node_t* primary = ramd_cluster_get_primary_node(cluster);
	if (!primary)
	{
		ramd_log_error("No primary node found for recovery");
		return false;
	}

	/* Take base backup from primary */
	if (!ramd_failover_take_basebackup(config, primary->hostname,
	                                   primary->postgresql_port))
	{
		ramd_log_error("Failed to take base backup for node recovery");
		return false;
	}

	/* Configure recovery settings */
	if (!ramd_failover_configure_recovery(config, primary->hostname,
	                                      primary->postgresql_port))
	{
		ramd_log_error("Failed to configure recovery for node");
		return false;
	}

	/* Update node status */
	failed_node->role = RAMD_ROLE_STANDBY;
	failed_node->is_healthy = true;

	ramd_log_info("Successfully recovered node %d as standby", failed_node_id);
	return true;
}


bool ramd_failover_take_basebackup(const ramd_config_t* config,
                                   const char* primary_host,
                                   int32_t primary_port)
{
	if (!config || !primary_host)
		return false;

	int result;

	ramd_log_info("Taking base backup from %s:%d", primary_host, primary_port);

	/* Use ramd_basebackup function */
	char conninfo[512];
	snprintf(conninfo, sizeof(conninfo), "host=%s port=%d dbname=postgres user=%s",
	         primary_host, primary_port, config->replication_user);
	
	PGconn *conn = ramd_conn_get(primary_host, primary_port, "postgres", 
	                             config->replication_user, "");
	if (!conn) {
		ramd_log_error("Failed to connect to primary for backup");
		return false;
	}

	result = ramd_take_basebackup(conn, config->postgresql_data_dir, "failover_backup");
	ramd_conn_close(conn);

	if (result == 0)
	{
		ramd_log_info("Base backup completed successfully");
		return true;
	}
	else
	{
		ramd_log_error("Base backup failed with exit code %d", result);
		return false;
	}
}


bool ramd_failover_configure_recovery(const ramd_config_t* config,
                                      const char* primary_host,
                                      int32_t primary_port)
{
	if (!config || !primary_host)
		return false;

	char recovery_conf_path[RAMD_MAX_PATH_LENGTH];
	FILE* recovery_file;

	ramd_log_info("Configuring recovery to replicate from %s:%d", primary_host,
	              primary_port);

	/* Create recovery.conf file path */
	snprintf(recovery_conf_path, sizeof(recovery_conf_path), "%s/recovery.conf",
	         config->postgresql_data_dir);

	/* Write recovery configuration */
	recovery_file = fopen(recovery_conf_path, "w");
	if (!recovery_file)
	{
		ramd_log_error("Failed to create recovery.conf file");
		return false;
	}

	fprintf(recovery_file, "standby_mode = 'on'\n");
	fprintf(recovery_file, "primary_conninfo = 'host=%s port=%d user=%s'\n",
	        primary_host, primary_port, config->replication_user);
	fprintf(recovery_file, "trigger_file = '%s/postgresql.trigger'\n",
	        config->postgresql_data_dir);
	fprintf(recovery_file, "recovery_target_timeline = 'latest'\n");

	fclose(recovery_file);

	ramd_log_info("Recovery configuration written to %s", recovery_conf_path);
	return true;
}
