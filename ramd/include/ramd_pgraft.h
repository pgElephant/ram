/*-------------------------------------------------------------------------
 *
 * ramd_pgraft.h
 *		PostgreSQL RAM Daemon - pgraft function declarations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_PGRAFT_H
#define RAMD_PGRAFT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Function declarations for pgraft functions used by ramd */
extern int pgraft_get_healthy_nodes_count(void);

#ifdef __cplusplus
}
#endif

#endif /* RAMD_PGRAFT_H */
