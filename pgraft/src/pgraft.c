/*-------------------------------------------------------------------------
 *
 * pgraft.c
 *      Main pgraft extension file with clean modular architecture
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "storage/shmem.h"
#include "utils/elog.h"

#include "../include/pgraft_core.h"
#include "../include/pgraft_go.h"
#include "../include/pgraft_state.h"
#include "../include/pgraft_log.h"
#include "../include/pgraft_guc.h"

/* Temporary function declarations to fix compilation */
bool		pgraft_go_is_loaded(void);
void		pgraft_go_unload_library(void);

PG_MODULE_MAGIC;

/* Extension version */
#define PGRAFT_VERSION "1.0.0"

/* Shared memory request hook */
static shmem_request_hook_type prev_shmem_request_hook = NULL;

/*
 * Shared memory request hook
 */
static void
pgraft_shmem_request_hook(void)
{
	if (prev_shmem_request_hook)
		prev_shmem_request_hook();
	
	/* Request shared memory for core system */
	RequestAddinShmemSpace(sizeof(pgraft_cluster_t));
	
	/* Request shared memory for Go state persistence */
	RequestAddinShmemSpace(sizeof(pgraft_go_state_t));
	
	/* Request shared memory for log replication */
	RequestAddinShmemSpace(sizeof(pgraft_log_state_t));
}


/*
 * Extension initialization
 */
void
_PG_init(void)
{
	elog(INFO, "pgraft: Initializing extension version %s", PGRAFT_VERSION);
	
	/* Install shared memory request hook */
	prev_shmem_request_hook = shmem_request_hook;
	shmem_request_hook = pgraft_shmem_request_hook;
	
	/* Register GUC variables */
	pgraft_register_guc_variables();
	
	elog(INFO, "pgraft: Extension initialized successfully");
}

/*
 * Extension cleanup
 */
void
_PG_fini(void)
{
	elog(INFO, "pgraft: Cleaning up extension");
	
	/* Unload Go library if loaded */
	if (pgraft_go_is_loaded())
		pgraft_go_unload_library();
	
	/* Cleanup core system */
	pgraft_core_cleanup();
	
	/* Restore shared memory request hook */
	shmem_request_hook = prev_shmem_request_hook;
	
	elog(INFO, "pgraft: Extension cleanup completed");
}