/*-------------------------------------------------------------------------
 *
 * pgram_librale_worker.c
 *   Background worker that owns librale and runs its tick functions
 *
 * This worker:
 *  - Initializes librale using pgram_librale_init()
 *  - Periodically runs librale tick functions in sequence
 *  - No threading needed - all operations are non-blocking ticks
 *  - Shuts down cleanly on SIGTERM
 *
 *------------------------------------------------------------------------- */

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

#include <pthread.h>

#include "librale.h"
#include "shutdown.h"
#include "pgram_librale.h"
#include "pgram_librale_worker.h"
#include "pgram_guc.h"

/* Globals */
static volatile sig_atomic_t lib_got_sigterm = false;
static volatile sig_atomic_t lib_got_sighup = false;

/* Prototypes */
void librale_worker_main(Datum main_arg) __attribute__((visibility("default")));
static void librale_worker_sigterm(SIGNAL_ARGS);
static void librale_worker_sighup(SIGNAL_ARGS);

void pgram_librale_worker_register(void)
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
	strlcpy(worker.bgw_name, LIBRALE_WORKER_NAME, sizeof(worker.bgw_name));
	strlcpy(worker.bgw_function_name, "librale_worker_main",
	        sizeof(worker.bgw_function_name));
	strlcpy(worker.bgw_type, "pg_ram", sizeof(worker.bgw_type));

	RegisterBackgroundWorker(&worker);
}

__attribute__((visibility("default"))) void librale_worker_main(Datum main_arg)
{
	(void) main_arg;

	/* Signals */
	pqsignal(SIGTERM, librale_worker_sigterm);
	pqsignal(SIGHUP, librale_worker_sighup);

	/* Ensure librale is initialized with GUCs set in postgresql.conf */
	if (!pgram_librale_init())
	{
		elog(ERROR, "pg_ram: Failed to initialize librale in librale worker");
		proc_exit(1);
	}

	/* Override default config inside pgram_librale_config using GUCs if
	 * provided */
	if (pg_ram_librale_config && pg_ram_librale_config->librale_config)
	{
		librale_config_t* cfg = pg_ram_librale_config->librale_config;
		if (pgram_node_id > 0)
			(void) librale_config_set_node_id(cfg, pgram_node_id);
		if (pgram_node_name && *pgram_node_name)
			(void) librale_config_set_node_name(cfg, pgram_node_name);
		if (pgram_node_ip && *pgram_node_ip)
			(void) librale_config_set_node_ip(cfg, pgram_node_ip);
		if (pgram_rale_port > 0)
			(void) librale_config_set_rale_port(cfg, (uint16) pgram_rale_port);
		if (pgram_dstore_port > 0)
			(void) librale_config_set_dstore_port(cfg,
			                                      (uint16) pgram_dstore_port);
		if (pgram_db_path && *pgram_db_path)
			(void) librale_config_set_db_path(cfg, pgram_db_path);
	}

	elog(LOG, "pg_ram: librale worker started with tick-based processing");

	/* Main tick-based processing loop */
	while (!lib_got_sigterm)
	{
		int rc;

		/* Drive all librale tick functions in sequence (non-blocking) */
		(void) librale_dstore_server_tick();
		(void) librale_dstore_client_tick();
		(void) librale_unix_socket_tick();
		(void) librale_rale_tick();

		rc = WaitLatch(&MyProc->procLatch,
		               WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
		               100, /* ms - faster ticks for responsiveness */
		               0);
		if (rc & WL_POSTMASTER_DEATH)
			break;
		if (rc & WL_LATCH_SET)
			ResetLatch(&MyProc->procLatch);

		if (lib_got_sighup)
		{
			lib_got_sighup = false;
			ProcessConfigFile(PGC_SIGHUP);
		}
	}

	/* Request shutdown inside librale */
	librale_request_shutdown();

	pgram_librale_cleanup();
	elog(LOG, "pg_ram: librale worker exiting");
	proc_exit(0);
}

static void librale_worker_sigterm(SIGNAL_ARGS)
{
	(void) postgres_signal_arg;
	lib_got_sigterm = true;
	SetLatch(&MyProc->procLatch);
}

static void librale_worker_sighup(SIGNAL_ARGS)
{
	(void) postgres_signal_arg;
	lib_got_sighup = true;
	SetLatch(&MyProc->procLatch);
}
