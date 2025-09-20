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
#include "ramd_conn.h"

ramd_daemon_t *g_ramd_daemon = NULL;
PGconn       *g_conn = NULL;
static volatile sig_atomic_t g_shutdown_requested = 0;

static bool
ramd_establish_postgres_connection(void)
{
	PGconn *conn;

	if (!g_ramd_daemon)
		return false;

	ramd_log_info("Attempting to connect to PostgreSQL at %s:%d",
				  g_ramd_daemon->config.hostname,
				  g_ramd_daemon->config.postgresql_port);

	conn = ramd_conn_get_cached(g_ramd_daemon->config.node_id,
							   g_ramd_daemon->config.hostname,
							   g_ramd_daemon->config.postgresql_port,
							   g_ramd_daemon->config.database_name,
							   g_ramd_daemon->config.database_user,
							   g_ramd_daemon->config.database_password);

	if (!conn)
	{
		ramd_log_error("Failed to connect to PostgreSQL");
		return false;
	}

	g_conn = conn;
	ramd_log_info("Successfully connected to PostgreSQL at %s:%d",
				  g_ramd_daemon->config.hostname,
				  g_ramd_daemon->config.postgresql_port);
	return true;
}

PGconn *
ramd_get_postgres_connection(void)
{
	return g_conn;
}

static void *
ramd_connection_monitor_thread(void *arg)
{
	(void)arg;

	while (!g_shutdown_requested)
	{
		PGconn *conn;

		sleep(60);

		if (g_shutdown_requested)
			break;

		conn = ramd_get_postgres_connection();
		if (!conn)
		{
			ramd_log_warning("PostgreSQL connection lost, attempting to reconnect...");

			while (!g_shutdown_requested && !ramd_establish_postgres_connection())
			{
				ramd_log_error("Failed to reconnect to PostgreSQL, retrying in 60 seconds...");
				sleep(60);
			}

			if (!g_shutdown_requested)
				ramd_log_info("Successfully reconnected to PostgreSQL");
		}
	}

	return NULL;
}

static void
ramd_signal_handler(int sig)
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
			ramd_log_info("Configuration reload requested: Received SIGHUP signal - reloading daemon configuration");
			break;
		default:
			break;
	}
}

static void
ramd_setup_signal_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = ramd_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	signal(SIGPIPE, SIG_IGN);
}

static void
ramd_usage(const char *progname)
{
	printf("PostgreSQL Auto-Failover Daemon (RAMD) %s\n\n", RAMD_VERSION_STRING);
	printf("Enterprise-grade PostgreSQL high availability and automatic failover management system.\n\n");
	printf("Usage: %s [OPTIONS]\n\n", progname);
	printf("Configuration Options:\n");
	printf("  -c, --config FILE     Primary configuration file path (required)\n");
	printf("  -D, --daemonize       Execute as system daemon process\n");
	printf("  -l, --log-level LEVEL Logging verbosity level (debug, info, notice, warning, error, fatal)\n");
	printf("  -L, --log-file FILE   Log output file path (default: stderr)\n");
	printf("  -p, --pid-file FILE   Process identifier file path\n");
	printf("\n");
	printf("Information Options:\n");
	printf("  -h, --help            Display this comprehensive help information\n");
	printf("  -V, --version         Display version and build information\n");
	printf("\n");
	printf("For technical support and documentation, contact: <support@pgelephant.com>\n");
	printf("Project homepage: https://www.pgelephant.com/ramd\n");
}

static void
ramd_version(void)
{
	printf("RAMD (PostgreSQL Auto-Failover Daemon) %s\n", RAMD_VERSION_STRING);
	printf("Enterprise PostgreSQL High Availability Management System\n");
	printf("Copyright (c) 2024-2025, pgElephant, Inc.\n");
	printf("Licensed under PostgreSQL License - see LICENSE file for details\n");
	printf("\nBuilt with support for:\n");
	printf("  - Automatic failover and recovery\n");
	printf("  - Synchronous replication management\n");
	printf("  - HTTP REST API interface\n");
	printf("  - Real-time cluster monitoring\n");
}

bool
ramd_init(const char *config_file)
{
	g_ramd_daemon = malloc(sizeof(ramd_daemon_t));
	if (!g_ramd_daemon)
	{
		fprintf(stderr, "Critical error - Failed to allocate memory for daemon structure\n");
		return false;
	}

	memset(g_ramd_daemon, 0, sizeof(ramd_daemon_t));

	if (pthread_mutex_init(&g_ramd_daemon->mutex, NULL) != 0)
	{
		fprintf(stderr, "Critical error - Failed to initialize thread synchronization mutex\n");
		free(g_ramd_daemon);
		g_ramd_daemon = NULL;
		return false;
	}

	if (!ramd_config_init(&g_ramd_daemon->config))
	{
		fprintf(stderr, "Initialization failure: Unable to initialize daemon configuration subsystem\n");
		ramd_cleanup();
		return false;
	}

	if (config_file && !ramd_config_load_file(&g_ramd_daemon->config, config_file))
	{
		fprintf(stderr, "Configuration error: Unable to load configuration file '%s' - please verify file exists and permissions\n",
				config_file);
		ramd_cleanup();
		return false;
	}

	if (!ramd_conn_init())
	{
		fprintf(stderr, "Failed to initialize connection subsystem\n");
		ramd_cleanup();
		return false;
	}

	fprintf(stderr, "Establishing PostgreSQL connection...\n");
	while (!ramd_establish_postgres_connection())
	{
		fprintf(stderr, "Failed to connect to PostgreSQL, retrying in 60 seconds...\n");
		sleep(60);
	}
	fprintf(stderr, "PostgreSQL connection established successfully\n");

	if (!ramd_cluster_init(&g_ramd_daemon->cluster, &g_ramd_daemon->config))
	{
		fprintf(stderr, "Cluster initialization failure: Unable to initialize cluster management subsystem\n");
		ramd_cleanup();
		return false;
	}

	if (!ramd_monitor_init(&g_ramd_daemon->monitor, &g_ramd_daemon->cluster,
						  &g_ramd_daemon->config))
	{
		ramd_log_error("Monitor initialization failure: Unable to initialize database monitoring subsystem");
		ramd_cleanup();
		return false;
	}

	ramd_failover_context_init(&g_ramd_daemon->failover_context);

	if (g_ramd_daemon->config.http_api_enabled)
	{
		if (!ramd_http_server_init(&g_ramd_daemon->http_server,
								  g_ramd_daemon->config.http_bind_address,
								  g_ramd_daemon->config.http_port))
		{
			ramd_log_error("HTTP API initialization failure: Unable to initialize HTTP API server on %s:%d",
						  g_ramd_daemon->config.http_bind_address,
						  g_ramd_daemon->config.http_port);
			ramd_cleanup();
			return false;
		}

		if (g_ramd_daemon->config.http_auth_enabled)
		{
			g_ramd_daemon->http_server.auth_enabled = true;
			strncpy(g_ramd_daemon->http_server.auth_token,
					g_ramd_daemon->config.http_auth_token,
					sizeof(g_ramd_daemon->http_server.auth_token) - 1);
		}
	}

	ramd_sync_config_t sync_config;
	memset(&sync_config, 0, sizeof(sync_config));
	sync_config.mode = g_ramd_daemon->config.synchronous_replication
						? RAMD_SYNC_REMOTE_APPLY
						: RAMD_SYNC_OFF;
	sync_config.num_sync_standbys = g_ramd_daemon->config.num_sync_standbys;
	sync_config.sync_timeout_ms = g_ramd_daemon->config.sync_timeout_ms;
	sync_config.enforce_sync_standbys = g_ramd_daemon->config.enforce_sync_standbys;

	if (!ramd_sync_replication_init(&sync_config))
	{
		ramd_log_error("Synchronous replication initialization failure: Unable to configure synchronous replication subsystem");
		ramd_cleanup();
		return false;
	}

	if (!ramd_config_reload_init())
	{
		ramd_log_error("Failed to initialize configuration reload system");
		ramd_cleanup();
		return false;
	}

	if (g_ramd_daemon->config.maintenance_mode_enabled)
	{
		if (!ramd_maintenance_init())
		{
			ramd_log_error("Failed to initialize maintenance mode system");
			ramd_cleanup();
			return false;
		}
	}

	ramd_log_info("RAMD daemon initialization completed successfully - all subsystems operational");
	return true;
}

void
ramd_cleanup(void)
{
	if (!g_ramd_daemon)
		return;

	ramd_log_info("Initiating RAMD daemon shutdown procedure - cleaning up all subsystems");

	if (g_ramd_daemon->config.maintenance_mode_enabled)
		ramd_maintenance_cleanup();

	ramd_config_reload_cleanup();
	ramd_sync_replication_cleanup();

	if (g_ramd_daemon->config.http_api_enabled)
		ramd_http_server_cleanup(&g_ramd_daemon->http_server);

	ramd_monitor_stop(&g_ramd_daemon->monitor);
	ramd_monitor_cleanup(&g_ramd_daemon->monitor);
	ramd_failover_context_cleanup(&g_ramd_daemon->failover_context);
	ramd_cluster_cleanup(&g_ramd_daemon->cluster);
	ramd_config_cleanup(&g_ramd_daemon->config);

	if (strlen(g_ramd_daemon->config.pid_file) > 0)
		ramd_remove_pidfile(g_ramd_daemon->config.pid_file);

	pthread_mutex_destroy(&g_ramd_daemon->mutex);
	ramd_conn_cleanup();
	ramd_logging_cleanup();

	free(g_ramd_daemon);
	g_ramd_daemon = NULL;
}

void
ramd_run(void)
{
	pthread_t conn_monitor_thread;

	if (!g_ramd_daemon)
	{
		fprintf(stderr, "Critical error - Daemon subsystem not properly initialized\n");
		return;
	}

	ramd_log_info("RAMD service startup: PostgreSQL Auto-Failover Daemon starting (Node ID: %d, Cluster: %s)",
				  g_ramd_daemon->config.node_id,
				  g_ramd_daemon->config.cluster_name);

	if (g_ramd_daemon->config.http_api_enabled)
	{
		if (!ramd_http_server_start(&g_ramd_daemon->http_server))
		{
			ramd_log_error("HTTP API startup failure: Unable to start HTTP API server on %s:%d",
						  g_ramd_daemon->config.http_bind_address,
						  g_ramd_daemon->config.http_port);
			ramd_cleanup();
			return;
		}
		ramd_log_info("HTTP API service operational: Server listening on %s:%d",
					  g_ramd_daemon->config.http_bind_address,
					  g_ramd_daemon->config.http_port);
	}

	if (g_ramd_daemon->config.daemonize)
	{
		if (!ramd_daemonize())
		{
			ramd_log_fatal("Daemonization failure: Unable to detach process and run as system daemon");
			return;
		}
	}

	if (strlen(g_ramd_daemon->config.pid_file) > 0)
	{
		if (!ramd_write_pidfile(g_ramd_daemon->config.pid_file))
		{
			ramd_log_warning("PID file operation warning: Unable to write process ID to file '%s'",
							g_ramd_daemon->config.pid_file);
		}
	}

	if (!ramd_monitor_start(&g_ramd_daemon->monitor))
	{
		ramd_log_fatal("Monitor startup failure: Critical error - unable to start database monitoring subsystem");
		return;
	}

	if (pthread_create(&conn_monitor_thread, NULL, ramd_connection_monitor_thread, NULL) != 0)
	{
		ramd_log_error("Failed to create PostgreSQL connection monitoring thread");
	}
	else
	{
		ramd_log_info("PostgreSQL connection monitoring thread started");
		pthread_detach(conn_monitor_thread);
	}

	g_ramd_daemon->running = true;

	while (!g_shutdown_requested && !g_ramd_daemon->shutdown_requested)
	{
		sleep(1);

		if (ramd_failover_should_trigger(&g_ramd_daemon->cluster,
										&g_ramd_daemon->config))
		{
			ramd_log_warning("Failover trigger detected: Automatic failover conditions met - initiating failover procedure");

			if (!ramd_failover_execute(&g_ramd_daemon->cluster,
									 &g_ramd_daemon->config,
									 &g_ramd_daemon->failover_context))
			{
				ramd_log_error("Failover execution failure: Automatic failover procedure encountered errors and could not complete successfully");
			}
		}
	}

	g_ramd_daemon->running = false;
	ramd_log_info("RAMD service shutdown: PostgreSQL Auto-Failover Daemon shutting down gracefully");
}

void
ramd_stop(void)
{
	if (g_ramd_daemon)
		g_ramd_daemon->shutdown_requested = true;
}

bool
ramd_is_running(void)
{
	return g_ramd_daemon && g_ramd_daemon->running;
}

bool
ramd_daemonize(void)
{
	pid_t pid;

	pid = fork();
	if (pid < 0)
		return false;
	if (pid > 0)
		exit(0);

	if (setsid() < 0)
		return false;

	pid = fork();
	if (pid < 0)
		return false;
	if (pid > 0)
		exit(0);

	if (chdir("/") < 0)
		return false;

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	return true;
}

bool
ramd_write_pidfile(const char *pidfile_path)
{
	FILE  *fp;
	pid_t  pid = getpid();

	fp = fopen(pidfile_path, "w");
	if (!fp)
		return false;

	fprintf(fp, "%d\n", (int) pid);
	fclose(fp);

	return true;
}

void
ramd_remove_pidfile(const char *pidfile_path)
{
	unlink(pidfile_path);
}

int
main(int argc, char *argv[])
{
	const char *config_file    = NULL;
	const char *log_file      = NULL;
	const char *pid_file      = NULL;
	const char *log_level_str = NULL;
	bool        daemonize_flag = false;
	int         c;

	static struct option long_options[] = {
		{"config", required_argument, 0, 'c'},
		{"daemonize", no_argument, 0, 'D'},
		{"log-level", required_argument, 0, 'l'},
		{"log-file", required_argument, 0, 'L'},
		{"pid-file", required_argument, 0, 'p'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'V'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, "c:Dl:L:p:hV", long_options, NULL)) != -1)
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

	ramd_setup_signal_handlers();

	if (!ramd_init(config_file))
	{
		fprintf(stderr, "Fatal initialization error - Unable to initialize daemon subsystem\n");
		exit(1);
	}

	if (daemonize_flag)
		g_ramd_daemon->config.daemonize = true;
	if (log_file)
		strncpy(g_ramd_daemon->config.log_file, log_file,
				sizeof(g_ramd_daemon->config.log_file) - 1);
	if (pid_file)
		strncpy(g_ramd_daemon->config.pid_file, pid_file,
				sizeof(g_ramd_daemon->config.pid_file) - 1);
	if (log_level_str)
		g_ramd_daemon->config.log_level = ramd_logging_string_to_level(log_level_str);

	if (!ramd_logging_init(g_ramd_daemon->config.log_file,
						  g_ramd_daemon->config.log_level,
						  strlen(g_ramd_daemon->config.log_file) > 0,
						  g_ramd_daemon->config.log_to_syslog,
						  g_ramd_daemon->config.log_to_console))
	{
		fprintf(stderr, "Critical error - Failed to initialize logging subsystem\n");
		ramd_cleanup();
		exit(1);
	}

	ramd_run();
	ramd_cleanup();

	return 0;
}

const char *
pgraft_get_cluster_state_file_path(void)
{
	return NULL;
}
