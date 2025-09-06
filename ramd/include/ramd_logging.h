/*-------------------------------------------------------------------------
 *
 * ramd_logging.h
 *		PostgreSQL Auto-Failover Daemon - Logging
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_LOGGING_H
#define RAMD_LOGGING_H

#include "ramd.h"

/* Log levels */
typedef enum ramd_log_level
{
	RAMD_LOG_LEVEL_DEBUG = 0,
	RAMD_LOG_LEVEL_INFO,
	RAMD_LOG_LEVEL_NOTICE,
	RAMD_LOG_LEVEL_WARNING,
	RAMD_LOG_LEVEL_ERROR,
	RAMD_LOG_LEVEL_FATAL
} ramd_log_level_t;

/* Logging configuration */
typedef struct ramd_logging_config
{
	ramd_log_level_t min_level;
	bool log_to_file;
	bool log_to_syslog;
	bool log_to_console;
	char log_file[RAMD_MAX_PATH_LENGTH];
	FILE* log_fp;
	bool initialized;
} ramd_logging_config_t;

/* Global logging configuration */
extern ramd_logging_config_t g_ramd_logging;

/* Logging initialization and cleanup */
extern bool ramd_logging_init(const char* log_file, ramd_log_level_t min_level,
                              bool log_to_file, bool log_to_syslog,
                              bool log_to_console);
extern void ramd_logging_cleanup(void);

/* Logging functions */
extern void ramd_log(ramd_log_level_t level, const char* file, int line,
                     const char* function, const char* format, ...);

/* Convenience macros - simplified for production use */
#define ramd_log_debug(...)                                                    \
	ramd_log(RAMD_LOG_LEVEL_DEBUG, "", 0, "", __VA_ARGS__)
#define ramd_log_info(...)                                                     \
	ramd_log(RAMD_LOG_LEVEL_INFO, "", 0, "", __VA_ARGS__)
#define ramd_log_notice(...)                                                   \
	ramd_log(RAMD_LOG_LEVEL_NOTICE, "", 0, "", __VA_ARGS__)
#define ramd_log_warning(...)                                                  \
	ramd_log(RAMD_LOG_LEVEL_WARNING, "", 0, "", __VA_ARGS__)
#define ramd_log_error(...)                                                    \
	ramd_log(RAMD_LOG_LEVEL_ERROR, "", 0, "", __VA_ARGS__)
#define ramd_log_fatal(...)                                                    \
	ramd_log(RAMD_LOG_LEVEL_FATAL, "", 0, "", __VA_ARGS__)

/* Log level utilities */
extern const char* ramd_logging_level_to_string(ramd_log_level_t level);
extern ramd_log_level_t ramd_logging_string_to_level(const char* level_str);

#endif /* RAMD_LOGGING_H */
