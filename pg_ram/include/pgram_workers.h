/*-------------------------------------------------------------------------
 *
 * pgram_workers.h
 *		Header for pg_ram background workers
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGRAM_WORKERS_H
#define PGRAM_WORKERS_H

#include "postgres.h"
#include "lib/stringinfo.h"

/* Function prototypes */
extern void pgram_worker_manager_init(void);
extern void pgram_worker_manager_cleanup(void);
extern void pgram_worker_manager_get_status(StringInfo buf);

#endif /* PGRAM_WORKERS_H */
