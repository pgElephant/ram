/*-------------------------------------------------------------------------
 *
 * ramctrl_defaults.h
 *		Central definition of all default values for RAMCTRL
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_DEFAULTS_H
#define RAMCTRL_DEFAULTS_H

/* RAMD Daemon Defaults */
#define RAMCTRL_DEFAULT_RAMD_BINARY      "ramd"
#define RAMCTRL_DEFAULT_RAMD_PIDFILE     "/var/run/ramd.pid"
#define RAMCTRL_DEFAULT_RAMD_LOGFILE     "/var/log/ramd.log"
#define RAMCTRL_DEFAULT_RAMD_CONFIG      "/etc/ramd/ramd.conf"

/* PostgreSQL Connection Defaults */
#define RAMCTRL_DEFAULT_PG_DATABASE      "postgres"
#define RAMCTRL_DEFAULT_PG_USER          "postgres"
#define RAMCTRL_DEFAULT_PG_PORT          5432

/* HTTP API Defaults */
#define RAMCTRL_DEFAULT_API_PORT         8008

/* Command Timeouts */
#define RAMCTRL_DEFAULT_TIMEOUT_SECONDS  30

/* Replication Defaults */
#define RAMCTRL_DEFAULT_WAL_E_PATH       "/usr/local/bin/wal-e"
#define RAMCTRL_DEFAULT_BACKUP_DIR       "/var/lib/postgresql/backups"

/* Display Defaults */
#define RAMCTRL_DEFAULT_TABLE_WIDTH      80
#define RAMCTRL_DEFAULT_REFRESH_INTERVAL 5

/* Basic Size Constants */
#define RAMCTRL_MAX_HOSTNAME_LENGTH      256
#define RAMCTRL_MAX_PATH_LENGTH          512
#define RAMCTRL_MAX_COMMAND_LENGTH       1024
#define RAMCTRL_MAX_NODES                16

/* Compatibility macros for code that uses old names */
#define RAMD_PIDFILE                     RAMCTRL_DEFAULT_RAMD_PIDFILE
#define RAMD_BINARY                      RAMCTRL_DEFAULT_RAMD_BINARY
#define RAMD_LOGFILE                     RAMCTRL_DEFAULT_RAMD_LOGFILE

#endif /* RAMCTRL_DEFAULTS_H */
