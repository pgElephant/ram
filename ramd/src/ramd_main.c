/*-------------------------------------------------------------------------
 *
 * ramd_main.c
 *		PostgreSQL Auto-Failover Daemon - Main Entry Point
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <pthread.h>
#include <signal.h>
#include <getopt.h>

#include "ramd.h"
#include "ramd_daemon.h"
#include "ramd_config.h"
#include "ramd_cluster.h"
#include "ramd_monitor.h"
#include "ramd_failover.h"
#include "ramd_logging.h"
#include "ramd_http_api.h"
#include "ramd_sync_replication.h"
#include "ramd_config_reload.h"
#include "ramd_maintenance.h"

/* Global daemon instance */
ramd_daemon_t* g_ramd_daemon = NULL;

/* Structure definition is in ramd_daemon.h */

/* Signal handling */
static volatile sig_atomic_t g_shutdown_requested = 0;

static void ramd_signal_handler(int sig)
{
	switch (sig)
	{
	case SIGTERM:
	case SIGINT:
		g_shutdown_requested = 1;
		if (g_ramd_daemon)
			g_ramd_daemon->shutdown_requested = true;
		break;
	case SIGHUP:
		/* Reload configuration */
		ramd_log_info("Configuration reload requested: Received SIGHUP signal "
		              "- reloading daemon configuration");
		break;
	default:
		break;
	}
}


static void ramd_setup_signal_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = ramd_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	/* Ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);
}


static void ramd_usage(const char* progname)
{
	printf("PostgreSQL Auto-Failover Daemon (RAMD) %s\n\n",
	       RAMD_VERSION_STRING);
	printf("Enterprise-grade PostgreSQL high availability and automatic "
	       "failover management system.\n\n");
	printf("Usage: %s [OPTIONS]\n\n", progname);
	printf("Configuration Options:\n");
	printf(
	    "  -c, --config FILE     Primary configuration file path (required)\n");
	printf("  -D, --daemonize       Execute as system daemon process\n");
	printf("  -l, --log-level LEVEL Logging verbosity level (debug, info, "
	       "notice, warning, error, fatal)\n");
	printf("  -L, --log-file FILE   Log output file path (default: stderr)\n");
	printf("  -p, --pid-file FILE   Process identifier file path\n");
	printf("\n");
	printf("Information Options:\n");
	printf("  -h, --help            Display this comprehensive help "
	       "information\n");
	printf("  -V, --version         Display version and build information\n");
	printf("\n");
	printf("For technical support and documentation, contact: "
	       "<support@pgelephant.com>\n");
	printf("Project homepage: https://www.pgelephant.com/ramd\n");
}


static void ramd_version(void)
{
	printf("RAMD (PostgreSQL Auto-Failover Daemon) %s\n", RAMD_VERSION_STRING);
	printf("Enterprise PostgreSQL High Availability Management System\n");
	printf("Copyright (c) 2024-2025, pgElephant, Inc.\n");
	printf(
	    "Licensed under PostgreSQL License - see LICENSE file for details\n");
	printf("\nBuilt with support for:\n");
	printf("  - Automatic failover and recovery\n");
	printf("  - Synchronous replication management\n");
	printf("  - HTTP REST API interface\n");
	printf("  - Real-time cluster monitoring\n");
}


bool ramd_init(const char* config_file)
{
	/* Allocate daemon structure */
	g_ramd_daemon = malloc(sizeof(ramd_daemon_t));
	if (!g_ramd_daemon)
	{
		fprintf(stderr, "Critical error - Failed to allocate memory for daemon "
		                "structure\n");
		return false;
	}

	memset(g_ramd_daemon, 0, sizeof(ramd_daemon_t));

	/* Initialize mutex */
	if (pthread_mutex_init(&g_ramd_daemon->mutex, NULL) != 0)
	{
		fprintf(stderr, "Critical error - Failed to initialize thread "
		                "synchronization mutex\n");
		free(g_ramd_daemon);
		g_ramd_daemon = NULL;
		return false;
	}

	/* Initialize configuration */
	if (!ramd_config_init(&g_ramd_daemon->config))
	{
		fprintf(stderr, "Initialization failure: Unable to initialize daemon "
		                "configuration subsystem\n");
		ramd_cleanup();
		return false;
	}

	/* Load configuration file */
	if (config_file &&
	    !ramd_config_load_file(&g_ramd_daemon->config, config_file))
	{
		fprintf(stderr,
		        "Configuration error: Unable to load configuration file '%s' - "
		        "please verify file exists and permissions\n",
		        config_file);
		ramd_cleanup();
		return false;
	}

	/* Logging will be initialized after command line overrides */

	/* Initialize cluster */
	if (!ramd_cluster_init(&g_ramd_daemon->cluster, &g_ramd_daemon->config))
	{
		fprintf(stderr, "Cluster initialization failure: Unable to initialize "
		                "cluster management subsystem\n");
		ramd_cleanup();
		return false;
	}

	/* Initialize monitor */
	if (!ramd_monitor_init(&g_ramd_daemon->monitor, &g_ramd_daemon->cluster,
	                       &g_ramd_daemon->config))
	{
		ramd_log_error("Monitor initialization failure: Unable to initialize "
		               "database monitoring subsystem");
		ramd_cleanup();
		return false;
	}

	/* Initialize failover context */
	ramd_failover_context_init(&g_ramd_daemon->failover_context);

	/* Initialize HTTP API server */
	if (g_ramd_daemon->config.http_api_enabled)
	{
		if (!ramd_http_server_init(&g_ramd_daemon->http_server,
		                           g_ramd_daemon->config.http_bind_address,
		                           g_ramd_daemon->config.http_port))
		{
			ramd_log_error("HTTP API initialization failure: Unable to "
			               "initialize HTTP API server on %s:%d",
			               g_ramd_daemon->config.http_bind_address,
			               g_ramd_daemon->config.http_port);
			ramd_cleanup();
			return false;
		}

		/* Set authentication if enabled */
		if (g_ramd_daemon->config.http_auth_enabled)
		{
			g_ramd_daemon->http_server.auth_enabled = true;
			strncpy(g_ramd_daemon->http_server.auth_token,
			        g_ramd_daemon->config.http_auth_token,
			        sizeof(g_ramd_daemon->http_server.auth_token) - 1);
		}
	}

	/* Initialize synchronous replication */
	ramd_sync_config_t sync_config;
	memset(&sync_config, 0, sizeof(sync_config));
	sync_config.mode = g_ramd_daemon->config.synchronous_replication
	                       ? RAMD_SYNC_REMOTE_APPLY
	                       : RAMD_SYNC_OFF;
	sync_config.num_sync_standbys = g_ramd_daemon->config.num_sync_standbys;
	sync_config.sync_timeout_ms = g_ramd_daemon->config.sync_timeout_ms;
	sync_config.enforce_sync_standbys =
	    g_ramd_daemon->config.enforce_sync_standbys;

	if (!ramd_sync_replication_init(&sync_config))
	{
		ramd_log_error("Synchronous replication initialization failure: Unable "
		               "to configure synchronous replication subsystem");
		ramd_cleanup();
		return false;
	}

	/* Initialize configuration reload system */
	if (!ramd_config_reload_init())
	{
		ramd_log_error("Failed to initialize configuration reload system");
		ramd_cleanup();
		return false;
	}

	/* Initialize maintenance mode system */
	if (g_ramd_daemon->config.maintenance_mode_enabled)
	{
		if (!ramd_maintenance_init())
		{
			ramd_log_error("Failed to initialize maintenance mode system");
			ramd_cleanup();
			return false;
		}
	}

	ramd_log_info("RAMD daemon initialization completed successfully - all "
	              "subsystems operational");
	return true;
}


void ramd_cleanup(void)
{
	if (!g_ramd_daemon)
		return;

	ramd_log_info("Initiating RAMD daemon shutdown procedure - cleaning up all "
	              "subsystems");

	/* Cleanup maintenance mode system */
	if (g_ramd_daemon->config.maintenance_mode_enabled)
	{
		ramd_maintenance_cleanup();
	}

	/* Cleanup configuration reload system */
	ramd_config_reload_cleanup();

	/* Cleanup synchronous replication */
	ramd_sync_replication_cleanup();

	/* Cleanup HTTP API server */
	if (g_ramd_daemon->config.http_api_enabled)
	{
		ramd_http_server_cleanup(&g_ramd_daemon->http_server);
	}

	/* Stop monitor */
	ramd_monitor_stop(&g_ramd_daemon->monitor);
	ramd_monitor_cleanup(&g_ramd_daemon->monitor);

	/* Cleanup failover context */
	ramd_failover_context_cleanup(&g_ramd_daemon->failover_context);

	/* Cleanup cluster */
	ramd_cluster_cleanup(&g_ramd_daemon->cluster);

	/* Cleanup configuration */
	ramd_config_cleanup(&g_ramd_daemon->config);

	/* Remove PID file */
	if (strlen(g_ramd_daemon->config.pid_file) > 0)
		ramd_remove_pidfile(g_ramd_daemon->config.pid_file);

	/* Destroy mutex */
	pthread_mutex_destroy(&g_ramd_daemon->mutex);

	/* Cleanup logging */
	ramd_logging_cleanup();

	/* Free daemon structure */
	free(g_ramd_daemon);
	g_ramd_daemon = NULL;
}


void ramd_run(void)
{
	if (!g_ramd_daemon)
	{
		fprintf(stderr,
		        "Critical error - Daemon subsystem not properly initialized\n");
		return;
	}

	ramd_log_info("RAMD service startup: PostgreSQL Auto-Failover Daemon "
	              "starting (Node ID: %d, Cluster: %s)",
	              g_ramd_daemon->config.node_id,
	              g_ramd_daemon->config.cluster_name);

	/* Start HTTP API server if enabled */
	if (g_ramd_daemon->config.http_api_enabled)
	{
		if (!ramd_http_server_start(&g_ramd_daemon->http_server))
		{
			ramd_log_error("HTTP API startup failure: Unable to start HTTP API "
			               "server on %s:%d",
			               g_ramd_daemon->config.http_bind_address,
			               g_ramd_daemon->config.http_port);
			ramd_cleanup();
			return;
		}
		ramd_log_info("HTTP API service operational: Server listening on %s:%d",
		              g_ramd_daemon->config.http_bind_address,
		              g_ramd_daemon->config.http_port);
	}

	/* Daemonize if requested */
	if (g_ramd_daemon->config.daemonize)
	{
		if (!ramd_daemonize())
		{
			ramd_log_fatal("Daemonization failure: Unable to detach process "
			               "and run as system daemon");
			return;
		}
	}

	/* Write PID file */
	if (strlen(g_ramd_daemon->config.pid_file) > 0)
	{
		if (!ramd_write_pidfile(g_ramd_daemon->config.pid_file))
		{
			ramd_log_warning("PID file operation warning: Unable to write "
			                 "process ID to file '%s'",
			                 g_ramd_daemon->config.pid_file);
		}
	}

	/* Start monitor */
	if (!ramd_monitor_start(&g_ramd_daemon->monitor))
	{
		ramd_log_fatal("Monitor startup failure: Critical error - unable to "
		               "start database monitoring subsystem");
		return;
	}

	g_ramd_daemon->running = true;

	/* Main daemon loop */
	while (!g_shutdown_requested && !g_ramd_daemon->shutdown_requested)
	{
		sleep(1);

		/* Check for failover conditions */
		if (ramd_failover_should_trigger(&g_ramd_daemon->cluster,
		                                 &g_ramd_daemon->config))
		{
			ramd_log_warning("Failover trigger detected: Automatic failover "
			                 "conditions met - initiating failover procedure");

			if (!ramd_failover_execute(&g_ramd_daemon->cluster,
			                           &g_ramd_daemon->config,
			                           &g_ramd_daemon->failover_context))
			{
				ramd_log_error(
				    "Failover execution failure: Automatic failover procedure "
				    "encountered errors and could not complete successfully");
			}
		}
	}

	g_ramd_daemon->running = false;
	ramd_log_info("RAMD service shutdown: PostgreSQL Auto-Failover Daemon "
	              "shutting down gracefully");
}


void ramd_stop(void)
{
	if (g_ramd_daemon)
		g_ramd_daemon->shutdown_requested = true;
}


bool ramd_is_running(void)
{
	return g_ramd_daemon && g_ramd_daemon->running;
}


bool ramd_daemonize(void)
{
	pid_t pid;

	/* Fork first time */
	pid = fork();
	if (pid < 0)
		return false;
	if (pid > 0)
		exit(0); /* Parent exits */

	/* Create new session */
	if (setsid() < 0)
		return false;

	/* Fork second time */
	pid = fork();
	if (pid < 0)
		return false;
	if (pid > 0)
		exit(0); /* Parent exits */

	/* Change working directory */
	if (chdir("/") < 0)
		return false;

	/* Close file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	return true;
}


bool ramd_write_pidfile(const char* pidfile_path)
{
	FILE* fp;
	pid_t pid = getpid();

	fp = fopen(pidfile_path, "w");
	if (!fp)
		return false;

	fprintf(fp, "%d\n", (int) pid);
	fclose(fp);

	return true;
}


void ramd_remove_pidfile(const char* pidfile_path)
{
	unlink(pidfile_path);
}


int main(int argc, char* argv[])
{
	const char* config_file = NULL;
	const char* log_file = NULL;
	const char* pid_file = NULL;
	const char* log_level_str = NULL;
	bool daemonize_flag = false;
	int c;

	static struct option long_options[] = {
	    {"config", required_argument, 0, 'c'},
	    {"daemonize", no_argument, 0, 'D'},
	    {"log-level", required_argument, 0, 'l'},
	    {"log-file", required_argument, 0, 'L'},
	    {"pid-file", required_argument, 0, 'p'},
	    {"help", no_argument, 0, 'h'},
	    {"version", no_argument, 0, 'V'},
	    {0, 0, 0, 0}};

	/* Parse command line arguments */
	while ((c = getopt_long(argc, argv, "c:Dl:L:p:hV", long_options, NULL)) !=
	       -1)
	{
		switch (c)
		{
		case 'c':
			config_file = optarg;
			break;
		case 'D':
			daemonize_flag = true;
			break;
		case 'l':
			log_level_str = optarg;
			break;
		case 'L':
			log_file = optarg;
			break;
		case 'p':
			pid_file = optarg;
			break;
		case 'h':
			ramd_usage(argv[0]);
			exit(0);
		case 'V':
			ramd_version();
			exit(0);
		default:
			ramd_usage(argv[0]);
			exit(1);
		}
	}

	/* Setup signal handlers */
	ramd_setup_signal_handlers();

	/* Initialize daemon */
	if (!ramd_init(config_file))
	{
		fprintf(stderr, "Fatal initialization error - Unable to initialize "
		                "daemon subsystem\n");
		exit(1);
	}

	/* Override configuration with command line options BEFORE initializing
	 * logging */
	if (daemonize_flag)
		g_ramd_daemon->config.daemonize = true;
	if (log_file)
		strncpy(g_ramd_daemon->config.log_file, log_file,
		        sizeof(g_ramd_daemon->config.log_file) - 1);
	if (pid_file)
		strncpy(g_ramd_daemon->config.pid_file, pid_file,
		        sizeof(g_ramd_daemon->config.pid_file) - 1);
	if (log_level_str)
		g_ramd_daemon->config.log_level =
		    ramd_logging_string_to_level(log_level_str);

	/* Now initialize logging with final configuration */
	if (!ramd_logging_init(g_ramd_daemon->config.log_file,
	                       g_ramd_daemon->config.log_level,
	                       strlen(g_ramd_daemon->config.log_file) > 0,
	                       g_ramd_daemon->config.log_to_syslog,
	                       g_ramd_daemon->config.log_to_console))
	{
		fprintf(stderr,
		        "Critical error - Failed to initialize logging subsystem\n");
		ramd_cleanup();
		exit(1);
	}

	/* Run daemon */
	ramd_run();

	/* Cleanup */
	ramd_cleanup();

	return 0;
}

/*
 * Stub implementation for pgraft_get_cluster_state_file_path()
 * This function is needed by consensus libraries but is only properly implemented
 * in the PostgreSQL extension context. For the standalone ramd daemon,
 * we provide a simple stub that returns NULL, which will cause
 * consensus libraries to fall back to other methods.
 */
const char* pgraft_get_cluster_state_file_path(void)
{
	return NULL;
}
