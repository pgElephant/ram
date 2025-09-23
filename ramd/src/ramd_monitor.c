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

extern PGconn *g_conn;

bool
ramd_monitor_init(ramd_monitor_t *monitor, ramd_cluster_t *cluster,
                 const ramd_config_t *config)
{
	if (!monitor || !cluster || !config)
		return false;

	memset(monitor, 0, sizeof(ramd_monitor_t));

	monitor->enabled = true;
	monitor->cluster = cluster;
	monitor->config = config;
	monitor->check_interval_ms = config->monitor_interval_ms;
	monitor->health_check_timeout_ms = config->health_check_timeout_ms;

	ramd_log_info("Database monitoring subsystem initialized successfully for cluster '%s'",
	              cluster->cluster_name);
	return true;
}

void
ramd_monitor_cleanup(ramd_monitor_t *monitor)
{
	if (!monitor)
		return;

	ramd_log_info("Cleaning up monitor");
	memset(monitor, 0, sizeof(ramd_monitor_t));
}

bool
ramd_monitor_start(ramd_monitor_t *monitor)
{
	if (!monitor || !monitor->enabled)
		return false;

	if (monitor->running)
		return true;

	ramd_log_info("Starting monitor thread");

	if (pthread_create(&monitor->thread, NULL, ramd_monitor_thread_main, monitor) != 0)
	{
		ramd_log_error("Failed to create monitor thread");
		return false;
	}

	monitor->running = true;
	return true;
}

void
ramd_monitor_stop(ramd_monitor_t *monitor)
{
	if (!monitor || !monitor->running)
		return;

	ramd_log_info("Stopping monitor thread");

	monitor->running = false;
	pthread_join(monitor->thread, NULL);
}

void*
ramd_monitor_thread_main(void *arg)
{
	ramd_monitor_t *monitor;

	monitor = (ramd_monitor_t *) arg;

	ramd_log_info("Monitor thread started");

	while (monitor->running)
	{
		ramd_monitor_run_cycle(monitor);
		usleep((useconds_t) (monitor->check_interval_ms * 1000));
	}

	ramd_log_info("Monitor thread stopped");
	return NULL;
}

void
ramd_monitor_run_cycle(ramd_monitor_t *monitor)
{
	if (!monitor)
		return;

	monitor->last_check = time(NULL);

	ramd_monitor_check_local_node(monitor);
	ramd_monitor_check_remote_nodes(monitor);
	ramd_monitor_check_leadership(monitor);
	ramd_monitor_detect_role_changes(monitor);
}

bool
ramd_monitor_is_running(const ramd_monitor_t *monitor)
{
	return monitor && monitor->running;
}

bool
ramd_monitor_check_local_node(ramd_monitor_t *monitor)
{
	ramd_postgresql_status_t pg_status;
	ramd_postgresql_connection_t local_conn;
	bool                       local_healthy = false;
	float                     health_score;

	if (!monitor)
		return false;

	if (ramd_postgresql_connect(&local_conn, monitor->config->hostname,
	                           monitor->config->postgresql_port,
	                           monitor->config->database_name,
	                           monitor->config->database_user,
	                           monitor->config->database_password))
	{
		if (ramd_postgresql_get_status(&local_conn, &pg_status))
			local_healthy = true;

		ramd_postgresql_disconnect(&local_conn);
	}

	if (!local_healthy)
	{
		ramd_log_warning("PostgreSQL health check failed - unable to connect to local database");
		return false;
	}

	health_score = ramd_monitor_calculate_node_health(monitor, NULL);

	if (!pg_status.is_running)
	{
		ramd_log_error("Local PostgreSQL database service is offline");
		return false;
	}

	if (!pg_status.accepts_connections)
	{
		ramd_log_warning("Local PostgreSQL is not accepting connections");
		return false;
	}

	ramd_log_debug("Local node health check: score=%.2f, role=%s, state=%s, uptime=%lds",
	               health_score, 
	               monitor->cluster->nodes[monitor->cluster->local_node_id].role == RAMD_ROLE_PRIMARY ? "primary" : "standby",
	               monitor->cluster->nodes[monitor->cluster->local_node_id].state == RAMD_NODE_STATE_PRIMARY ? "primary" : "standby",
	               time(NULL) - monitor->cluster->nodes[monitor->cluster->local_node_id].last_seen);
	return true;
}

bool
ramd_monitor_check_remote_nodes(ramd_monitor_t *monitor)
{
	bool     all_healthy = true;
	int32_t  healthy_count = 0;
	int32_t  i;
	int32_t  remote_node_count;

	if (!monitor || !monitor->cluster)
		return false;

	for (i = 0; i < monitor->cluster->node_count; i++)
	{
		ramd_node_t                 *node = &monitor->cluster->nodes[i];
		ramd_postgresql_connection_t remote_conn;
		bool                        node_healthy = false;

		if (node->node_id == monitor->config->node_id)
			continue;

		if (ramd_postgresql_connect(&remote_conn, node->hostname,
		                           node->postgresql_port,
		                           monitor->config->database_name,
		                           monitor->config->database_user,
		                           monitor->config->database_password))
		{
			ramd_postgresql_status_t remote_status;
			float                    health_score;

			if (ramd_postgresql_get_status(&remote_conn, &remote_status))
			{
				node_healthy = remote_status.is_running && remote_status.accepts_connections;
				health_score = remote_status.is_running ? RAMD_MAX_HEALTH_SCORE : RAMD_MIN_HEALTH_SCORE;

				ramd_cluster_update_node_health(monitor->cluster, node->node_id, health_score);

				if (node_healthy)
					healthy_count++;
			}

			ramd_postgresql_disconnect(&remote_conn);
		}

		if (!node_healthy)
		{
			ramd_log_warning("Node %d (%s) health check failed",
			                node->node_id, node->hostname);
			all_healthy = false;
		}
		else
		{
			ramd_log_debug("Remote node health check: node=%d (%s), score=%.2f, role=%s, lag=%dms",
			              node->node_id, node->hostname, node->health_score,
			              node->role == RAMD_ROLE_PRIMARY ? "primary" : "standby",
			              node->replication_lag_ms);
		}
	}

	remote_node_count = monitor->cluster->node_count > 0 ? monitor->cluster->node_count - 1 : 0;
	ramd_log_info("Remote nodes health check: %d/%d nodes healthy",
	              healthy_count, remote_node_count);

	return all_healthy;
}

bool
ramd_monitor_check_cluster_health(ramd_monitor_t *monitor)
{
	int32_t healthy_nodes;
	int32_t required_for_quorum;

	if (!monitor || !monitor->cluster)
		return false;

	healthy_nodes = ramd_cluster_count_healthy_nodes(monitor->cluster);
	required_for_quorum = (monitor->cluster->node_count / 2) + 1;

	if (healthy_nodes < required_for_quorum)
	{
		ramd_log_error("Cluster does not have quorum: %d/%d nodes healthy (need %d)",
		               healthy_nodes, monitor->cluster->node_count, required_for_quorum);
		return false;
	}

	if (!ramd_cluster_has_primary(monitor->cluster))
	{
		ramd_log_warning("Cluster has no primary node");
		return false;
	}

	ramd_log_debug("Cluster health assessment: healthy=%d/%d, quorum=%s, primary=%d, leader=%d, term=%d",
	               healthy_nodes, monitor->cluster->node_count,
	               ramd_cluster_has_quorum(monitor->cluster) ? "yes" : "no",
	               monitor->cluster->primary_node_id, monitor->cluster->leader_node_id, 0);

	return true;
}

bool
ramd_monitor_check_leadership(ramd_monitor_t *monitor)
{
	bool         am_leader = false;
	ramd_node_t *self;

	if (!monitor || !monitor->cluster)
		return false;

	self = ramd_cluster_find_node(monitor->cluster, monitor->config->node_id);
	if (self && (self->role == RAMD_ROLE_PRIMARY || self->is_leader))
		am_leader = true;

	ramd_log_debug("Leadership status: am_leader=%s, role=%s, primary_id=%d, leader_id=%d",
	               am_leader ? "true" : "false",
	               self ? (self->role == RAMD_ROLE_PRIMARY ? "primary" : "standby") : "unknown",
	               monitor->cluster->primary_node_id, monitor->cluster->leader_node_id);
	return true;
}

bool
ramd_monitor_detect_leadership_change(ramd_monitor_t *monitor)
{
	static ramd_role_t last_known_role = RAMD_ROLE_UNKNOWN;
	ramd_node_t       *self;

	if (!monitor)
		return false;

	self = ramd_cluster_find_node(monitor->cluster, monitor->config->node_id);
	if (!self)
		return false;

	if (self->role != last_known_role)
	{
		ramd_log_info("Leadership change detected: %d -> %d",
		              last_known_role, self->role);

		if (!ramd_monitor_handle_role_change(monitor, last_known_role, self->role))
		{
			ramd_log_error("Failed to handle role change");
			return false;
		}

		last_known_role = self->role;
		return true;
	}

	return false;
}

bool
ramd_monitor_handle_leadership_gained(ramd_monitor_t *monitor)
{
	char     standby_names[RAMD_MAX_COMMAND_LENGTH];
	int      i;

	if (!monitor)
		return false;

	ramd_log_info("Gained leadership - becoming primary");

	ramd_cluster_update_node_role(monitor->cluster, monitor->config->node_id,
	                             RAMD_ROLE_PRIMARY);

	if (!ramd_postgresql_promote(monitor->config))
	{
		ramd_log_error("Failed to promote PostgreSQL to primary");
		return false;
	}

	if (monitor->config->synchronous_replication)
	{
		size_t current_len = 0;
		standby_names[0] = '\0';
		for (i = 0; i < monitor->cluster->node_count; i++)
		{
			ramd_node_t *node = &monitor->cluster->nodes[i];
			if (node->role == RAMD_ROLE_STANDBY && node->is_healthy)
			{
				size_t hostname_len = strlen(node->hostname);
				if (current_len > 0)
				{
					if (current_len + 1 < sizeof(standby_names) - 1)
					{
						strncat(standby_names, ",", sizeof(standby_names) - current_len - 1);
						current_len++;
					}
					else
					{
						ramd_log_error("Standby names buffer too small");
						break;
					}
				}
				if (current_len + hostname_len < sizeof(standby_names) - 1)
				{
					strncat(standby_names, node->hostname, sizeof(standby_names) - current_len - 1);
					current_len += hostname_len;
				}
				else
				{
					ramd_log_error("Standby names buffer too small for hostname");
					break;
				}
			}
		}

		if (strlen(standby_names) > 0)
			ramd_postgresql_configure_synchronous_replication(monitor->config, standby_names);
	}

	ramd_log_info("Successfully became primary node");
	return true;
}

bool
ramd_monitor_handle_leadership_lost(ramd_monitor_t *monitor)
{
	ramd_node_t *primary;

	if (!monitor)
		return false;

	ramd_log_info("Lost leadership - becoming standby");

	ramd_cluster_update_node_role(monitor->cluster, monitor->config->node_id,
	                             RAMD_ROLE_STANDBY);

	primary = ramd_cluster_get_primary_node(monitor->cluster);
	if (primary)
	{
		if (!ramd_postgresql_demote_to_standby(monitor->config,
		                                       primary->hostname,
		                                       primary->postgresql_port))
		{
			ramd_log_error("Failed to demote PostgreSQL to standby");
			return false;
		}
	}
	else
		ramd_log_warning("No primary found to replicate from");

	ramd_log_info("Successfully became standby node");
	return true;
}

bool
ramd_monitor_check_primary_health(ramd_monitor_t *monitor)
{
	ramd_node_t                 *primary;
	ramd_postgresql_connection_t primary_conn;
	bool                        primary_healthy = false;

	if (!monitor || !monitor->cluster)
		return false;

	primary = ramd_cluster_get_primary_node(monitor->cluster);
	if (!primary)
	{
		ramd_log_debug("No primary node to check");
		return false;
	}

	if (primary->node_id == monitor->config->node_id)
		return true;

	if (ramd_postgresql_connect(&primary_conn, primary->hostname,
	                           primary->postgresql_port,
	                           monitor->config->database_name,
	                           monitor->config->database_user,
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

bool
ramd_monitor_detect_primary_failure(ramd_monitor_t *monitor)
{
	static int      consecutive_failures = 0;
	static time_t   last_failure_time __attribute__((unused)) = 0;
	const int       failover_threshold = RAMD_FAILOVER_THRESHOLD;

	if (!monitor || !monitor->cluster)
		return false;

	if (!ramd_monitor_check_primary_health(monitor))
	{
		consecutive_failures++;
		last_failure_time = time(NULL);

		if (consecutive_failures >= failover_threshold)
		{
			ramd_log_error("Primary failure detected after %d consecutive failures",
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
		if (consecutive_failures > 0)
		{
			ramd_log_info("Primary health restored after %d failures",
			              consecutive_failures);
			consecutive_failures = 0;
		}
	}

	return false;
}

bool
ramd_monitor_handle_primary_failure(ramd_monitor_t *monitor __attribute__((unused)))
{
	return true;
}

bool
ramd_monitor_detect_role_changes(ramd_monitor_t *monitor __attribute__((unused)))
{
	return false;
}

bool
ramd_monitor_handle_role_change(ramd_monitor_t *monitor __attribute__((unused)),
                               ramd_role_t old_role __attribute__((unused)),
                               ramd_role_t new_role __attribute__((unused)))
{
	return true;
}

bool
ramd_monitor_sync_cluster_state(ramd_monitor_t *monitor __attribute__((unused)))
{
	return true;
}

bool
ramd_monitor_update_node_states(ramd_monitor_t *monitor __attribute__((unused)))
{
	return true;
}

bool
ramd_monitor_broadcast_state_change(ramd_monitor_t *monitor __attribute__((unused)),
                                   int32_t node_id __attribute__((unused)),
                                   ramd_node_state_t new_state __attribute__((unused)))
{
	return true;
}

float
ramd_monitor_calculate_node_health(ramd_monitor_t *monitor __attribute__((unused)),
                                  ramd_node_t *node __attribute__((unused)))
{
	return 1.0f;
}

bool
ramd_monitor_is_node_healthy(const ramd_monitor_t *monitor __attribute__((unused)),
                            const ramd_node_t *node __attribute__((unused)))
{
	return true;
}
