/*-------------------------------------------------------------------------
 *
 * utils.c
 *		Utility functions for pgraft extension
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
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

/*
 * Get current timestamp as string
 */
char *
pgraft_get_timestamp_string(void)
{
    static char timestamp_str[64];
    TimestampTz now = GetCurrentTimestamp();
    
    snprintf(timestamp_str, sizeof(timestamp_str), "%lld", (long long)now);
    return timestamp_str;
}

/*
 * Format memory size
 */
char *
pgraft_format_memory_size(size_t bytes)
{
    static char size_str[32];
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = (double)bytes;
    
    while (size >= 1024 && unit < 4)
    {
        size /= 1024;
        unit++;
    }
    
    snprintf(size_str, sizeof(size_str), "%.1f %s", size, units[unit]);
    return size_str;
}

/*
 * Generate unique node ID
 */
uint64_t
pgraft_generate_node_id(void)
{
    static uint64_t node_counter = 1;
    return node_counter++;
}

/*
 * Check if string is empty or null
 */
bool
pgraft_is_string_empty(const char *str)
{
    return str == NULL || strlen(str) == 0;
}

/*
 * Safe string copy
 */
int
pgraft_safe_strcpy(char *dest, const char *src, size_t dest_size)
{
    if (dest == NULL || src == NULL || dest_size == 0)
        return -1;
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    
    return 0;
}

/*
 * Convert boolean to string
 */
const char *
pgraft_bool_to_string(bool value)
{
    return value ? "true" : "false";
}

/*
 * Parse boolean from string
 */
bool
pgraft_string_to_bool(const char *str)
{
    if (str == NULL)
        return false;
    
    if (strcasecmp(str, "true") == 0 || strcasecmp(str, "1") == 0 || strcasecmp(str, "yes") == 0)
        return true;
    
    return false;
}