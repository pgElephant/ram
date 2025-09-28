/*
 * PostgreSQL background worker for pgraft network server
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#ifndef PGRaft_WORKER_H
#define PGRaft_WORKER_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/palloc.h"
#include "lib/ilist.h"
#include "nodes/pg_list.h"

typedef enum {
    WORKER_STATUS_IDLE = 0,
    WORKER_STATUS_INITIALIZING = 1,
    WORKER_STATUS_RUNNING = 2,
    WORKER_STATUS_STOPPING = 3,
    WORKER_STATUS_STOPPED = 4
} WORKER_STATUS;

/* Command types for worker queue */
typedef enum {
    COMMAND_INIT = 1,
    COMMAND_ADD_NODE = 2,
    COMMAND_REMOVE_NODE = 3,
    COMMAND_LOG_APPEND = 4,
    COMMAND_LOG_COMMIT = 5,
    COMMAND_LOG_APPLY = 6,
    COMMAND_SHUTDOWN = 7
} COMMAND_TYPE;

/* Command status enum */
typedef enum {
    COMMAND_STATUS_PENDING = 0,
    COMMAND_STATUS_PROCESSING = 1,
    COMMAND_STATUS_COMPLETED = 2,
    COMMAND_STATUS_FAILED = 3
} COMMAND_STATUS;

/* Command structure with status tracking */
typedef struct {
    COMMAND_TYPE type;
    int node_id;
    char address[256];
    int port;
    char cluster_id[256];
    /* Log operation fields */
    char log_data[1024];  /* For log append/commit data */
    int log_index;        /* For log operations */
    /* Status tracking */
    COMMAND_STATUS status;
    char error_message[512];  /* Error message if failed */
    int64_t timestamp;        /* Command timestamp */
} pgraft_command_t;

/* Command queue configuration */
#define MAX_COMMANDS 100

/* Background worker state structure */
typedef struct {
    int node_id;
    char address[256];
    int port;
    WORKER_STATUS status;
    
    /* Fixed-size circular buffer for commands */
    pgraft_command_t commands[MAX_COMMANDS];
    int command_head;  /* Index of next command to process */
    int command_tail;  /* Index of next slot to write */
    int command_count; /* Number of commands in queue */
    
    /* Fixed-size circular buffer for command status */
    pgraft_command_t status_commands[MAX_COMMANDS];
    int status_head;  /* Index of next status to read */
    int status_tail;  /* Index of next slot to write */
    int status_count; /* Number of status entries */
} pgraft_worker_state_t;

/* Background worker functions */
void pgraft_worker_main(Datum main_arg);
void pgraft_register_worker(void);

/* Worker control functions */
pgraft_worker_state_t *pgraft_worker_get_state(void);

/* Command queue functions */
bool pgraft_queue_command(COMMAND_TYPE type, int node_id, const char *address, int port, const char *cluster_id);
bool pgraft_queue_log_command(COMMAND_TYPE type, const char *log_data, int log_index);
bool pgraft_dequeue_command(pgraft_command_t *cmd);
bool pgraft_queue_is_empty(void);

/* Command status functions */
bool pgraft_add_command_to_status(pgraft_command_t *cmd);
bool pgraft_get_command_status(int64_t timestamp, pgraft_command_t *status_cmd);
bool pgraft_update_command_status(int64_t timestamp, COMMAND_STATUS status, const char *error_message);
bool pgraft_remove_completed_commands(void);

#endif /* PGRaft_WORKER_H */
