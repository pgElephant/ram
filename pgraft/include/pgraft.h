/*
 * pgraft.h
 * PostgreSQL extension header for distributed consensus
 *
 * This file contains function declarations and data structures
 * for the pgraft PostgreSQL extension that provides distributed
 * consensus capabilities using a custom Raft implementation.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 * All rights reserved.
 */

#ifndef PGRAFT_H
#define PGRAFT_H

#include "postgres.h"
#include "fmgr.h"
#include "access/htup_details.h"
#include "access/tupdesc.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "nodes/pg_list.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "utils/guc.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "utils/elog.h"

/* Extension version */
#define PGRAFT_VERSION "1.0.0"
#define PGRAFT_VERSION_NUM 10000

/* Message structure */
typedef struct pgraft_message
{
    int msg_type;
    uint64_t from_node;
    uint64_t to_node;
    uint64_t term;
    uint64_t index;
    size_t data_size;
    char *data;
} pgraft_message_t;

/* Raft state enumeration */
typedef enum
{
    PGRAFT_STATE_FOLLOWER = 0,
    PGRAFT_STATE_CANDIDATE = 1,
    PGRAFT_STATE_LEADER = 2,
    PGRAFT_STATE_UNKNOWN = 3
} pgraft_state_t;

/* Health status enumeration */
typedef enum
{
    PGRAFT_HEALTH_OK = 0,
    PGRAFT_HEALTH_WARNING = 1,
    PGRAFT_HEALTH_ERROR = 2,
    PGRAFT_HEALTH_CRITICAL = 3
} pgraft_health_status_t;

typedef enum
{
    PGRAFT_HEALTH_EVENT_INFO,
    PGRAFT_HEALTH_EVENT_WARNING,
    PGRAFT_HEALTH_EVENT_ERROR
} pgraft_health_event_type_t;

/* Error codes */
typedef enum
{
    PGRAFT_ERROR_NONE = 0,
    PGRAFT_ERROR_NOT_INITIALIZED = 1,
    PGRAFT_ERROR_ALREADY_INITIALIZED = 2,
    PGRAFT_ERROR_INVALID_PARAMETER = 3,
    PGRAFT_ERROR_NOT_LEADER = 4,
    PGRAFT_ERROR_NODE_NOT_FOUND = 5,
    PGRAFT_ERROR_CLUSTER_FULL = 6,
    PGRAFT_ERROR_NETWORK_ERROR = 7,
    PGRAFT_ERROR_TIMEOUT = 8,
    PGRAFT_ERROR_INTERNAL = 9
} pgraft_error_code_t;

/* Raft state structure */
typedef struct
{
    pgraft_state_t state;
    int current_term;
    int voted_for;
    int leader_id;
    int last_log_index;
    int last_log_term;
    int commit_index;
    int last_applied;
    TimestampTz last_heartbeat;
    bool is_initialized;
    bool is_running;
} pgraft_raft_state_t;

/* Health worker status structure */
typedef struct
{
    bool is_running;
    TimestampTz last_activity;
    int health_checks_performed;
    int last_health_status;
    int warnings_count;
    int errors_count;
} pgraft_health_worker_status_t;

/* Core GUC variables */
extern int pgraft_node_id;
extern int pgraft_port;
extern char *pgraft_address;
extern int pgraft_log_level;
extern int pgraft_heartbeat_interval;
extern int pgraft_election_timeout;
extern bool pgraft_worker_enabled;
extern int pgraft_worker_interval;
extern char *pgraft_cluster_name;
extern int pgraft_cluster_size;
extern bool pgraft_enable_auto_cluster_formation;

/* Core Raft functions */
extern Datum pgraft_init(PG_FUNCTION_ARGS);
extern Datum pgraft_start(PG_FUNCTION_ARGS);
extern Datum pgraft_stop(PG_FUNCTION_ARGS);
extern Datum pgraft_add_node(PG_FUNCTION_ARGS);
extern Datum pgraft_remove_node(PG_FUNCTION_ARGS);
extern Datum pgraft_get_state(PG_FUNCTION_ARGS);
extern Datum pgraft_get_leader(PG_FUNCTION_ARGS);
extern Datum pgraft_get_nodes(PG_FUNCTION_ARGS);
extern Datum pgraft_get_log(PG_FUNCTION_ARGS);
extern Datum pgraft_get_stats(PG_FUNCTION_ARGS);
extern Datum pgraft_append_log(PG_FUNCTION_ARGS);
extern Datum pgraft_commit_log(PG_FUNCTION_ARGS);
extern Datum pgraft_read_log(PG_FUNCTION_ARGS);
extern Datum pgraft_version(PG_FUNCTION_ARGS);
extern Datum pgraft_is_leader(PG_FUNCTION_ARGS);
extern Datum pgraft_get_term(PG_FUNCTION_ARGS);

/* Monitoring functions */
extern Datum pgraft_get_cluster_health(PG_FUNCTION_ARGS);
extern Datum pgraft_get_performance_metrics(PG_FUNCTION_ARGS);
extern Datum pgraft_is_cluster_healthy(PG_FUNCTION_ARGS);
extern Datum pgraft_get_system_stats(PG_FUNCTION_ARGS);
extern Datum pgraft_get_quorum_status(PG_FUNCTION_ARGS);

/* Worker management functions */
extern void pgraft_worker_manager_init(void);
extern void pgraft_worker_manager_cleanup(void);
extern bool pgraft_worker_manager_get_status(pgraft_health_worker_status_t *status);
extern void pgraft_health_worker_init(void);
extern void pgraft_health_worker_start(void);
extern void pgraft_health_worker_stop(void);
extern void pgraft_health_worker_cleanup(void);
extern bool pgraft_health_worker_get_status(pgraft_health_worker_status_t *status);
extern bool pgraft_health_worker_check(void);
extern void pgraft_health_worker_main(Datum main_arg);
extern void pgraft_health_worker_register(void);
extern void pgraft_health_status_snapshot(pgraft_health_worker_status_t* out);

/* Communication functions */
extern int pgraft_send_message(uint64_t to_node, int msg_type, const char *data, size_t data_size, uint64_t term, uint64_t index);
extern int pgraft_broadcast_message(int msg_type, const char *data, size_t data_size, uint64_t term, uint64_t index);
extern int pgraft_add_node_comm(uint64_t node_id, const char *address, int port);
extern int pgraft_remove_node_comm(uint64_t node_id);

/* Monitor functions */
extern void pgraft_monitor_tick(void);
extern void pgraft_log_health_event(pgraft_health_event_type_t event_type, const char *message);

/* Utility functions */
extern char *pgraft_get_timestamp_string(void);
extern char *pgraft_format_memory_size(size_t bytes);
extern uint64_t pgraft_generate_node_id(void);
extern bool pgraft_is_string_empty(const char *str);
extern int pgraft_safe_strcpy(char *dest, const char *src, size_t dest_size);
extern const char *pgraft_bool_to_string(bool value);
extern bool pgraft_string_to_bool(const char *str);

/* Additional pgraft functions */
extern void pgraft_process_incoming_messages(void);
extern void pgraft_create_snapshot(void);
extern void pgraft_consensus_worker_main(Datum main_arg);

/* Core Raft implementation functions */
extern void pgraft_raft_init(void);
extern void pgraft_raft_start(void);
extern void pgraft_raft_stop(void);
extern pgraft_raft_state_t *pgraft_raft_get_state(void);
extern int pgraft_raft_add_node(uint64_t node_id, const char *address, int port);
extern int pgraft_raft_remove_node(uint64_t node_id);
extern int pgraft_raft_append_log(const char *data, size_t data_len);
extern int pgraft_raft_commit_log(long index);
extern int pgraft_raft_step_message(const char *data, size_t data_len);
extern char *pgraft_raft_get_stats(void);
extern char *pgraft_raft_get_nodes(void);
extern char *pgraft_raft_get_logs(void);
extern char *pgraft_raft_get_network_status(void);
extern void pgraft_raft_free_string(char *str);
extern void pgraft_start_election(void);
extern bool pgraft_process_vote_request(int candidate_id, int term, int last_log_index, int last_log_term);
extern bool pgraft_process_heartbeat(int leader_id, int term, int commit_index);
extern bool pgraft_append_log_entry(const char *command, int term);
extern void pgraft_commit_log_entries(void);
extern void pgraft_send_heartbeats(void);
extern void pgraft_consensus_loop(void);
extern void pgraft_raft_cleanup(void);
extern bool pgraft_is_initialized(void);
extern bool pgraft_is_healthy(void);
extern pgraft_health_status_t pgraft_get_health_status(void);

/* Configuration functions */
extern void pgraft_register_guc_variables(void);
extern void pgraft_validate_configuration(void);

/* Communication functions */
extern int pgraft_comm_init(const char *address, int port);
extern int pgraft_comm_shutdown(void);
extern bool pgraft_comm_initialized(void);
extern int pgraft_comm_get_active_connections(void);
extern pgraft_message_t *pgraft_comm_receive_message(void);
extern void pgraft_comm_free_message(pgraft_message_t *msg);
extern int pgraft_send_message(uint64_t to_node, int msg_type, const char *data, size_t data_size, uint64_t term, uint64_t index);
extern int pgraft_broadcast_message(int msg_type, const char *data, size_t data_size, uint64_t term, uint64_t index);
extern int pgraft_add_node_comm(uint64_t node_id, const char *address, int port);
extern int pgraft_remove_node_comm(uint64_t node_id);
extern bool pgraft_is_node_connected(uint64_t node_id);
extern int pgraft_get_connected_nodes_count(void);
extern int pgraft_get_healthy_nodes_count(void);

/* Monitoring functions */
extern void pgraft_monitor_init(void);
extern void pgraft_monitor_shutdown(void);
extern void pgraft_monitor_update_metrics(void);
extern void pgraft_monitor_check_health(void);

/* Metrics functions */
extern void pgraft_metrics_init(void);
extern void pgraft_metrics_record_request(bool success, int response_time_ms);
extern void pgraft_metrics_record_connection(bool connected);
extern void pgraft_metrics_record_heartbeat(void);
extern void pgraft_metrics_record_election(void);
extern void pgraft_metrics_record_log_append(void);
extern void pgraft_metrics_record_log_commit(void);
extern Datum pgraft_reset_metrics(PG_FUNCTION_ARGS);

/* Utility functions */
extern const char* pgraft_error_to_string(pgraft_error_code_t error);
extern void pgraft_log_operation(int level, const char *operation, const char *details);
extern bool pgraft_validate_node_id(int node_id);
extern bool pgraft_validate_address(const char *address);
extern bool pgraft_validate_port(int port);
extern bool pgraft_validate_term(int term);
extern bool pgraft_validate_log_index(int index);

/* Constants */
#define PGRAFT_MAX_NODES 100
#define PGRAFT_MAX_LOG_ENTRIES 10000
#define PGRAFT_MAX_MESSAGE_SIZE 8192
#define PGRAFT_DEFAULT_HEARTBEAT_INTERVAL 1000
#define PGRAFT_DEFAULT_ELECTION_TIMEOUT 5000

/* Macros for error handling */
#define PGRAFT_CHECK_INITIALIZED() \
    do { \
        if (!pgraft_is_initialized()) { \
            elog(ERROR, "pgraft: not initialized, call pgraft_init first"); \
        } \
    } while (0)

#define PGRAFT_CHECK_LEADER() \
    do { \
        if (!pgraft_is_leader()) { \
            elog(ERROR, "pgraft: operation requires leader role"); \
        } \
    } while (0)

#define PGRAFT_CHECK_PARAMETER(condition, message) \
    do { \
        if (!(condition)) { \
            elog(ERROR, "pgraft: " message); \
        } \
    } while (0)

/* ============================================================================
 * REPLICATION FUNCTIONS - Using etcd-io/raft patterns
 * ============================================================================ */

/* Replicate log entry to all nodes in the cluster */
extern int pgraft_replicate_log_entry(const char *data, size_t data_len);

/* Get replication status information */
extern char *pgraft_get_replication_info(void);

/* Create a snapshot for replication */
extern char *pgraft_create_replication_snapshot(void);

/* Apply a snapshot for replication */
extern int pgraft_apply_replication_snapshot(const char *snapshot_data);

/* Replicate data to a specific node */
extern int pgraft_replicate_to_node(uint64_t node_id, const char *data, size_t data_len);

/* Get replication lag in milliseconds */
extern double pgraft_get_replication_lag(void);

/* Force synchronization of replication */
extern int pgraft_sync_replication(void);

/* Free string returned by replication functions */
extern void pgraft_free_replication_string(char *str);

#endif /* PGRAFT_H */