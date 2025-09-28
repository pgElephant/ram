/*
 * PostgreSQL background worker for pgraft network server
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/palloc.h"
#include "lib/ilist.h"
#include "nodes/pg_list.h"
#include "storage/ipc.h"
#include "storage/shmem.h"
#include "postmaster/bgworker.h"
#include "miscadmin.h"
#include <string.h>

#include "../include/pgraft_worker.h"
#include "../include/pgraft_go.h"
#include "../include/pgraft_guc.h"

/* Background worker registration moved to pgraft.c */
