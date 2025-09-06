/*-------------------------------------------------------------------------
 *
 * quorum_selection.h
 *		Quorum selection using dstore and librale for consensus
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef QUORUM_SELECTION_H
#define QUORUM_SELECTION_H

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "librale.h"
#include "dstore.h"
#include "utils/timestamp.h"

/* Quorum configuration */
typedef struct
{
	bool enabled;
	int32_t node_id;
	char cluster_name[256];
	char dstore_host[256];
	int32_t dstore_port;
	int32_t consensus_timeout_ms;
	int32_t election_timeout_ms;
	bool auto_failover_enabled;
	float health_threshold_for_leadership;
} quorum_selection_config_t;

/* Node information for quorum */
typedef struct
{
	int32_t node_id;
	char hostname[256];
	int32_t port;
	bool is_healthy;
	bool is_leader;
	bool is_candidate;
	bool is_available;
	float health_score;
	timestamptz last_seen;
	int32_t term;
	pg_lsn last_wal_lsn;
} quorum_node_info_t;

/* Quorum decision result */
typedef struct
{
	bool has_quorum;
	bool consensus_reached;
	int32_t leader_node_id;
	char leader_hostname[256];
	int32_t leader_port;
	int32_t total_nodes;
	int32_t healthy_nodes;
	int32_t votes_received;
	int32_t votes_required;
	int32_t current_term;
	bool split_brain_detected;
	bool needs_election;
	timestamptz decision_time;
	char decision_reason[512];
} quorum_decision_t;

/* Cluster state for consensus */
typedef struct
{
	char cluster_id[256];
	int32_t generation;
	bool is_stable;
	int32_t primary_count;
	int32_t standby_count;
	int32_t failed_nodes;
	bool auto_failover_in_progress;
	timestamptz last_topology_change;
	quorum_node_info_t nodes[32]; /* Support up to 32 nodes */
	int32_t node_count;
} quorum_cluster_state_t;

extern quorum_selection_config_t* quorum_selection_config;

/* Quorum initialization */
extern bool quorum_selection_init(void);
extern void quorum_selection_cleanup(void);

/* Core quorum functions */
extern bool quorum_selection_register_node(int32_t node_id,
                                           const char* hostname, int32_t port);
extern bool quorum_selection_unregister_node(int32_t node_id);
extern bool quorum_selection_update_node_health(int32_t node_id,
                                                float health_score);
extern bool quorum_selection_check_quorum(quorum_decision_t* decision_out);

/* Leadership election */
extern bool quorum_selection_start_election(void);
extern bool quorum_selection_vote_for_leader(int32_t candidate_node_id);
extern bool quorum_selection_become_leader(void);
extern bool quorum_selection_step_down_leader(void);
extern bool quorum_selection_is_leader(void);
extern int32_t quorum_selection_get_leader_id(void);

/* Consensus operations using dstore + librale */
extern bool quorum_selection_propose_value(const char* key, const char* value);
extern bool quorum_selection_get_consensus_value(const char* key,
                                                 char* value_out,
                                                 size_t value_size);
extern bool quorum_selection_delete_consensus_value(const char* key);

/* Cluster management */
extern bool
quorum_selection_get_cluster_state(quorum_cluster_state_t* state_out);
extern bool quorum_selection_trigger_failover(int32_t target_node_id);
extern bool quorum_selection_detect_split_brain(void);
extern bool quorum_selection_resolve_split_brain(void);

/* Monitoring and status */
extern bool quorum_selection_is_healthy(void);
extern char* quorum_selection_get_status_summary(void);
extern bool quorum_selection_should_trigger_failover(void);

#endif /* QUORUM_SELECTION_H */
