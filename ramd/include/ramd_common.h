/*-------------------------------------------------------------------------
 *
 * ramd_common.h
 *		Common definitions and utilities for RAMD
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_COMMON_H
#define RAMD_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Maximum lengths */
#define RAMD_MAX_HOSTNAME_LENGTH 256

/* Common status codes */
typedef enum {
    RAMD_STATUS_UNKNOWN = 0,
    RAMD_STATUS_HEALTHY,
    RAMD_STATUS_UNHEALTHY,
    RAMD_STATUS_MAINTENANCE,
    RAMD_STATUS_OFFLINE
} ramd_status_t;

/* Common error codes */
typedef enum {
    RAMD_ERROR_NONE = 0,
    RAMD_ERROR_INVALID_PARAM,
    RAMD_ERROR_MEMORY_ALLOCATION,
    RAMD_ERROR_FILE_OPERATION,
    RAMD_ERROR_NETWORK,
    RAMD_ERROR_DATABASE,
    RAMD_ERROR_TIMEOUT,
    RAMD_ERROR_NOT_FOUND,
    RAMD_ERROR_ALREADY_EXISTS,
    RAMD_ERROR_PERMISSION_DENIED,
    RAMD_ERROR_INTERNAL
} ramd_error_t;

/* Common utility functions */
extern const char* ramd_status_to_string(ramd_status_t status);
extern const char* ramd_error_to_string(ramd_error_t error);
extern bool ramd_is_valid_hostname(const char* hostname);
extern bool ramd_is_valid_port(int port);
extern time_t ramd_get_current_timestamp(void);
extern void ramd_sleep_ms(int milliseconds);

#endif /* RAMD_COMMON_H */
