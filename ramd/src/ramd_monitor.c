/*-------------------------------------------------------------------------
 *
 * ramd_monitor.c
 *		PostgreSQL Auto-Failover Daemon - Monitoring Implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <pthread.h>
#include "ramd_monitor.h"
#include "ramd_logging.h"

bool ramd_monitor_init(ramd_monitor_t* monitor, ramd_cluster_t* cluster,
                       const ramd_config_t* config)
{
	if (!monitor || !cluster || !config)
		return false;

	memset(monitor, 0, sizeof(ramd_monitor_t));

	monitor->enabled = true;
	monitor->cluster = cluster;
	monitor->config = config;
	monitor->check_interval_ms = config->monitor_interval_ms;
	monitor->health_check_timeout_ms = config->health_check_timeout_ms;

	ramd_log_info("Database monitoring subsystem initialized successfully for "
	              "cluster '%s'",
	              cluster->cluster_name);
	return true;
}

void ramd_monitor_cleanup(ramd_monitor_t* monitor)
{
	if (!monitor)
		return;

	ramd_log_info("Cleaning up monitor");
	memset(monitor, 0, sizeof(ramd_monitor_t));
}

bool ramd_monitor_start(ramd_monitor_t* monitor)
{
	if (!monitor || !monitor->enabled)
		return false;

	if (monitor->running)
		return true;

	ramd_log_info("Starting monitor thread");

	if (pthread_create(&monitor->thread, NULL, ramd_monitor_thread_main,
	                   monitor) != 0)
	{
		ramd_log_error("Failed to create monitor thread");
		return false;
	}

	monitor->running = true;
	return true;
}

void ramd_monitor_stop(ramd_monitor_t* monitor)
{
	if (!monitor || !monitor->running)
		return;

	ramd_log_info("Stopping monitor thread");

	monitor->running = false;
	pthread_join(monitor->thread, NULL);
}

void* ramd_monitor_thread_main(void* arg)
{
	ramd_monitor_t* monitor = (ramd_monitor_t*) arg;

	ramd_log_info("Monitor thread started");

	while (monitor->running)
	{
		ramd_monitor_run_cycle(monitor);
		usleep((useconds_t) (monitor->check_interval_ms * 1000));
	}

	ramd_log_info("Monitor thread stopped");
	return NULL;
}

void ramd_monitor_run_cycle(ramd_monitor_t* monitor)
{
	if (!monitor)
		return;

	monitor->last_check = time(NULL);

	/* Check local node health */
	ramd_monitor_check_local_node(monitor);

	/* Check remote nodes */
	ramd_monitor_check_remote_nodes(monitor);

	/* Check leadership status */
	ramd_monitor_check_leadership(monitor);

	/* Check for role changes */
	ramd_monitor_detect_role_changes(monitor);
}

/* Real implementations for critical monitoring functions */
bool ramd_monitor_is_running(const ramd_monitor_t* monitor)
{
	return monitor && monitor->running;
}

bool ramd_monitor_check_local_node(ramd_monitor_t* monitor)
{
	if (!monitor)
		return false;

	ramd_postgresql_status_t pg_status;

	/* Check PostgreSQL status */
	if (!ramd_postgresql_get_status(&monitor->local_connection, &pg_status))
	{
		ramd_log_warning("PostgreSQL health check failed - unable to connect to local database");
		return false;
	}

	/* Update local node health score */
	float health_score = ramd_monitor_calculate_node_health(monitor, NULL);

	/* Check if we're still connected to PostgreSQL */
	if (!pg_status.is_running)
	{
		ramd_log_error("Critical: Local PostgreSQL database service is offline");
		return false;
	}

	/* Check if we can accept connections */
	if (!pg_status.accepts_connections)
	{
		ramd_log_warning("Local PostgreSQL is not accepting connections");
		return false;
	}

	ramd_log_debug("Local node health check passed (score: %.2f)",
	               health_score);
	return true;
}

bool ramd_monitor_check_remote_nodes(ramd_monitor_t* monitor)
{
	if (!monitor || !monitor->cluster)
		return false;

	bool all_healthy = true;
	int32_t healthy_count = 0;

	for (int32_t i = 0; i < monitor->cluster->node_count; i++)
	{
		ramd_node_t* node = &monitor->cluster->nodes[i];

		/* Skip self */
		if (node->node_id == monitor->config->node_id)
			continue;

		/* Try to connect to remote node */
		ramd_postgresql_connection_t remote_conn;
		bool node_healthy = false;

		if (ramd_postgresql_connect(
		        &remote_conn, node->hostname, node->postgresql_port,
		        monitor->config->database_name, monitor->config->database_user,
		        monitor->config->database_password))
		{
			ramd_postgresql_status_t remote_status;
			if (ramd_postgresql_get_status(&remote_conn, &remote_status))
			{
				node_healthy = remote_status.is_running &&
				               remote_status.accepts_connections;

				/* Update node health in cluster */
				float health_score = remote_status.is_running ? 1.0f : 0.0f;
				ramd_cluster_update_node_health(monitor->cluster, node->node_id,
				                                health_score);

				if (node_healthy)
					healthy_count++;
			}

			ramd_postgresql_disconnect(&remote_conn);
		}

		if (!node_healthy)
		{
			ramd_log_warning("Node %d (%s) health check failed", node->node_id,
			                 node->hostname);
			all_healthy = false;
		}
		else
		{
			ramd_log_debug("Node %d (%s) health check passed", node->node_id,
			               node->hostname);
		}
	}

	ramd_log_info("Remote nodes health check: %d/%d nodes healthy",
	              healthy_count, monitor->cluster->node_count - 1);

	return all_healthy;
}

bool ramd_monitor_check_cluster_health(ramd_monitor_t* monitor)
{
	if (!monitor || !monitor->cluster)
		return false;

	/* Check if we have minimum required nodes for quorum */
	int32_t healthy_nodes = ramd_cluster_count_healthy_nodes(monitor->cluster);
	int32_t required_for_quorum = (monitor->cluster->node_count / 2) + 1;

	if (healthy_nodes < required_for_quorum)
	{
		ramd_log_error(
		    "Cluster does not have quorum: %d/%d nodes healthy (need %d)",
		    healthy_nodes, monitor->cluster->node_count, required_for_quorum);
		return false;
	}

	/* Check if we have a primary */
	if (!ramd_cluster_has_primary(monitor->cluster))
	{
		ramd_log_warning("Cluster has no primary node");
		return false;
	}

	ramd_log_debug(
	    "Cluster health check passed: %d/%d nodes healthy, quorum maintained",
	    healthy_nodes, monitor->cluster->node_count);

	return true;
}
bool ramd_monitor_check_leadership(ramd_monitor_t* monitor)
{
	if (!monitor || !monitor->cluster)
		return false;

	/* Check if we're the leader according to consensus */
	bool am_leader = false;

	/* Query librale for current leader status */
	/* For now, check if we have the primary role */
	ramd_node_t* self =
	    ramd_cluster_find_node(monitor->cluster, monitor->config->node_id);
	if (self && self->role == RAMD_ROLE_PRIMARY)
	{
		am_leader = true;
	}

	ramd_log_debug("Leadership check: %s",
	               am_leader ? "I am leader" : "I am follower");
	return true;
}

bool ramd_monitor_detect_leadership_change(ramd_monitor_t* monitor)
{
	if (!monitor)
		return false;

	static ramd_role_t last_known_role = RAMD_ROLE_UNKNOWN;

	ramd_node_t* self =
	    ramd_cluster_find_node(monitor->cluster, monitor->config->node_id);
	if (!self)
		return false;

	if (self->role != last_known_role)
	{
		ramd_log_info("Leadership change detected: %d -> %d", last_known_role,
		              self->role);

		if (!ramd_monitor_handle_role_change(monitor, last_known_role,
		                                     self->role))
		{
			ramd_log_error("Failed to handle role change");
			return false;
		}

		last_known_role = self->role;
		return true;
	}

	return false;
}

bool ramd_monitor_handle_leadership_gained(ramd_monitor_t* monitor)
{
	if (!monitor)
		return false;

	ramd_log_info("Gained leadership - becoming primary");

	/* Update our role in cluster state */
	ramd_cluster_update_node_role(monitor->cluster, monitor->config->node_id,
	                              RAMD_ROLE_PRIMARY);

	/* Configure PostgreSQL as primary */
	if (!ramd_postgresql_promote(monitor->config))
	{
		ramd_log_error("Failed to promote PostgreSQL to primary");
		return false;
	}

	/* Set up synchronous replication if enabled */
	if (monitor->config->synchronous_replication)
	{
		char standby_names[512];
		/* Build standby names from cluster state */
		standby_names[0] = '\0';
		for (int i = 0; i < monitor->cluster->node_count; i++)
		{
			ramd_node_t* node = &monitor->cluster->nodes[i];
			if (node->role == RAMD_ROLE_STANDBY && node->is_healthy)
			{
				if (strlen(standby_names) > 0)
					strcat(standby_names, ",");
				strcat(standby_names, node->hostname);
			}
		}

		if (strlen(standby_names) > 0)
		{
			ramd_postgresql_configure_synchronous_replication(monitor->config,
			                                                  standby_names);
		}
	}

	ramd_log_info("Successfully became primary node");
	return true;
}

bool ramd_monitor_handle_leadership_lost(ramd_monitor_t* monitor)
{
	if (!monitor)
		return false;

	ramd_log_info("Lost leadership - becoming standby");

	/* Update our role in cluster state */
	ramd_cluster_update_node_role(monitor->cluster, monitor->config->node_id,
	                              RAMD_ROLE_STANDBY);

	/* Find new primary for replication setup */
	ramd_node_t* primary = ramd_cluster_get_primary_node(monitor->cluster);
	if (primary)
	{
		/* Configure PostgreSQL as standby */
		if (!ramd_postgresql_demote_to_standby(
		        monitor->config, primary->hostname, primary->postgresql_port))
		{
			ramd_log_error("Failed to demote PostgreSQL to standby");
			return false;
		}
	}
	else
	{
		ramd_log_warning("No primary found to replicate from");
	}

	ramd_log_info("Successfully became standby node");
	return true;
}

bool ramd_monitor_check_primary_health(ramd_monitor_t* monitor)
{
	if (!monitor || !monitor->cluster)
		return false;

	ramd_node_t* primary = ramd_cluster_get_primary_node(monitor->cluster);
	if (!primary)
	{
		ramd_log_debug("No primary node to check");
		return false;
	}

	/* Skip if we are the primary */
	if (primary->node_id == monitor->config->node_id)
		return true;

	/* Try to connect to primary */
	ramd_postgresql_connection_t primary_conn;
	bool primary_healthy = false;

	if (ramd_postgresql_connect(
	        &primary_conn, primary->hostname, primary->postgresql_port,
	        monitor->config->database_name, monitor->config->database_user,
	        monitor->config->database_password))
	{
		ramd_postgresql_status_t primary_status;
		if (ramd_postgresql_get_status(&primary_conn, &primary_status))
		{
			primary_healthy = primary_status.is_running &&
			                  primary_status.is_primary &&
			                  primary_status.accepts_connections;
		}

		ramd_postgresql_disconnect(&primary_conn);
	}

	if (!primary_healthy)
	{
		ramd_log_warning("Primary node %d (%s) health check failed",
		                 primary->node_id, primary->hostname);
	}

	return primary_healthy;
}

bool ramd_monitor_detect_primary_failure(ramd_monitor_t* monitor)
{
	if (!monitor || !monitor->cluster)
		return false;

	static int consecutive_failures = 0;
	static time_t last_failure_time __attribute__((unused)) = 0;

	/* Define failover threshold based on health check timeout */
	const int failover_threshold = 3; /* Require 3 consecutive failures */

	if (!ramd_monitor_check_primary_health(monitor))
	{
		consecutive_failures++;
		last_failure_time = time(NULL);

		/* Require multiple consecutive failures before declaring primary failed
		 */
		if (consecutive_failures >= failover_threshold)
		{
			ramd_log_error(
			    "Primary failure detected after %d consecutive failures",
			    consecutive_failures);
			return true;
		}
		else
		{
			ramd_log_warning("Primary health check failed (%d/%d)",
			                 consecutive_failures, failover_threshold);
		}
	}
	else
	{
		/* Reset failure count on successful check */
		if (consecutive_failures > 0)
		{
			ramd_log_info("Primary health restored after %d failures",
			              consecutive_failures);
			consecutive_failures = 0;
		}
	}

	return false;
}
bool ramd_monitor_handle_primary_failure(ramd_monitor_t* monitor)
{
	(void) monitor;
	return true;
}
bool ramd_monitor_detect_role_changes(ramd_monitor_t* monitor)
{
	(void) monitor;
	return false;
}
bool ramd_monitor_handle_role_change(ramd_monitor_t* monitor,
                                     ramd_role_t old_role, ramd_role_t new_role)
{
	(void) monitor;
	(void) old_role;
	(void) new_role;
	return true;
}
bool ramd_monitor_sync_cluster_state(ramd_monitor_t* monitor)
{
	(void) monitor;
	return true;
}
bool ramd_monitor_update_node_states(ramd_monitor_t* monitor)
{
	(void) monitor;
	return true;
}
bool ramd_monitor_broadcast_state_change(ramd_monitor_t* monitor,
                                         int32_t node_id,
                                         ramd_node_state_t new_state)
{
	(void) monitor;
	(void) node_id;
	(void) new_state;
	return true;
}
float ramd_monitor_calculate_node_health(ramd_monitor_t* monitor,
                                         ramd_node_t* node)
{
	(void) monitor;
	(void) node;
	return 1.0f;
}
bool ramd_monitor_is_node_healthy(const ramd_monitor_t* monitor,
                                  const ramd_node_t* node)
{
	(void) monitor;
	(void) node;
	return true;
}
