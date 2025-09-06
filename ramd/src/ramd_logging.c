/*-------------------------------------------------------------------------
 *
 * ramd_logging.c
 *		PostgreSQL Auto-Failover Daemon - Logging Implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdarg.h>
#include <syslog.h>

#include "ramd_logging.h"

/* Global logging configuration */
ramd_logging_config_t g_ramd_logging = {0};

bool ramd_logging_init(const char* log_file, ramd_log_level_t min_level,
                       bool log_to_file, bool log_to_syslog,
                       bool log_to_console)
{
	memset(&g_ramd_logging, 0, sizeof(g_ramd_logging));

	g_ramd_logging.min_level = min_level;
	g_ramd_logging.log_to_file = log_to_file;
	g_ramd_logging.log_to_syslog = log_to_syslog;
	g_ramd_logging.log_to_console = log_to_console;

	/* Initialize file logging */
	if (log_to_file && log_file && strlen(log_file) > 0)
	{
		strncpy(g_ramd_logging.log_file, log_file,
		        sizeof(g_ramd_logging.log_file) - 1);

		g_ramd_logging.log_fp = fopen(log_file, "a");
		if (!g_ramd_logging.log_fp)
		{
			fprintf(
			    stderr,
			    "Critical error - Unable to open log file for writing: %s\n",
			    log_file);
			return false;
		}
	}

	/* Initialize syslog */
	if (log_to_syslog)
	{
		openlog("ramd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
	}

	g_ramd_logging.initialized = true;
	return true;
}


void ramd_logging_cleanup(void)
{
	if (!g_ramd_logging.initialized)
		return;

	/* Close log file */
	if (g_ramd_logging.log_fp)
	{
		fclose(g_ramd_logging.log_fp);
		g_ramd_logging.log_fp = NULL;
	}

	/* Close syslog */
	if (g_ramd_logging.log_to_syslog)
	{
		closelog();
	}

	g_ramd_logging.initialized = false;
}


void ramd_log(ramd_log_level_t level, const char* file, int line,
              const char* function, const char* format, ...)
{
	va_list args;
	char timestamp[64];
	char message[RAMD_MAX_LOG_MESSAGE];
	char full_message[RAMD_MAX_LOG_MESSAGE + 256];
	time_t now;
	struct tm* tm_info;

	if (!g_ramd_logging.initialized || level < g_ramd_logging.min_level)
		return;

	/* Format the message */
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	/* Get timestamp */
	time(&now);
	tm_info = localtime(&now);
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

	/* Create full message with context */
	if (file && strlen(file) > 0 && line > 0)
	{
		/* Debug format with file/line/function */
		snprintf(full_message, sizeof(full_message), "%s [%s] %s:%d %s(): %s",
		         timestamp, ramd_logging_level_to_string(level), file, line,
		         function, message);
	}
	else
	{
		/* Production format - clean and meaningful */
		snprintf(full_message, sizeof(full_message), "%s [%s] %s", timestamp,
		         ramd_logging_level_to_string(level), message);
	}

	/* Log to console */
	if (g_ramd_logging.log_to_console)
	{
		FILE* output = (level >= RAMD_LOG_LEVEL_WARNING) ? stderr : stdout;
		fprintf(output, "%s\n", full_message);
		fflush(output);
	}

	/* Log to file */
	if (g_ramd_logging.log_to_file && g_ramd_logging.log_fp)
	{
		fprintf(g_ramd_logging.log_fp, "%s\n", full_message);
		fflush(g_ramd_logging.log_fp);
	}

	/* Log to syslog */
	if (g_ramd_logging.log_to_syslog)
	{
		int syslog_priority;

		switch (level)
		{
		case RAMD_LOG_LEVEL_DEBUG:
			syslog_priority = LOG_DEBUG;
			break;
		case RAMD_LOG_LEVEL_INFO:
			syslog_priority = LOG_INFO;
			break;
		case RAMD_LOG_LEVEL_NOTICE:
			syslog_priority = LOG_NOTICE;
			break;
		case RAMD_LOG_LEVEL_WARNING:
			syslog_priority = LOG_WARNING;
			break;
		case RAMD_LOG_LEVEL_ERROR:
			syslog_priority = LOG_ERR;
			break;
		case RAMD_LOG_LEVEL_FATAL:
			syslog_priority = LOG_CRIT;
			break;
		default:
			syslog_priority = LOG_INFO;
			break;
		}

		syslog(syslog_priority, "%s", message);
	}
}


const char* ramd_logging_level_to_string(ramd_log_level_t level)
{
	switch (level)
	{
	case RAMD_LOG_LEVEL_DEBUG:
		return "DEBUG";
	case RAMD_LOG_LEVEL_INFO:
		return "INFO";
	case RAMD_LOG_LEVEL_NOTICE:
		return "NOTICE";
	case RAMD_LOG_LEVEL_WARNING:
		return "WARNING";
	case RAMD_LOG_LEVEL_ERROR:
		return "ERROR";
	case RAMD_LOG_LEVEL_FATAL:
		return "FATAL";
	default:
		return "UNKNOWN";
	}
}


ramd_log_level_t ramd_logging_string_to_level(const char* level_str)
{
	if (!level_str)
		return RAMD_LOG_LEVEL_INFO;

	if (strcasecmp(level_str, "debug") == 0)
		return RAMD_LOG_LEVEL_DEBUG;
	else if (strcasecmp(level_str, "info") == 0)
		return RAMD_LOG_LEVEL_INFO;
	else if (strcasecmp(level_str, "notice") == 0)
		return RAMD_LOG_LEVEL_NOTICE;
	else if (strcasecmp(level_str, "warning") == 0)
		return RAMD_LOG_LEVEL_WARNING;
	else if (strcasecmp(level_str, "error") == 0)
		return RAMD_LOG_LEVEL_ERROR;
	else if (strcasecmp(level_str, "fatal") == 0)
		return RAMD_LOG_LEVEL_FATAL;
	else
		return RAMD_LOG_LEVEL_INFO;
}
