/*
 * PostgreSQL RAM Daemon - Multiple Synchronous Standbys Header
 * 
 * This header defines the API for multiple synchronous standbys support.
 */

#ifndef RAMD_SYNC_STANDBYS_H
#define RAMD_SYNC_STANDBYS_H

#include "ramd_common.h"

/* Maximum number of synchronous standbys */
#define RAMD_MAX_SYNC_STANDBYS 10

/* Function prototypes */

/*
 * Initialize synchronous standbys system
 */
bool ramd_sync_standbys_init(void);

/*
 * Add synchronous standby
 */
bool ramd_sync_standbys_add(const char* name, const char* hostname, int port, 
                           int priority, char* error_message, size_t error_size);

/*
 * Remove synchronous standby
 */
bool ramd_sync_standbys_remove(const char* name, char* error_message, size_t error_size);

/*
 * Set number of synchronous standbys
 */
bool ramd_sync_standbys_set_count(int count, char* error_message, size_t error_size);

/*
 * Enable ANY N configuration
 */
bool ramd_sync_standbys_enable_any_n(int min_sync, int max_sync, char* error_message, size_t error_size);

/*
 * Disable ANY N configuration
 */
bool ramd_sync_standbys_disable_any_n(char* error_message, size_t error_size);

/*
 * Set synchronous commit level
 */
bool ramd_sync_standbys_set_commit_level(const char* level, char* error_message, size_t error_size);

/*
 * Get synchronous standbys status
 */
bool ramd_sync_standbys_get_status(char* output, size_t output_size);

/*
 * Update standby status
 */
bool ramd_sync_standbys_update_status(const char* name, bool is_sync, int lag_ms);

/*
 * Get active synchronous standbys
 */
bool ramd_sync_standbys_get_active(char* output, size_t output_size);

/*
 * Check if standby is synchronous
 */
bool ramd_sync_standbys_is_sync(const char* name);

/*
 * Get standby priority
 */
int ramd_sync_standbys_get_priority(const char* name);

/*
 * Set standby priority
 */
bool ramd_sync_standbys_set_priority(const char* name, int priority, char* error_message, size_t error_size);

#endif /* RAMD_SYNC_STANDBYS_H */
