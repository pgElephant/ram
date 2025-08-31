/*-------------------------------------------------------------------------
 *
 * ram_config_system.h
 *		RAM configuration system with OS detection and platform-specific defaults
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAM_CONFIG_SYSTEM_H
#define RAM_CONFIG_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

/*-------------------------------------------------------------------------
 * Configuration System Types
 *-------------------------------------------------------------------------*/

typedef enum {
    RAM_CONFIG_TYPE_DAEMON = 0,
    RAM_CONFIG_TYPE_CLUSTER,
    RAM_CONFIG_TYPE_DATABASE,
    RAM_CONFIG_TYPE_LOGGING,
    RAM_CONFIG_TYPE_NETWORK,
    RAM_CONFIG_TYPE_SECURITY,
    RAM_CONFIG_TYPE_PERFORMANCE
} ram_config_type_t;

typedef struct {
    char                *key;
    char                *value;
    ram_config_type_t   type;
    bool                required;
    bool                dynamic;
    char                *description;
    char                *default_value;
} ram_config_option_t;

typedef struct {
    char                *config_file;
    char                *log_file;
    char                *pid_file;
    char                *data_directory;
    char                *socket_directory;
    char                *working_directory;
    bool                daemon_mode;
    bool                foreground_mode;
    int                 log_level;
    int                 max_connections;
    int                 timeout_seconds;
    char                *user;
    char                *group;
} ram_config_context_t;

/*-------------------------------------------------------------------------
 * Platform Detection and Defaults
 *-------------------------------------------------------------------------*/

/**
 * Get the operating system name
 */
const char *ram_get_os_name(void);

/**
 * Get the operating system family
 */
const char *ram_get_os_family(void);

/**
 * Get the architecture name
 */
const char *ram_get_arch_name(void);

/**
 * Check if running on macOS
 */
bool ram_is_macos(void);

/**
 * Check if running on Linux
 */
bool ram_is_linux(void);

/**
 * Check if running on Windows
 */
bool ram_is_windows(void);

/**
 * Check if running on a Unix-like system
 */
bool ram_is_unix(void);

/*-------------------------------------------------------------------------
 * Platform-Specific Paths
 *-------------------------------------------------------------------------*/

/**
 * Get platform-specific default configuration directory
 */
const char *ram_get_default_config_dir(void);

/**
 * Get platform-specific default log directory
 */
const char *ram_get_default_log_dir(void);

/**
 * Get platform-specific default data directory
 */
const char *ram_get_default_data_dir(void);

/**
 * Get platform-specific default PID directory
 */
const char *ram_get_default_pid_dir(void);

/**
 * Get platform-specific default socket directory
 */
const char *ram_get_default_socket_dir(void);

/**
 * Get platform-specific default working directory
 */
const char *ram_get_default_working_dir(void);

/*-------------------------------------------------------------------------
 * Platform-Specific Features
 *-------------------------------------------------------------------------*/

/**
 * Check if Unix domain sockets are supported
 */
bool ram_has_unix_sockets(void);

/**
 * Check if systemd is available (Linux)
 */
bool ram_has_systemd(void);

/**
 * Check if launchd is available (macOS)
 */
bool ram_has_launchd(void);

/**
 * Check if watchdog is available (Linux)
 */
bool ram_has_watchdog(void);

/**
 * Check if a specific feature is available
 */
bool ram_has_feature(const char *feature);

/*-------------------------------------------------------------------------
 * Configuration Management
 *-------------------------------------------------------------------------*/

/**
 * Initialize configuration system
 */
int ram_config_init(ram_config_context_t *ctx);

/**
 * Load configuration from file
 */
int ram_config_load(ram_config_context_t *ctx, const char *config_file);

/**
 * Load configuration with platform-specific defaults
 */
int ram_config_load_with_defaults(ram_config_context_t *ctx, const char *config_file);

/**
 * Get configuration value
 */
const char *ram_config_get(ram_config_context_t *ctx, const char *key);

/**
 * Set configuration value
 */
int ram_config_set(ram_config_context_t *ctx, const char *key, const char *value);

/**
 * Validate configuration
 */
int ram_config_validate(ram_config_context_t *ctx);

/**
 * Get configuration as JSON string
 */
int ram_config_to_json(ram_config_context_t *ctx, char *json_buffer, size_t buffer_size);

/**
 * Clean up configuration system
 */
void ram_config_cleanup(ram_config_context_t *ctx);

/*-------------------------------------------------------------------------
 * Platform-Specific Configuration
 *-------------------------------------------------------------------------*/

/**
 * Get platform-specific daemon configuration
 */
int ram_get_daemon_config(ram_config_context_t *ctx);

/**
 * Get platform-specific service configuration
 */
int ram_get_service_config(ram_config_context_t *ctx);

/**
 * Get platform-specific logging configuration
 */
int ram_get_logging_config(ram_config_context_t *ctx);

/**
 * Get platform-specific network configuration
 */
int ram_get_network_config(ram_config_context_t *ctx);

/*-------------------------------------------------------------------------
 * Utility Functions
 *-------------------------------------------------------------------------*/

/**
 * Expand environment variables in configuration values
 */
int ram_config_expand_env(ram_config_context_t *ctx);

/**
 * Resolve relative paths to absolute paths
 */
int ram_config_resolve_paths(ram_config_context_t *ctx);

/**
 * Create necessary directories
 */
int ram_config_create_directories(ram_config_context_t *ctx);

/**
 * Check file permissions
 */
int ram_config_check_permissions(ram_config_context_t *ctx);

#endif /* RAM_CONFIG_SYSTEM_H */
