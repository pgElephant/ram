/*-------------------------------------------------------------------------
 *
 * pgram_health_worker.c
 *		Background worker for node health monitoring
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
#include "pgram_health_worker.h"
#include "pgram_health_types.h"

static volatile sig_atomic_t got_sigterm = false;
static volatile sig_atomic_t got_sighup = false;

static int pgram_health_period_ms = HEALTH_WORKER_SLEEP_MS;
static bool pgram_health_verbose = false;

static health_worker_status_t g_health_status;

void health_worker_main(Datum main_arg) __attribute__((visibility("default")));
static void health_worker_sigterm(SIGNAL_ARGS);
static void health_worker_sighup(SIGNAL_ARGS);
static void health_worker_cleanup(void);
static void perform_health_checks(void);

void pgram_health_status_snapshot(health_worker_status_t* out)
{
	if (!out)
		return;
	*out = g_health_status;
}

void pgram_health_worker_register(void)
{
	BackgroundWorker worker; /* Background worker configuration */

	MemSet(&worker, 0, sizeof(BackgroundWorker));

	worker.bgw_flags =
	    BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	worker.bgw_restart_time = 1;
	worker.bgw_main_arg = Int32GetDatum(0);
	worker.bgw_notify_pid = 0;
	strlcpy(worker.bgw_library_name, "pg_ram", sizeof(worker.bgw_library_name));
	strlcpy(worker.bgw_name, HEALTH_WORKER_NAME, sizeof(worker.bgw_name));
	strlcpy(worker.bgw_function_name, "health_worker_main",
	        sizeof(worker.bgw_function_name));
	strlcpy(worker.bgw_type, "pg_ram", sizeof(worker.bgw_type));

	RegisterBackgroundWorker(&worker);
}

__attribute__((visibility("default"))) void health_worker_main(Datum main_arg)
{
	(void) main_arg;

	pqsignal(SIGTERM, health_worker_sigterm);
	pqsignal(SIGHUP, health_worker_sighup);

	DefineCustomIntVariable("pg_ram.health_check_period_ms",
	                        "Health worker sleep between checks (ms)", NULL,
	                        &pgram_health_period_ms, HEALTH_WORKER_SLEEP_MS,
	                        100, 60000, PGC_POSTMASTER, 0, NULL, NULL, NULL);

	DefineCustomBoolVariable(
	    "pg_ram.health_verbose", "Enable verbose health logging", NULL,
	    &pgram_health_verbose, false, PGC_POSTMASTER, 0, NULL, NULL, NULL);

	elog(LOG, "pg_ram: Health worker started");
	MemSet(&g_health_status, 0, sizeof(g_health_status));
	g_health_status.is_running = true;

	while (!got_sigterm)
	{
		int rc;

		perform_health_checks();

		rc = WaitLatch(&MyProc->procLatch,
		               WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
		               pgram_health_period_ms, 0);

		if (rc & WL_POSTMASTER_DEATH)
		{
			elog(LOG,
			     "pg_ram: Health worker shutting down due to postmaster death");
			break;
		}

		if (rc & WL_LATCH_SET)
			ResetLatch(&MyProc->procLatch);

		if (got_sighup)
		{
			got_sighup = false;
			ProcessConfigFile(PGC_SIGHUP);
		}
	}

	health_worker_cleanup();
	proc_exit(0);
}

static void perform_health_checks(void)
{
	TimestampTz now = GetCurrentTimestamp();
	uint32_t node_count;
	bool has_quorum;
	int32_t current_role;
	PgramHealthStatus status;

	g_health_status.last_activity = now;
	g_health_status.health_checks_performed++;

	node_count = pgram_librale_get_node_count();
	has_quorum = pgram_librale_has_quorum();
	current_role = pgram_librale_get_current_role();

	status = PGRAM_HEALTH_OK;
	if (node_count == 0)
		status = PGRAM_HEALTH_WARNING;
	if (!has_quorum && node_count > 1)
		status = PGRAM_HEALTH_ERROR;

	if (pgram_librale_is_leader())
	{
		uint32_t connected = 0;
		int32_t nid;

		for (nid = 1; nid <= (int32_t) node_count; nid++)
		{
			if (pgram_librale_is_node_healthy(nid))
				connected++;
		}
		if (node_count > 0 && connected == 0)
			status = PGRAM_HEALTH_CRITICAL;
	}

	g_health_status.last_health_status = status;
	if (status == PGRAM_HEALTH_WARNING)
		g_health_status.warnings_count++;
	if (status == PGRAM_HEALTH_ERROR || status == PGRAM_HEALTH_CRITICAL)
		g_health_status.errors_count++;

	if (pgram_health_verbose)
		elog(LOG, "pg_ram: health check: role=%d nodes=%u quorum=%s status=%d",
		     current_role, node_count, has_quorum ? "yes" : "no", status);
}

static void health_worker_sigterm(SIGNAL_ARGS)
{
	(void) postgres_signal_arg;
	got_sigterm = true;
	SetLatch(&MyProc->procLatch);
}

static void health_worker_sighup(SIGNAL_ARGS)
{
	(void) postgres_signal_arg;
	got_sighup = true;
	SetLatch(&MyProc->procLatch);
}

static void health_worker_cleanup(void)
{
	g_health_status.is_running = false;
	elog(LOG, "pg_ram: Health worker cleaning up");
}
