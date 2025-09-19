/*-------------------------------------------------------------------------
 *
 * ramd_basebackup.h
 *		PostgreSQL RAM Daemon - Base backup function declarations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * This file provides function declarations for base backup functionality
 * in the ramd daemon. It provides functions to take and manage base
 * backups using pg_basebackup.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_BASEBACKUP_H
#define RAMD_BASEBACKUP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libpq-fe.h>

/*
 * Take a base backup using pg_basebackup
 * 
 * Parameters:
 *   conn - PostgreSQL connection
 *   target_dir - Directory where backup should be stored
 *   label - Label for the backup
 * 
 * Returns: 0 on success, -1 on failure
 */
extern int ramd_take_basebackup(PGconn *conn, const char *target_dir, const char *label);

#ifdef __cplusplus
}
#endif

#endif /* RAMD_BASEBACKUP_H */
