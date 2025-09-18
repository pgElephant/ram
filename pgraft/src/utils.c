/*
 * utils.c
 * Utility functions for pgraft extension
 *
 * This module provides essential utility functions for the pgraft extension.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 * All rights reserved.
 */

#include "postgres.h"
#include "../include/pgraft.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "utils/elog.h"
#include "utils/guc.h"
#include "lib/stringinfo.h"
#include <string.h>
#include <time.h>

/* Error handling functions */
const char*
pgraft_error_to_string(pgraft_error_code_t error)
{
    switch (error)
    {
        case PGRAFT_ERROR_NONE:
            return "No error";
        case PGRAFT_ERROR_NOT_INITIALIZED:
            return "pgraft not initialized";
        case PGRAFT_ERROR_ALREADY_INITIALIZED:
            return "pgraft already initialized";
        case PGRAFT_ERROR_INVALID_PARAMETER:
            return "Invalid parameter";
        case PGRAFT_ERROR_NOT_LEADER:
            return "Not leader";
        case PGRAFT_ERROR_NODE_NOT_FOUND:
            return "Node not found";
        case PGRAFT_ERROR_CLUSTER_FULL:
            return "Cluster is full";
        case PGRAFT_ERROR_NETWORK_ERROR:
            return "Network error";
        case PGRAFT_ERROR_TIMEOUT:
            return "Timeout";
        case PGRAFT_ERROR_INTERNAL:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

/* Logging functions */
void
pgraft_log_operation(int level, const char *operation, const char *details)
{
    StringInfoData buf;
    char *log_message;
    
    initStringInfo(&buf);
    appendStringInfo(&buf, "Operation: %s", operation);
    
    if (details)
    {
        appendStringInfo(&buf, ", Details: %s", details);
    }
    
    log_message = buf.data;
    
    switch (level)
    {
        case 0: /* DEBUG */
            elog(DEBUG1, "pgraft: %s", log_message);
            break;
        case 1: /* INFO */
            elog(INFO, "pgraft: %s", log_message);
            break;
        case 2: /* WARNING */
            elog(WARNING, "pgraft: %s", log_message);
            break;
        case 3: /* ERROR */
            elog(ERROR, "pgraft: %s", log_message);
            break;
        default:
            elog(LOG, "pgraft: %s", log_message);
            break;
    }
}

/* Validation functions */
bool
pgraft_validate_node_id(int node_id)
{
    return node_id > 0 && node_id <= PGRAFT_MAX_NODES;
}

bool
pgraft_validate_address(const char *address)
{
    if (!address)
        return false;
    
    if (strlen(address) == 0)
        return false;
    
    if (strlen(address) > 255)
        return false;
    
    return true;
}

bool
pgraft_validate_port(int port)
{
    return port > 0 && port <= 65535;
}

bool
pgraft_validate_term(int term)
{
    return term >= 0;
}

bool
pgraft_validate_log_index(int index)
{
    return index >= 0 && index <= PGRAFT_MAX_LOG_ENTRIES;
}