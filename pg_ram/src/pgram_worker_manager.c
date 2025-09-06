/*-------------------------------------------------------------------------
 *
 * pgram_worker_manager.c
 *		Manager for pg_ram background workers
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
#include "utils/guc.h"
#include "utils/elog.h"
#include "pgram_librale.h"
#include "pgram_librale_worker.h"
#include "pgram_health_worker.h"
#include "pgram_workers.h"

#define MAX_WORKERS 10

typedef struct
{
	const char* name;        /* Worker name for identification */
	const char* function;    /* Function name to call */
	int sleep_ms;            /* Sleep interval in milliseconds */
	bool enabled;            /* Whether worker is enabled */
	BackgroundWorker worker; /* PostgreSQL background worker struct */
} worker_definition_t;

static worker_definition_t workers[] = {{.name = "pg_ram librale worker",
                                         .function = "librale_worker_main",
                                         .sleep_ms = 200,
                                         .enabled = true},
                                        {.name = "pg_ram health worker",
                                         .function = "health_worker_main",
                                         .sleep_ms = 5000,
                                         .enabled = true}};

static int worker_count = sizeof(workers) / sizeof(workers[0]);
static bool workers_registered = false;

static void worker_manager_init(void);
static void register_worker(worker_definition_t* worker_def);
static void unregister_worker(worker_definition_t* worker_def);
static void worker_manager_cleanup(void);

void pgram_worker_manager_init(void);
void pgram_worker_manager_cleanup(void);
void pgram_worker_manager_get_status(StringInfo buf);

void pgram_worker_manager_init(void)
{
	if (workers_registered)
		return;

	worker_manager_init();
	workers_registered = true;

	elog(LOG, "pg_ram: Worker manager initialized with %d workers",
	     worker_count);
}


static void worker_manager_init(void)
{
	int i;

	for (i = 0; i < worker_count; i++)
	{
		if (workers[i].enabled)
			register_worker(&workers[i]);
	}
}


static void register_worker(worker_definition_t* worker_def)
{
	if (strcmp(worker_def->function, "librale_worker_main") == 0)
	{
		pgram_librale_worker_register();
		elog(LOG, "pg_ram: Registered librale worker");
	}
	else if (strcmp(worker_def->function, "health_worker_main") == 0)
	{
		pgram_health_worker_register();
		elog(LOG, "pg_ram: Registered health worker");
	}
	else
		elog(WARNING, "pg_ram: Unknown worker function: %s",
		     worker_def->function);
}


static void __attribute__((used))
unregister_worker(worker_definition_t* worker_def)
{
	elog(DEBUG1, "pg_ram: Worker unregistration requested for: %s",
	     worker_def->name);
}


void pgram_worker_manager_cleanup(void)
{
	if (!workers_registered)
		return;

	worker_manager_cleanup();
	workers_registered = false;

	elog(LOG, "pg_ram: Worker manager cleaned up");
}


static void worker_manager_cleanup(void)
{
	elog(DEBUG1, "pg_ram: Worker manager cleanup completed");
}


void pgram_worker_manager_get_status(StringInfo buf)
{
	int i;

	appendStringInfo(buf, "pg_ram Worker Manager Status:\n");
	appendStringInfo(buf, "Workers registered: %s\n",
	                 workers_registered ? "yes" : "no");
	appendStringInfo(buf, "Total workers: %d\n", worker_count);

	for (i = 0; i < worker_count; i++)
	{
		appendStringInfo(buf, "  %s: %s\n", workers[i].name,
		                 workers[i].enabled ? "enabled" : "disabled");
	}
}
