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
#define RAMCTRL_DEFAULT_RAMD_PIDFILE     NULL	/* Will be set from config */
#define RAMCTRL_DEFAULT_RAMD_LOGFILE     NULL	/* Will be set from config */
#define RAMCTRL_DEFAULT_RAMD_CONFIG      NULL	/* Will be set from config */

/* PostgreSQL Connection Defaults */
#define RAMCTRL_DEFAULT_PG_DATABASE      NULL	/* Will be set from config */
#define RAMCTRL_DEFAULT_PG_USER          NULL	/* Will be set from config */
#define RAMCTRL_DEFAULT_PG_PORT          0		/* Will be set from config */

/* HTTP API Defaults */
#define RAMCTRL_DEFAULT_API_PORT         0		/* Will be set from config */

/* Command Timeouts */
#define RAMCTRL_DEFAULT_TIMEOUT_SECONDS  30

/* Replication Defaults */
#define RAMCTRL_DEFAULT_WAL_E_PATH       NULL	/* Will be set from config */
#define RAMCTRL_DEFAULT_BACKUP_DIR       NULL	/* Will be set from config */

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
