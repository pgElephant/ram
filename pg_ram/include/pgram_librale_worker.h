/*-------------------------------------------------------------------------
 *
 * pgram_librale_worker.h
 *   Background worker that owns librale and runs its loops
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *------------------------------------------------------------------------- */

#ifndef PGRAM_LIBRALE_WORKER_H
#define PGRAM_LIBRALE_WORKER_H

#include "postgres.h"

#define LIBRALE_WORKER_NAME "pg_ram librale worker"

void pgram_librale_worker_register(void);

#endif
