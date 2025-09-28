/*
 * PostgreSQL utility functions for pgraft
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/palloc.h"
#include "lib/ilist.h"
#include "nodes/pg_list.h"
#include "../include/pgraft_worker.h"

#include <time.h>

/*
 * Add command to queue (called by SQL functions)
 */
bool
pgraft_queue_command(COMMAND_TYPE type, int node_id, const char *address, int port, const char *cluster_id)
{
	pgraft_worker_state_t *state;
	pgraft_command_t *cmd;
	
	elog(LOG, "pgraft: pgraft_queue_command called with type=%d, node_id=%d, address=%s, port=%d", 
		 type, node_id, address ? address : "NULL", port);
	
	state = pgraft_worker_get_state();
	if (state == NULL) {
		elog(ERROR, "pgraft: Failed to get worker state in pgraft_queue_command");
		return false;
	}
	
	/* Check if queue is full */
	if (state->command_count >= MAX_COMMANDS) {
		elog(WARNING, "pgraft: Command queue is full, cannot queue new command");
		return false;
	}
	
	/* Get pointer to next slot in circular buffer */
	cmd = &state->commands[state->command_tail];
	
	/* Initialize command */
	cmd->type = type;
	cmd->node_id = node_id;
	strncpy(cmd->address, address, sizeof(cmd->address) - 1);
	cmd->address[sizeof(cmd->address) - 1] = '\0';
	cmd->port = port;
	
	if (cluster_id) {
		strncpy(cmd->cluster_id, cluster_id, sizeof(cmd->cluster_id) - 1);
		cmd->cluster_id[sizeof(cmd->cluster_id) - 1] = '\0';
	} else {
		cmd->cluster_id[0] = '\0';
	}
	
	/* Initialize status tracking */
	cmd->status = COMMAND_STATUS_PENDING;
	cmd->error_message[0] = '\0';
	cmd->timestamp = time(NULL);
	
	/* Update circular buffer pointers */
	state->command_tail = (state->command_tail + 1) % MAX_COMMANDS;
	state->command_count++;
	
	elog(LOG, "pgraft: Command %d queued for node %d at %s:%d (count=%d)", 
		 type, node_id, address, port, state->command_count);
	return true;
}

/*
 * Remove command from queue (called by worker)
 * Returns true if command was dequeued, false if queue is empty
 */
bool
pgraft_dequeue_command(pgraft_command_t *cmd)
{
	pgraft_worker_state_t *state;
	
	state = pgraft_worker_get_state();
	if (state == NULL) {
		return false;
	}
	
	/* Check if queue is empty */
	if (state->command_count == 0) {
		return false;
	}
	
	/* Copy command from head of circular buffer */
	*cmd = state->commands[state->command_head];
	
	/* Update circular buffer pointers */
	state->command_head = (state->command_head + 1) % MAX_COMMANDS;
	state->command_count--;
	
	return true;
}

/*
 * Check if command queue is empty
 */
bool
pgraft_queue_is_empty(void)
{
	pgraft_worker_state_t *state;
	
	state = pgraft_worker_get_state();
	if (state == NULL) {
		return true;
	}
	
	return (state->command_count == 0);
}

/*
 * Add log command to queue (called by SQL log functions)
 */
bool
pgraft_queue_log_command(COMMAND_TYPE type, const char *log_data, int log_index)
{
	pgraft_worker_state_t *state;
	pgraft_command_t *cmd;
	
	state = pgraft_worker_get_state();
	if (state == NULL) {
		return false;
	}
	
	/* Check if queue is full */
	if (state->command_count >= MAX_COMMANDS) {
		elog(WARNING, "pgraft: Command queue is full, cannot queue log command");
		return false;
	}
	
	/* Get pointer to next slot in circular buffer */
	cmd = &state->commands[state->command_tail];
	
	/* Initialize command */
	cmd->type = type;
	cmd->node_id = 0;  /* Not applicable for log commands */
	cmd->address[0] = '\0';  /* Not applicable for log commands */
	cmd->port = 0;  /* Not applicable for log commands */
	cmd->cluster_id[0] = '\0';  /* Not applicable for log commands */
	
	/* Set log-specific fields */
	if (log_data) {
		strncpy(cmd->log_data, log_data, sizeof(cmd->log_data) - 1);
		cmd->log_data[sizeof(cmd->log_data) - 1] = '\0';
	} else {
		cmd->log_data[0] = '\0';
	}
	cmd->log_index = log_index;
	
	/* Initialize status tracking */
	cmd->status = COMMAND_STATUS_PENDING;
	cmd->error_message[0] = '\0';
	cmd->timestamp = time(NULL);
	
	/* Update circular buffer pointers */
	state->command_tail = (state->command_tail + 1) % MAX_COMMANDS;
	state->command_count++;
	
	elog(LOG, "pgraft: Log command %d queued (index=%d, count=%d)", type, log_index, state->command_count);
	return true;
}

/*
 * Add command to status tracking buffer
 */
bool
pgraft_add_command_to_status(pgraft_command_t *cmd)
{
	pgraft_worker_state_t *state;
	pgraft_command_t *status_cmd;
	
	state = pgraft_worker_get_state();
	if (state == NULL) {
		return false;
	}
	
	/* Check if status buffer is full */
	if (state->status_count >= MAX_COMMANDS) {
		elog(WARNING, "pgraft: Status buffer is full, removing oldest entry");
		/* Remove oldest status entry */
		state->status_head = (state->status_head + 1) % MAX_COMMANDS;
		state->status_count--;
	}
	
	/* Get pointer to next slot in status circular buffer */
	status_cmd = &state->status_commands[state->status_tail];
	
	/* Copy command to status buffer */
	*status_cmd = *cmd;
	
	/* Update circular buffer pointers */
	state->status_tail = (state->status_tail + 1) % MAX_COMMANDS;
	state->status_count++;
	
	return true;
}

/*
 * Get command status by timestamp
 */
bool
pgraft_get_command_status(int64_t timestamp, pgraft_command_t *status_cmd)
{
	pgraft_worker_state_t *state;
	int i;
	
	state = pgraft_worker_get_state();
	if (state == NULL) {
		return false;
	}
	
	/* Search through status buffer */
	for (i = 0; i < state->status_count; i++) {
		int index = (state->status_head + i) % MAX_COMMANDS;
		if (state->status_commands[index].timestamp == timestamp) {
			*status_cmd = state->status_commands[index];
			return true;
		}
	}
	
	return false;
}

/*
 * Update command status in status buffer
 */
bool
pgraft_update_command_status(int64_t timestamp, COMMAND_STATUS status, const char *error_message)
{
	pgraft_worker_state_t *state;
	int i;
	
	state = pgraft_worker_get_state();
	if (state == NULL) {
		return false;
	}
	
	/* Search through status buffer */
	for (i = 0; i < state->status_count; i++) {
		int index = (state->status_head + i) % MAX_COMMANDS;
		if (state->status_commands[index].timestamp == timestamp) {
			state->status_commands[index].status = status;
			if (error_message) {
				strncpy(state->status_commands[index].error_message, error_message, 
						sizeof(state->status_commands[index].error_message) - 1);
				state->status_commands[index].error_message[sizeof(state->status_commands[index].error_message) - 1] = '\0';
			}
			return true;
		}
	}
	
	return false;
}

/*
 * Remove completed commands from status buffer
 */
bool
pgraft_remove_completed_commands(void)
{
	pgraft_worker_state_t *state;
	int removed = 0;
	int i;
	
	state = pgraft_worker_get_state();
	if (state == NULL) {
		return false;
	}
	
	/* Remove completed and failed commands from status buffer */
	for (i = 0; i < state->status_count; i++) {
		int index = (state->status_head + i) % MAX_COMMANDS;
		COMMAND_STATUS status = state->status_commands[index].status;
		
		if (status == COMMAND_STATUS_COMPLETED || status == COMMAND_STATUS_FAILED) {
			removed++;
		} else {
			/* Move command to front if we removed some */
			if (removed > 0) {
				int new_index = (state->status_head + i - removed) % MAX_COMMANDS;
				state->status_commands[new_index] = state->status_commands[index];
			}
		}
	}
	
	/* Update status buffer pointers */
	state->status_head = (state->status_head + removed) % MAX_COMMANDS;
	state->status_count -= removed;
	
	if (removed > 0) {
		elog(LOG, "pgraft: Removed %d completed commands from status buffer", removed);
	}
	
	return true;
}
