/*-------------------------------------------------------------------------
 *
 * pg_ram.c
 *		PostgreSQL extension for RAM with librale distributed consensus
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "commands/dbcommands.h"
#include "postmaster/postmaster.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "tcop/utility.h"
#include "pgram_librale.h"
#include "pgram_workers.h"
#include "pgram_guc.h"
#include "postgresql_monitor.h"
#include "postgresql_healthcheck.h"
#include "quorum_selection.h"

PG_MODULE_MAGIC;

/* Hooks */
static ProcessUtility_hook_type prev_ProcessUtility_hook = NULL;

/* Function prototypes */
void		_PG_init(void);
void		_PG_fini(void);
static void StartExtensionNode(void);
static void pg_ram_ProcessUtility(PlannedStmt *pstmt,
								  const char *queryString,
								  bool readOnlyTree,
								  ProcessUtilityContext context,
								  ParamListInfo params,
								  struct QueryEnvironment *queryEnv,
								  DestReceiver *dest,
								  QueryCompletion *completionTag);

void
_PG_init(void)
{
	if (!process_shared_preload_libraries_in_progress)
	{
		ereport(ERROR,
				(errmsg("pg_ram can only be loaded via shared_preload_libraries"),
				 errhint("Add pg_ram to shared_preload_libraries "
						 "configuration variable in postgresql.conf.")));
	}

	prev_ProcessUtility_hook = ProcessUtility_hook;
	ProcessUtility_hook = pg_ram_ProcessUtility;

	pgram_guc_init();
	StartExtensionNode();
	pgram_worker_manager_init();
	postgresql_monitor_init();
	postgresql_healthcheck_init();
	quorum_selection_init();

	ereport(LOG, (errmsg("pg_ram: _PG_init completed successfully.")));
}

void
_PG_fini(void)
{
	pgram_worker_manager_cleanup();
	quorum_selection_cleanup();
	postgresql_healthcheck_cleanup();
	postgresql_monitor_cleanup();
	pgram_librale_cleanup();

	if (ProcessUtility_hook == pg_ram_ProcessUtility)
		ProcessUtility_hook = prev_ProcessUtility_hook;

	ereport(LOG, (errmsg("pg_ram: _PG_fini completed successfully.")));
}

static void
StartExtensionNode(void)
{
	ereport(LOG, (errmsg("pg_ram: extension node initialized")));
}

static void
pg_ram_ProcessUtility(PlannedStmt *pstmt,
					  const char *queryString,
					  bool readOnlyTree,
					  ProcessUtilityContext context,
					  ParamListInfo params,
					  struct QueryEnvironment *queryEnv,
					  DestReceiver *dest,
					  QueryCompletion *completionTag)
{
	if (prev_ProcessUtility_hook)
		prev_ProcessUtility_hook(pstmt, queryString, readOnlyTree, context,
								  params, queryEnv, dest, completionTag);
	else
		standard_ProcessUtility(pstmt, queryString, readOnlyTree, context,
								params, queryEnv, dest, completionTag);
}
