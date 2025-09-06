/*-------------------------------------------------------------------------
 *
 * quorum_selection.c
 *		Quorum selection using dstore and librale implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/elog.h"
#include "utils/timestamp.h"
#include "quorum_selection.h"
#include "pgram_librale.h"
#include "pgram_guc.h"

quorum_selection_config_t* quorum_selection_config = NULL;

bool quorum_selection_init(void)
{
	if (quorum_selection_config != NULL && quorum_selection_config->enabled)
		return true;

	quorum_selection_config =
	    (quorum_selection_config_t*) palloc0(sizeof(quorum_selection_config_t));

	if (quorum_selection_config == NULL)
	{
		elog(ERROR,
		     "pg_ram: Failed to allocate quorum selection configuration");
		return false;
	}

	/* Initialize configuration */
	quorum_selection_config->enabled = true;
	quorum_selection_config->node_id = (pgram_node_id > 0) ? pgram_node_id : 1;
	quorum_selection_config->dstore_port =
	    (pgram_dstore_port > 0) ? pgram_dstore_port : 7401;
	quorum_selection_config->consensus_timeout_ms = 10000; /* 10 seconds */
	quorum_selection_config->election_timeout_ms = 5000;   /* 5 seconds */
	quorum_selection_config->auto_failover_enabled = false;
	quorum_selection_config->health_threshold_for_leadership = 0.8; /* 80% */

	/* Set cluster name and dstore host */
	snprintf(quorum_selection_config->cluster_name,
	         sizeof(quorum_selection_config->cluster_name), "pg_ram_cluster");

	if (pgram_node_ip && pgram_node_ip[0] != '\0')
		snprintf(quorum_selection_config->dstore_host,
		         sizeof(quorum_selection_config->dstore_host), "%s",
		         pgram_node_ip);
	else
		snprintf(quorum_selection_config->dstore_host,
		         sizeof(quorum_selection_config->dstore_host), "");

	elog(LOG, "pg_ram: Quorum selection initialized - node_id: %d, cluster: %s",
	     quorum_selection_config->node_id,
	     quorum_selection_config->cluster_name);

	return true;
}

void quorum_selection_cleanup(void)
{
	if (quorum_selection_config == NULL)
		return;

	pfree(quorum_selection_config);
	quorum_selection_config = NULL;

	elog(LOG, "pg_ram: Quorum selection cleanup completed");
}

bool quorum_selection_register_node(int32_t node_id, const char* hostname,
                                    int32_t port)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (hostname == NULL)
		return false;

	/* Use librale to register the node */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		librale_status_t result = pgram_librale_add_node(
		    node_id, hostname, hostname, 7400, /* rale_port */
		    quorum_selection_config->dstore_port);
		if (result != LIBRALE_SUCCESS)
		{
			elog(WARNING, "pg_ram: Failed to register node %d via librale: %d",
			     node_id, result);
			return false;
		}
	}

	elog(LOG, "pg_ram: Node %d (%s:%d) registered for quorum selection",
	     node_id, hostname, port);
	return true;
}

bool quorum_selection_unregister_node(int32_t node_id)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	/* Use librale to remove the node */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		librale_status_t result = pgram_librale_remove_node(node_id);
		if (result != LIBRALE_SUCCESS)
		{
			elog(WARNING,
			     "pg_ram: Failed to unregister node %d via librale: %d",
			     node_id, result);
			return false;
		}
	}

	elog(LOG, "pg_ram: Node %d unregistered from quorum selection", node_id);
	return true;
}

bool quorum_selection_update_node_health(int32_t node_id, float health_score)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	/* Store health information for quorum decisions */
	/* In a real implementation, this would update dstore with health data */

	elog(DEBUG1, "pg_ram: Updated health score for node %d: %.2f", node_id,
	     health_score);
	return true;
}

bool quorum_selection_check_quorum(quorum_decision_t* decision_out)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (decision_out == NULL)
		return false;

	/* Initialize decision structure */
	memset(decision_out, 0, sizeof(quorum_decision_t));
	decision_out->decision_time = GetCurrentTimestamp();

	/* Check quorum via librale */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		decision_out->has_quorum = pgram_librale_has_quorum();
		decision_out->leader_node_id = pgram_librale_get_leader_id();
		decision_out->total_nodes = (int32_t) pgram_librale_get_node_count();
		decision_out->consensus_reached = decision_out->has_quorum;

		/* Calculate required votes (majority) */
		decision_out->votes_required = (decision_out->total_nodes / 2) + 1;
		decision_out->healthy_nodes =
		    decision_out->total_nodes; /* Simplified */
		decision_out->votes_received = decision_out->healthy_nodes;

		/* Set decision reason */
		if (decision_out->has_quorum)
		{
			snprintf(decision_out->decision_reason,
			         sizeof(decision_out->decision_reason),
			         "Quorum achieved with %d/%d nodes, leader: %d",
			         decision_out->healthy_nodes, decision_out->total_nodes,
			         decision_out->leader_node_id);
		}
		else
		{
			snprintf(decision_out->decision_reason,
			         sizeof(decision_out->decision_reason),
			         "No quorum - only %d/%d nodes available (need %d)",
			         decision_out->healthy_nodes, decision_out->total_nodes,
			         decision_out->votes_required);
		}
	}

	return true;
}

bool quorum_selection_is_leader(void)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	return pgram_librale_is_leader();
}

int32_t quorum_selection_get_leader_id(void)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return -1;

	return pgram_librale_get_leader_id();
}

bool quorum_selection_start_election(void)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	/* Trigger leader election via librale */
	elog(LOG, "pg_ram: Starting leader election for node %d",
	     quorum_selection_config->node_id);

	/* In a real implementation, this would trigger the election process */
	return true;
}

bool quorum_selection_become_leader(void)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	/* Check if we're eligible to become leader */
	if (!quorum_selection_should_trigger_failover())
		return false;

	elog(LOG, "pg_ram: Node %d attempting to become leader",
	     quorum_selection_config->node_id);

	/* Use librale consensus to become leader */
	return true;
}

bool quorum_selection_step_down_leader(void)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (!quorum_selection_is_leader())
		return false;

	elog(LOG, "pg_ram: Node %d stepping down as leader",
	     quorum_selection_config->node_id);

	return true;
}

bool quorum_selection_get_cluster_state(quorum_cluster_state_t* state_out)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (state_out == NULL)
		return false;

	/* Initialize cluster state */
	memset(state_out, 0, sizeof(quorum_cluster_state_t));

	snprintf(state_out->cluster_id, sizeof(state_out->cluster_id), "%s",
	         quorum_selection_config->cluster_name);
	state_out->generation = 1;
	state_out->is_stable = true;
	state_out->last_topology_change = GetCurrentTimestamp();

	/* Get cluster information from librale */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		state_out->node_count = (int32_t) pgram_librale_get_node_count();
		state_out->primary_count = pgram_librale_is_leader() ? 1 : 0;
		state_out->standby_count =
		    state_out->node_count - state_out->primary_count;
	}

	return true;
}

bool quorum_selection_should_trigger_failover(void)
{
	quorum_decision_t decision;

	if (!quorum_selection_check_quorum(&decision))
		return false;

	/* Trigger failover if we have quorum but no leader */
	return (decision.has_quorum && decision.leader_node_id <= 0);
}

char* quorum_selection_get_status_summary(void)
{
	quorum_decision_t decision;
	char* summary;

	if (!quorum_selection_check_quorum(&decision))
		return pstrdup("Quorum selection not available");

	summary = (char*) palloc(512);
	snprintf(summary, 512,
	         "Quorum Status: %s | Nodes: %d/%d healthy | Leader: %d | %s",
	         decision.has_quorum ? "QUORUM" : "NO QUORUM",
	         decision.healthy_nodes, decision.total_nodes,
	         decision.leader_node_id,
	         decision.consensus_reached ? "Consensus reached" : "No consensus");

	return summary;
}

bool quorum_selection_vote_for_leader(int32_t candidate_node_id)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (candidate_node_id <= 0)
		return false;

	/* Use librale to cast vote for the candidate */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		librale_status_t result =
		    pgram_librale_vote_for_leader(candidate_node_id);
		if (result != LIBRALE_SUCCESS)
		{
			elog(WARNING, "pg_ram: Failed to vote for leader candidate %d: %d",
			     candidate_node_id, result);
			return false;
		}

		elog(LOG, "pg_ram: Node %d voted for leader candidate %d",
		     quorum_selection_config->node_id, candidate_node_id);
		return true;
	}

	return false;
}

bool quorum_selection_propose_value(const char* key, const char* value)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (key == NULL || value == NULL)
		return false;

	/* Use dstore to propose consensus value */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		librale_status_t result = pgram_librale_propose_value(key, value);
		if (result != LIBRALE_SUCCESS)
		{
			elog(WARNING, "pg_ram: Failed to propose value for key '%s': %d",
			     key, result);
			return false;
		}

		elog(DEBUG1, "pg_ram: Proposed consensus value for key '%s'", key);
		return true;
	}

	return false;
}

bool quorum_selection_get_consensus_value(const char* key, char* value_out,
                                          size_t value_size)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (key == NULL || value_out == NULL || value_size == 0)
		return false;

	/* Get consensus value from dstore */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		char* consensus_value = pgram_librale_get_consensus_value(key);
		if (consensus_value == NULL)
		{
			elog(DEBUG1, "pg_ram: No consensus value found for key '%s'", key);
			return false;
		}

		/* Copy value with bounds checking */
		size_t len = strlen(consensus_value);
		if (len >= value_size)
		{
			elog(WARNING,
			     "pg_ram: Consensus value too large for buffer (need %zu, have "
			     "%zu)",
			     len + 1, value_size);
			pfree(consensus_value);
			return false;
		}

		strncpy(value_out, consensus_value, value_size - 1);
		value_out[value_size - 1] = '\0';

		pfree(consensus_value);
		elog(DEBUG1, "pg_ram: Retrieved consensus value for key '%s'", key);
		return true;
	}

	return false;
}

bool quorum_selection_delete_consensus_value(const char* key)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (key == NULL)
		return false;

	/* Delete consensus value from dstore */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		librale_status_t result = pgram_librale_delete_consensus_value(key);
		if (result != LIBRALE_SUCCESS)
		{
			elog(WARNING,
			     "pg_ram: Failed to delete consensus value for key '%s': %d",
			     key, result);
			return false;
		}

		elog(DEBUG1, "pg_ram: Deleted consensus value for key '%s'", key);
		return true;
	}

	return false;
}

bool quorum_selection_trigger_failover(int32_t target_node_id)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (!quorum_selection_config->auto_failover_enabled)
	{
		elog(WARNING, "pg_ram: Auto-failover is disabled");
		return false;
	}

	/* Check if current node is eligible to trigger failover */
	quorum_decision_t decision;
	if (!quorum_selection_check_quorum(&decision) || !decision.has_quorum)
	{
		elog(WARNING, "pg_ram: Cannot trigger failover - no quorum available");
		return false;
	}

	/* Trigger failover via librale */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		librale_status_t result =
		    pgram_librale_trigger_failover(target_node_id);
		if (result != LIBRALE_SUCCESS)
		{
			elog(ERROR, "pg_ram: Failed to trigger failover to node %d: %d",
			     target_node_id, result);
			return false;
		}

		elog(LOG, "pg_ram: Triggered failover from node %d to node %d",
		     quorum_selection_config->node_id, target_node_id);
		return true;
	}

	return false;
}

bool quorum_selection_detect_split_brain(void)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	/* Check for multiple leaders or conflicting cluster states */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		int32_t leader_count = pgram_librale_get_leader_count();
		if (leader_count > 1)
		{
			elog(WARNING,
			     "pg_ram: Split-brain detected - multiple leaders (%d)",
			     leader_count);
			return true;
		}

		/* Check for network partitions */
		if (pgram_librale_has_network_partition())
		{
			elog(WARNING, "pg_ram: Split-brain detected - network partition");
			return true;
		}
	}

	return false;
}

bool quorum_selection_resolve_split_brain(void)
{
	if (quorum_selection_config == NULL || !quorum_selection_config->enabled)
		return false;

	if (!quorum_selection_detect_split_brain())
		return true; /* No split-brain to resolve */

	/* Attempt to resolve split-brain condition */
	if (pg_ram_librale_config != NULL && pg_ram_librale_config->initialized)
	{
		/* Force new leader election */
		librale_status_t result = pgram_librale_force_leader_election();
		if (result != LIBRALE_SUCCESS)
		{
			elog(
			    ERROR,
			    "pg_ram: Failed to resolve split-brain via leader election: %d",
			    result);
			return false;
		}

		/* Wait for consensus to stabilize */
		pg_usleep(quorum_selection_config->consensus_timeout_ms * 1000);

		/* Verify split-brain is resolved */
		if (quorum_selection_detect_split_brain())
		{
			elog(ERROR, "pg_ram: Split-brain resolution failed");
			return false;
		}

		elog(LOG, "pg_ram: Split-brain successfully resolved");
		return true;
	}

	return false;
}

bool quorum_selection_is_healthy(void)
{
	quorum_decision_t decision;
	return quorum_selection_check_quorum(&decision) && decision.has_quorum;
}
