/*
 * PostgreSQL RAM Daemon - Multiple Synchronous Standbys Support
 * 
 * This module provides support for multiple synchronous standbys
 * with ANY N configuration and flexible synchronous replication.
 */

#include "ramd_sync_standbys.h"
#include "ramd_logging.h"
#include "ramd_postgresql.h"
#include "ramd_cluster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Synchronous standby configuration */
typedef struct {
    char name[64];
    char hostname[256];
    int port;
    int priority;
    bool enabled;
    time_t last_seen;
    bool is_sync;
    int lag_ms;
} ramd_sync_standby_t;

/* Synchronous replication configuration */
typedef struct {
    int num_sync_standbys;           /* Number of synchronous standbys required */
    char sync_standby_names[1024];   /* Comma-separated list of standby names */
    char sync_commit_level[32];      /* "on", "remote_write", "remote_apply" */
    bool any_n_enabled;              /* Enable ANY N configuration */
    int min_sync_standbys;           /* Minimum number of sync standbys */
    int max_sync_standbys;           /* Maximum number of sync standbys */
    ramd_sync_standby_t standbys[RAMD_MAX_SYNC_STANDBYS];
    int standby_count;
} ramd_sync_replication_t;

/* Global synchronous replication context */
static ramd_sync_replication_t g_sync_replication = {0};
static pthread_mutex_t g_sync_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Initialize synchronous standbys system
 */
bool
ramd_sync_standbys_init(void)
{
    ramd_log_info("Initializing synchronous standbys system");
    
    pthread_mutex_lock(&g_sync_mutex);
    
    /* Set default configuration */
    g_sync_replication.num_sync_standbys = 1;
    g_sync_replication.sync_commit_level[0] = '\0';
    strcpy(g_sync_replication.sync_commit_level, "on");
    g_sync_replication.any_n_enabled = false;
    g_sync_replication.min_sync_standbys = 1;
    g_sync_replication.max_sync_standbys = RAMD_MAX_SYNC_STANDBYS;
    g_sync_replication.standby_count = 0;
    
    pthread_mutex_unlock(&g_sync_mutex);
    
    ramd_log_success("Synchronous standbys system initialized");
    return true;
}

/*
 * Add synchronous standby
 */
bool
ramd_sync_standbys_add(const char* name, const char* hostname, int port, 
                      int priority, char* error_message, size_t error_size)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    /* Check if standby already exists */
    for (int i = 0; i < g_sync_replication.standby_count; i++) {
        if (strcmp(g_sync_replication.standbys[i].name, name) == 0) {
            snprintf(error_message, error_size, "Synchronous standby already exists: %s", name);
            pthread_mutex_unlock(&g_sync_mutex);
            return false;
        }
    }
    
    /* Check if we have room for more standbys */
    if (g_sync_replication.standby_count >= RAMD_MAX_SYNC_STANDBYS) {
        snprintf(error_message, error_size, "Maximum number of synchronous standbys reached");
        pthread_mutex_unlock(&g_sync_mutex);
        return false;
    }
    
    /* Add new standby */
    ramd_sync_standby_t* standby = &g_sync_replication.standbys[g_sync_replication.standby_count];
    strncpy(standby->name, name, sizeof(standby->name) - 1);
    strncpy(standby->hostname, hostname, sizeof(standby->hostname) - 1);
    standby->port = port;
    standby->priority = priority;
    standby->enabled = true;
    standby->last_seen = time(NULL);
    standby->is_sync = false;
    standby->lag_ms = 0;
    
    g_sync_replication.standby_count++;
    
    pthread_mutex_unlock(&g_sync_mutex);
    
    /* Update PostgreSQL configuration */
    if (!ramd_sync_standbys_update_postgresql_config()) {
        snprintf(error_message, error_size, "Failed to update PostgreSQL configuration");
        return false;
    }
    
    ramd_log_success("Synchronous standby added: %s (%s:%d)", name, hostname, port);
    return true;
}

/*
 * Remove synchronous standby
 */
bool
ramd_sync_standbys_remove(const char* name, char* error_message, size_t error_size)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    /* Find standby to remove */
    int standby_index = -1;
    for (int i = 0; i < g_sync_replication.standby_count; i++) {
        if (strcmp(g_sync_replication.standbys[i].name, name) == 0) {
            standby_index = i;
            break;
        }
    }
    
    if (standby_index == -1) {
        snprintf(error_message, error_size, "Synchronous standby not found: %s", name);
        pthread_mutex_unlock(&g_sync_mutex);
        return false;
    }
    
    /* Remove standby by shifting array */
    for (int i = standby_index; i < g_sync_replication.standby_count - 1; i++) {
        g_sync_replication.standbys[i] = g_sync_replication.standbys[i + 1];
    }
    g_sync_replication.standby_count--;
    
    pthread_mutex_unlock(&g_sync_mutex);
    
    /* Update PostgreSQL configuration */
    if (!ramd_sync_standbys_update_postgresql_config()) {
        snprintf(error_message, error_size, "Failed to update PostgreSQL configuration");
        return false;
    }
    
    ramd_log_success("Synchronous standby removed: %s", name);
    return true;
}

/*
 * Set number of synchronous standbys
 */
bool
ramd_sync_standbys_set_count(int count, char* error_message, size_t error_size)
{
    if (count < 1 || count > RAMD_MAX_SYNC_STANDBYS) {
        snprintf(error_message, error_size, "Invalid synchronous standby count: %d (must be 1-%d)", 
                count, RAMD_MAX_SYNC_STANDBYS);
        return false;
    }
    
    pthread_mutex_lock(&g_sync_mutex);
    g_sync_replication.num_sync_standbys = count;
    pthread_mutex_unlock(&g_sync_mutex);
    
    /* Update PostgreSQL configuration */
    if (!ramd_sync_standbys_update_postgresql_config()) {
        snprintf(error_message, error_size, "Failed to update PostgreSQL configuration");
        return false;
    }
    
    ramd_log_success("Synchronous standby count set to: %d", count);
    return true;
}

/*
 * Enable ANY N configuration
 */
bool
ramd_sync_standbys_enable_any_n(int min_sync, int max_sync, char* error_message, size_t error_size)
{
    if (min_sync < 1 || min_sync > max_sync || max_sync > RAMD_MAX_SYNC_STANDBYS) {
        snprintf(error_message, error_size, "Invalid ANY N configuration: min=%d, max=%d", min_sync, max_sync);
        return false;
    }
    
    pthread_mutex_lock(&g_sync_mutex);
    g_sync_replication.any_n_enabled = true;
    g_sync_replication.min_sync_standbys = min_sync;
    g_sync_replication.max_sync_standbys = max_sync;
    pthread_mutex_unlock(&g_sync_mutex);
    
    /* Update PostgreSQL configuration */
    if (!ramd_sync_standbys_update_postgresql_config()) {
        snprintf(error_message, error_size, "Failed to update PostgreSQL configuration");
        return false;
    }
    
    ramd_log_success("ANY N configuration enabled: min=%d, max=%d", min_sync, max_sync);
    return true;
}

/*
 * Disable ANY N configuration
 */
bool
ramd_sync_standbys_disable_any_n(char* error_message, size_t error_size)
{
    pthread_mutex_lock(&g_sync_mutex);
    g_sync_replication.any_n_enabled = false;
    pthread_mutex_unlock(&g_sync_mutex);
    
    /* Update PostgreSQL configuration */
    if (!ramd_sync_standbys_update_postgresql_config()) {
        snprintf(error_message, error_size, "Failed to update PostgreSQL configuration");
        return false;
    }
    
    ramd_log_success("ANY N configuration disabled");
    return true;
}

/*
 * Set synchronous commit level
 */
bool
ramd_sync_standbys_set_commit_level(const char* level, char* error_message, size_t error_size)
{
    if (strcmp(level, "on") != 0 && strcmp(level, "remote_write") != 0 && 
        strcmp(level, "remote_apply") != 0) {
        snprintf(error_message, error_size, "Invalid synchronous commit level: %s", level);
        return false;
    }
    
    pthread_mutex_lock(&g_sync_mutex);
    strncpy(g_sync_replication.sync_commit_level, level, sizeof(g_sync_replication.sync_commit_level) - 1);
    pthread_mutex_unlock(&g_sync_mutex);
    
    /* Update PostgreSQL configuration */
    if (!ramd_sync_standbys_update_postgresql_config()) {
        snprintf(error_message, error_size, "Failed to update PostgreSQL configuration");
        return false;
    }
    
    ramd_log_success("Synchronous commit level set to: %s", level);
    return true;
}

/*
 * Update PostgreSQL configuration
 */
bool
ramd_sync_standbys_update_postgresql_config(void)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    /* Build synchronous_standby_names */
    g_sync_replication.sync_standby_names[0] = '\0';
    
    if (g_sync_replication.any_n_enabled) {
        /* ANY N configuration */
        snprintf(g_sync_replication.sync_standby_names, 
                sizeof(g_sync_replication.sync_standby_names),
                "ANY %d (", g_sync_replication.min_sync_standbys);
        
        char* ptr = g_sync_replication.sync_standby_names + strlen(g_sync_replication.sync_standby_names);
        size_t remaining = sizeof(g_sync_replication.sync_standby_names) - strlen(g_sync_replication.sync_standby_names);
        
        for (int i = 0; i < g_sync_replication.standby_count; i++) {
            if (i > 0) {
                strncat(ptr, ", ", remaining - 1);
                ptr += 2;
                remaining -= 2;
            }
            strncat(ptr, g_sync_replication.standbys[i].name, remaining - 1);
            size_t len = strlen(g_sync_replication.standbys[i].name);
            ptr += len;
            remaining -= len;
        }
        
        strncat(ptr, ")", remaining - 1);
    } else {
        /* Fixed number configuration */
        int sync_count = 0;
        for (int i = 0; i < g_sync_replication.standby_count && sync_count < g_sync_replication.num_sync_standbys; i++) {
            if (g_sync_replication.standbys[i].enabled) {
                if (sync_count > 0) {
                    strncat(g_sync_replication.sync_standby_names, ", ", 
                           sizeof(g_sync_replication.sync_standby_names) - strlen(g_sync_replication.sync_standby_names) - 1);
                }
                strncat(g_sync_replication.sync_standby_names, g_sync_replication.standbys[i].name,
                       sizeof(g_sync_replication.sync_standby_names) - strlen(g_sync_replication.sync_standby_names) - 1);
                sync_count++;
            }
        }
    }
    
    pthread_mutex_unlock(&g_sync_mutex);
    
    /* Update PostgreSQL parameters */
    char error_message[512];
    bool success = true;
    
    /* Set synchronous_standby_names */
    if (!ramd_postgresql_set_parameter("synchronous_standby_names", 
                                      g_sync_replication.sync_standby_names, 
                                      error_message, sizeof(error_message))) {
        ramd_log_error("Failed to set synchronous_standby_names: %s", error_message);
        success = false;
    }
    
    /* Set synchronous_commit */
    if (!ramd_postgresql_set_parameter("synchronous_commit", 
                                      g_sync_replication.sync_commit_level, 
                                      error_message, sizeof(error_message))) {
        ramd_log_error("Failed to set synchronous_commit: %s", error_message);
        success = false;
    }
    
    return success;
}

/*
 * Get synchronous standbys status
 */
bool
ramd_sync_standbys_get_status(char* output, size_t output_size)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    char* ptr = output;
    size_t remaining = output_size;
    int written = 0;
    
    written = snprintf(ptr, remaining,
        "{\"sync_replication\":{\"num_sync_standbys\":%d,\"any_n_enabled\":%s,"
        "\"min_sync_standbys\":%d,\"max_sync_standbys\":%d,"
        "\"sync_commit_level\":\"%s\",\"sync_standby_names\":\"%s\","
        "\"standby_count\":%d,\"standbys\":[",
        g_sync_replication.num_sync_standbys,
        g_sync_replication.any_n_enabled ? "true" : "false",
        g_sync_replication.min_sync_standbys,
        g_sync_replication.max_sync_standbys,
        g_sync_replication.sync_commit_level,
        g_sync_replication.sync_standby_names,
        g_sync_replication.standby_count);
    
    if (written < 0 || (size_t)written >= remaining) {
        pthread_mutex_unlock(&g_sync_mutex);
        return false;
    }
    ptr += written;
    remaining -= written;
    
    for (int i = 0; i < g_sync_replication.standby_count; i++) {
        if (i > 0) {
            written = snprintf(ptr, remaining, ",");
            if (written < 0 || (size_t)written >= remaining) {
                pthread_mutex_unlock(&g_sync_mutex);
                return false;
            }
            ptr += written;
            remaining -= written;
        }
        
        written = snprintf(ptr, remaining,
            "{\"name\":\"%s\",\"hostname\":\"%s\",\"port\":%d,\"priority\":%d,"
            "\"enabled\":%s,\"is_sync\":%s,\"last_seen\":%ld,\"lag_ms\":%d}",
            g_sync_replication.standbys[i].name,
            g_sync_replication.standbys[i].hostname,
            g_sync_replication.standbys[i].port,
            g_sync_replication.standbys[i].priority,
            g_sync_replication.standbys[i].enabled ? "true" : "false",
            g_sync_replication.standbys[i].is_sync ? "true" : "false",
            g_sync_replication.standbys[i].last_seen,
            g_sync_replication.standbys[i].lag_ms);
        
        if (written < 0 || (size_t)written >= remaining) {
            pthread_mutex_unlock(&g_sync_mutex);
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    written = snprintf(ptr, remaining, "]}}");
    if (written < 0 || (size_t)written >= remaining) {
        pthread_mutex_unlock(&g_sync_mutex);
        return false;
    }
    
    pthread_mutex_unlock(&g_sync_mutex);
    return true;
}

/*
 * Update standby status
 */
bool
ramd_sync_standbys_update_status(const char* name, bool is_sync, int lag_ms)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    for (int i = 0; i < g_sync_replication.standby_count; i++) {
        if (strcmp(g_sync_replication.standbys[i].name, name) == 0) {
            g_sync_replication.standbys[i].is_sync = is_sync;
            g_sync_replication.standbys[i].lag_ms = lag_ms;
            g_sync_replication.standbys[i].last_seen = time(NULL);
            pthread_mutex_unlock(&g_sync_mutex);
            return true;
        }
    }
    
    pthread_mutex_unlock(&g_sync_mutex);
    return false;
}

/*
 * Get active synchronous standbys
 */
bool
ramd_sync_standbys_get_active(char* output, size_t output_size)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    char* ptr = output;
    size_t remaining = output_size;
    int written = 0;
    
    written = snprintf(ptr, remaining, "{\"active_sync_standbys\":[");
    if (written < 0 || (size_t)written >= remaining) {
        pthread_mutex_unlock(&g_sync_mutex);
        return false;
    }
    ptr += written;
    remaining -= written;
    
    int active_count = 0;
    for (int i = 0; i < g_sync_replication.standby_count; i++) {
        if (g_sync_replication.standbys[i].enabled && g_sync_replication.standbys[i].is_sync) {
            if (active_count > 0) {
                written = snprintf(ptr, remaining, ",");
                if (written < 0 || (size_t)written >= remaining) {
                    pthread_mutex_unlock(&g_sync_mutex);
                    return false;
                }
                ptr += written;
                remaining -= written;
            }
            
            written = snprintf(ptr, remaining,
                "{\"name\":\"%s\",\"hostname\":\"%s\",\"port\":%d,\"lag_ms\":%d}",
                g_sync_replication.standbys[i].name,
                g_sync_replication.standbys[i].hostname,
                g_sync_replication.standbys[i].port,
                g_sync_replication.standbys[i].lag_ms);
            
            if (written < 0 || (size_t)written >= remaining) {
                pthread_mutex_unlock(&g_sync_mutex);
                return false;
            }
            ptr += written;
            remaining -= written;
            active_count++;
        }
    }
    
    written = snprintf(ptr, remaining, "],\"count\":%d}", active_count);
    if (written < 0 || (size_t)written >= remaining) {
        pthread_mutex_unlock(&g_sync_mutex);
        return false;
    }
    
    pthread_mutex_unlock(&g_sync_mutex);
    return true;
}

/*
 * Check if standby is synchronous
 */
bool
ramd_sync_standbys_is_sync(const char* name)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    for (int i = 0; i < g_sync_replication.standby_count; i++) {
        if (strcmp(g_sync_replication.standbys[i].name, name) == 0) {
            bool is_sync = g_sync_replication.standbys[i].is_sync;
            pthread_mutex_unlock(&g_sync_mutex);
            return is_sync;
        }
    }
    
    pthread_mutex_unlock(&g_sync_mutex);
    return false;
}

/*
 * Get standby priority
 */
int
ramd_sync_standbys_get_priority(const char* name)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    for (int i = 0; i < g_sync_replication.standby_count; i++) {
        if (strcmp(g_sync_replication.standbys[i].name, name) == 0) {
            int priority = g_sync_replication.standbys[i].priority;
            pthread_mutex_unlock(&g_sync_mutex);
            return priority;
        }
    }
    
    pthread_mutex_unlock(&g_sync_mutex);
    return -1;
}

/*
 * Set standby priority
 */
bool
ramd_sync_standbys_set_priority(const char* name, int priority, char* error_message, size_t error_size)
{
    pthread_mutex_lock(&g_sync_mutex);
    
    for (int i = 0; i < g_sync_replication.standby_count; i++) {
        if (strcmp(g_sync_replication.standbys[i].name, name) == 0) {
            g_sync_replication.standbys[i].priority = priority;
            pthread_mutex_unlock(&g_sync_mutex);
            
            /* Update PostgreSQL configuration */
            if (!ramd_sync_standbys_update_postgresql_config()) {
                snprintf(error_message, error_size, "Failed to update PostgreSQL configuration");
                return false;
            }
            
            ramd_log_success("Standby priority updated: %s = %d", name, priority);
            return true;
        }
    }
    
    snprintf(error_message, error_size, "Standby not found: %s", name);
    pthread_mutex_unlock(&g_sync_mutex);
    return false;
}
