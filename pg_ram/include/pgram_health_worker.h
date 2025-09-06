/*-------------------------------------------------------------------------
 *
 * pgram_health_worker.h
 *		Header for pg_ram health monitoring background worker
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGRAM_HEALTH_WORKER_H
#define PGRAM_HEALTH_WORKER_H

#include "postgres.h"
#include "pgram_health_types.h"

/* Worker configuration constants */
#define HEALTH_WORKER_NAME "pg_ram health worker"
/* Default sleep; can be overridden via GUC */
#define HEALTH_WORKER_SLEEP_MS 5000 /* 5 seconds */

/* Function declarations */
extern void pgram_health_worker_register(void);

/* Expose current status (from shared memory) */
extern void pgram_health_status_snapshot(health_worker_status_t* out);

/* Health check functions */
extern PgramHealthStatus pgram_health_check_cluster(void);
extern PgramHealthStatus pgram_health_check_quorum(void);
extern PgramHealthStatus pgram_health_check_consensus(void);

#endif /* PGRAM_HEALTH_WORKER_H */
