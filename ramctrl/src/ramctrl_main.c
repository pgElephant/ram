/*-------------------------------------------------------------------------
 *
 * ramctrl_main.c
 *		PostgreSQL RAM Control Utility - Main Entry Point
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include "ramctrl.h"
#include "ramctrl_database.h"
#include "ramctrl_daemon.h"
#include "ramctrl_defaults.h"
#include "ramctrl_watch.h"
#include "ramctrl_help.h"
#include "ramctrl_show.h"

void ramctrl_usage(const char* progname)
{
	fprintf(stdout, "PostgreSQL RAM Control Utility (ramctrl) %s\n\n",
	        RAMCTRL_VERSION_STRING);
	fprintf(stdout, "Usage: %s [OPTIONS] COMMAND [ARGS...]\n\n", progname);

	fprintf(stdout, "Global Options:\n");
	fprintf(stdout,
	        "  -h, --host HOST       PostgreSQL host (default: from config)\n");
	fprintf(stdout,
	        "  -p, --port PORT       PostgreSQL port (default: from config)\n");
	fprintf(stdout,
	        "  -d, --database DB     Database name (default: postgres)\n");
	fprintf(stdout,
	        "  -U, --user USER       Database user (default: postgres)\n");
	fprintf(stdout, "  -W, --password PASS   Database password\n");
	fprintf(stdout, "  -c, --config FILE     Configuration file path\n");
	fprintf(stdout, "  -v, --verbose         Verbose output\n");
	fprintf(stdout, "  -j, --json            JSON output format\n");
	fprintf(stdout,
	        "  -t, --timeout SEC     Timeout in seconds (default: 30)\n");
	fprintf(
	    stdout,
	    "      --table            Table output format (overrides --json)\n");
	fprintf(stdout, "      --help            Show this help message\n");
	fprintf(stdout, "      --version         Show version information\n");
	fprintf(stdout, "\n");

	fprintf(stdout, "Commands:\n");
	fprintf(stdout, "  status                        Show cluster status\n");
	fprintf(stdout, "  start [NODE_ID]               Start ramd daemon "
	                "(optionally on specific node)\n");
	fprintf(stdout, "  stop [NODE_ID]                Stop ramd daemon "
	                "(optionally on specific node)\n");
	fprintf(stdout, "  restart [NODE_ID]             Restart ramd daemon "
	                "(optionally on specific node)\n");
	fprintf(stdout,
	        "  promote NODE_ID               Promote node to primary\n");
	fprintf(stdout, "  demote NODE_ID                Demote node from primary "
	                "to standby\n");
	fprintf(stdout, "  failover [NODE_ID]            Trigger manual failover "
	                "(optionally to specific node)\n");
	fprintf(stdout,
	        "  show cluster                  Show cluster information\n");
	fprintf(stdout, "  show nodes                    Show all nodes status\n");
	fprintf(stdout, "  add-node NODE_ID HOST PORT    Add node to cluster\n");
	fprintf(stdout,
	        "  remove-node NODE_ID           Remove node from cluster\n");
	fprintf(stdout,
	        "  enable-maintenance NODE_ID    Enable maintenance mode\n");
	fprintf(stdout,
	        "  disable-maintenance NODE_ID   Disable maintenance mode\n");
	fprintf(stdout, "  logs [NODE_ID]                Show daemon logs\n");
	fprintf(stdout, "  watch [cluster|nodes|replication] Watch mode for "
	                "real-time monitoring\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Replication Management:\n");
	fprintf(
	    stdout,
	    "  show replication              Show replication status and lag\n");
	fprintf(stdout, "  set replication-mode MODE     Set replication mode "
	                "(async/sync_remote_write/sync_remote_apply/auto)\n");
	fprintf(stdout, "  set lag-threshold BYTES       Set maximum lag threshold "
	                "for failover\n");
	fprintf(stdout, "  wal-e backup                  Create WAL-E backup\n");
	fprintf(stdout,
	        "  wal-e restore BACKUP_NAME     Restore from WAL-E backup\n");
	fprintf(stdout,
	        "  wal-e list                    List available WAL-E backups\n");
	fprintf(stdout, "  wal-e delete BACKUP_NAME      Delete WAL-E backup\n");
	fprintf(stdout, "  bootstrap run NODE_TYPE       Run bootstrap script "
	                "(primary/standby)\n");
	fprintf(stdout,
	        "  bootstrap validate            Validate bootstrap script\n");
	fprintf(stdout, "\n");

	fprintf(stdout, "Examples:\n");
	fprintf(stdout, "  %s status                      # Show cluster status\n",
	        progname);
	fprintf(stdout,
	        "  %s show cluster                # Show detailed cluster info\n",
	        progname);
	fprintf(stdout,
	        "  %s show nodes                  # Show all nodes status\n",
	        progname);
	fprintf(stdout,
	        "  %s promote 2                   # Promote node 2 to primary\n",
	        progname);
	fprintf(stdout,
	        "  %s failover                    # Trigger automatic failover\n",
	        progname);
	fprintf(stdout,
	        "  %s add-node 4 host4 PORT       # Add node 4 to cluster\n",
	        progname);
	fprintf(stdout,
	        "  %s start                       # Start local ramd daemon\n",
	        progname);
	fprintf(stdout, "  %s logs 1                      # Show logs for node 1\n",
	        progname);
	fprintf(stdout, "\n");
	fprintf(stdout, "Output Formats:\n");
	fprintf(stdout, "  Default: Human-readable text\n");
	fprintf(stdout, "  --json:  JSON format for scripting\n");
	fprintf(stdout, "  --table: Professional table format\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Report bugs to <support@pgelephant.com>\n");
}


void ramctrl_version(void)
{
	fprintf(stdout, "ramctrl (PostgreSQL RAM Control Utility) %s\n",
	        RAMCTRL_VERSION_STRING);
	fprintf(stdout, "Copyright (c) 2024-2025, pgElephant, Inc.\n");
}


bool ramctrl_init(ramctrl_context_t* ctx)
{
	if (!ctx)
		return false;

	memset(ctx, 0, sizeof(ramctrl_context_t));

	/* Set defaults */
	ctx->hostname[0] = '\0'; /* No default hostname */
	ctx->port = 0;           /* No default port */
	strncpy(ctx->database, RAMCTRL_DEFAULT_PG_DATABASE,
	        sizeof(ctx->database) - 1);
	strncpy(ctx->user, RAMCTRL_DEFAULT_PG_USER, sizeof(ctx->user) - 1);
	ctx->timeout_seconds = RAMCTRL_DEFAULT_TIMEOUT_SECONDS;
	ctx->verbose = false;
	ctx->json_output = false;
	ctx->table_output = false;
	ctx->command = RAMCTRL_CMD_UNKNOWN;
	/* Initialize subcommands */
	ctx->show_command = RAMCTRL_SHOW_UNKNOWN;
	ctx->node_command = RAMCTRL_NODE_UNKNOWN;
	ctx->watch_command = RAMCTRL_WATCH_UNKNOWN;
	ctx->replication_command = RAMCTRL_REPLICATION_UNKNOWN;
	ctx->replica_command = RAMCTRL_REPLICA_UNKNOWN;
	ctx->backup_command = RAMCTRL_BACKUP_UNKNOWN;
	ctx->bootstrap_command = RAMCTRL_BOOTSTRAP_UNKNOWN;

	return true;
}


void ramctrl_cleanup(ramctrl_context_t* ctx)
{
	if (!ctx)
		return;

	memset(ctx->password, 0, sizeof(ctx->password));
}


bool ramctrl_parse_args(ramctrl_context_t* ctx, int argc, char* argv[])
{
	int c;
	int option_index = 0;

	static struct option long_options[] = {
	    {"host", required_argument, 0, 'h'},
	    {"port", required_argument, 0, 'p'},
	    {"database", required_argument, 0, 'd'},
	    {"user", required_argument, 0, 'U'},
	    {"password", required_argument, 0, 'W'},
	    {"config", required_argument, 0, 'c'},
	    {"verbose", no_argument, 0, 'v'},
	    {"json", no_argument, 0, 'j'},
	    {"timeout", required_argument, 0, 't'},
	    {"table", no_argument, 0, 'T'},
	    {"help", no_argument, 0, 1000},
	    {"version", no_argument, 0, 1001},
	    {0, 0, 0, 0}};

	while ((c = getopt_long(argc, argv, "h:p:d:U:W:c:vjt:T", long_options,
	                        &option_index)) != -1)
	{
		switch (c)
		{
		case 'h':
			strncpy(ctx->hostname, optarg, sizeof(ctx->hostname) - 1);
			ctx->hostname[sizeof(ctx->hostname) - 1] = '\0';
			break;
		case 'p':
			ctx->port = atoi(optarg);
			if (ctx->port <= 0 || ctx->port > 65535)
			{
				fprintf(stderr, "ramctrl: invalid port number: %s\n", optarg);
				return false;
			}
			break;
		case 'd':
			strncpy(ctx->database, optarg, sizeof(ctx->database) - 1);
			ctx->database[sizeof(ctx->database) - 1] = '\0';
			break;
		case 'U':
			strncpy(ctx->user, optarg, sizeof(ctx->user) - 1);
			ctx->user[sizeof(ctx->user) - 1] = '\0';
			break;
		case 'W':
			strncpy(ctx->password, optarg, sizeof(ctx->password) - 1);
			ctx->password[sizeof(ctx->password) - 1] = '\0';
			break;
		case 'c':
			strncpy(ctx->config_file, optarg, sizeof(ctx->config_file) - 1);
			ctx->config_file[sizeof(ctx->config_file) - 1] = '\0';
			break;
		case 'v':
			ctx->verbose = true;
			break;
		case 'j':
			ctx->json_output = true;
			break;
		case 't':
			ctx->timeout_seconds = atoi(optarg);
			if (ctx->timeout_seconds <= 0)
			{
				fprintf(stderr, "ramctrl: invalid timeout value: %s\n", optarg);
				return false;
			}
			break;
		case 'T':
			ctx->table_output = true;
			ctx->json_output = false;
			break;
		case 1000:
			if (ctx->command == RAMCTRL_CMD_UNKNOWN)
				ctx->command = RAMCTRL_CMD_HELP;
			return true;
		case 1001:
			if (ctx->command == RAMCTRL_CMD_UNKNOWN)
				ctx->command = RAMCTRL_CMD_VERSION;
			return true;
		default:
			fprintf(stderr, "ramctrl: invalid option: %c\n", c);
			return false;
		}
	}

	/* Parse command */
	if (optind < argc)
	{
		if (strcmp(argv[optind], "status") == 0)
			ctx->command = RAMCTRL_CMD_STATUS;
		else if (strcmp(argv[optind], "start") == 0)
			ctx->command = RAMCTRL_CMD_START;
		else if (strcmp(argv[optind], "stop") == 0)
			ctx->command = RAMCTRL_CMD_STOP;
		else if (strcmp(argv[optind], "restart") == 0)
			ctx->command = RAMCTRL_CMD_RESTART;
		else if (strcmp(argv[optind], "promote") == 0)
			ctx->command = RAMCTRL_CMD_PROMOTE;
		else if (strcmp(argv[optind], "demote") == 0)
			ctx->command = RAMCTRL_CMD_DEMOTE;
		else if (strcmp(argv[optind], "failover") == 0)
			ctx->command = RAMCTRL_CMD_FAILOVER;
		else if (strcmp(argv[optind], "show") == 0)
		{
			ctx->command = RAMCTRL_CMD_SHOW;
			optind++;
			if (optind < argc)
			{
				if (strcmp(argv[optind], "cluster") == 0)
					ctx->show_command = RAMCTRL_SHOW_CLUSTER;
				else if (strcmp(argv[optind], "nodes") == 0)
					ctx->show_command = RAMCTRL_SHOW_NODES;
				else if (strcmp(argv[optind], "replication") == 0)
					ctx->show_command = RAMCTRL_SHOW_REPLICATION;
				else if (strcmp(argv[optind], "status") == 0)
					ctx->show_command = RAMCTRL_SHOW_STATUS;
				else if (strcmp(argv[optind], "config") == 0)
					ctx->show_command = RAMCTRL_SHOW_CONFIG;
				else if (strcmp(argv[optind], "logs") == 0)
					ctx->show_command = RAMCTRL_SHOW_LOGS;
				else
				{
					ctx->show_command = RAMCTRL_SHOW_UNKNOWN;
					optind--; /* Don't consume unknown subcommand */
				}
				if (ctx->show_command != RAMCTRL_SHOW_UNKNOWN)
					optind++;
			}
			else
			{
				ctx->show_command = RAMCTRL_SHOW_UNKNOWN;
			}
		}
		else if (strcmp(argv[optind], "node") == 0)
		{
			ctx->command = RAMCTRL_CMD_NODE;
			optind++;
			if (optind < argc)
			{
				if (strcmp(argv[optind], "add") == 0)
					ctx->node_command = RAMCTRL_NODE_ADD;
				else if (strcmp(argv[optind], "remove") == 0)
					ctx->node_command = RAMCTRL_NODE_REMOVE;
				else if (strcmp(argv[optind], "list") == 0)
					ctx->node_command = RAMCTRL_NODE_LIST;
				else if (strcmp(argv[optind], "status") == 0)
					ctx->node_command = RAMCTRL_NODE_STATUS;
				else if (strcmp(argv[optind], "maintenance-on") == 0)
					ctx->node_command = RAMCTRL_NODE_ENABLE_MAINTENANCE;
				else if (strcmp(argv[optind], "maintenance-off") == 0)
					ctx->node_command = RAMCTRL_NODE_DISABLE_MAINTENANCE;
				else
				{
					ctx->node_command = RAMCTRL_NODE_UNKNOWN;
					optind--; /* Don't consume unknown subcommand */
				}
				if (ctx->node_command != RAMCTRL_NODE_UNKNOWN)
					optind++;
			}
			else
			{
				ctx->node_command = RAMCTRL_NODE_UNKNOWN;
			}
		}
		else if (strcmp(argv[optind], "logs") == 0)
			ctx->command = RAMCTRL_CMD_LOGS;
		else if (strcmp(argv[optind], "help") == 0)
		{
			ctx->command = RAMCTRL_CMD_HELP;
			optind++;
			if (optind < argc)
			{
				strncpy(ctx->command_args[ctx->command_argc], argv[optind],
				        sizeof(ctx->command_args[ctx->command_argc]) - 1);
				ctx->command_args[ctx->command_argc]
				                 [sizeof(ctx->command_args[ctx->command_argc]) - 1] =
				    '\0';
				ctx->command_argc++;
				optind++;
			}
		}
		else if (strcmp(argv[optind], "version") == 0)
			ctx->command = RAMCTRL_CMD_VERSION;
		else if (strcmp(argv[optind], "replication") == 0)
		{
			ctx->command = RAMCTRL_CMD_REPLICATION;
			optind++;
			if (optind < argc)
			{
				if (strcmp(argv[optind], "status") == 0)
					ctx->replication_command = RAMCTRL_REPLICATION_STATUS;
				else if (strcmp(argv[optind], "set-mode") == 0)
					ctx->replication_command = RAMCTRL_REPLICATION_SET_MODE;
				else if (strcmp(argv[optind], "set-lag") == 0)
					ctx->replication_command = RAMCTRL_REPLICATION_SET_LAG_THRESHOLD;
				else if (strcmp(argv[optind], "slots") == 0)
					ctx->replication_command = RAMCTRL_REPLICATION_SHOW_SLOTS;
				else
				{
					ctx->replication_command = RAMCTRL_REPLICATION_UNKNOWN;
					optind--; /* Don't consume unknown subcommand */
				}
				if (ctx->replication_command != RAMCTRL_REPLICATION_UNKNOWN)
					optind++;
			}
			else
			{
				ctx->replication_command = RAMCTRL_REPLICATION_UNKNOWN;
			}
		}
		else if (strcmp(argv[optind], "replica") == 0)
		{
			ctx->command = RAMCTRL_CMD_REPLICA;
			optind++;
			if (optind < argc)
			{
				if (strcmp(argv[optind], "add") == 0)
					ctx->replica_command = RAMCTRL_REPLICA_ADD;
				else if (strcmp(argv[optind], "remove") == 0)
					ctx->replica_command = RAMCTRL_REPLICA_REMOVE;
				else if (strcmp(argv[optind], "list") == 0)
					ctx->replica_command = RAMCTRL_REPLICA_LIST;
				else if (strcmp(argv[optind], "status") == 0)
					ctx->replica_command = RAMCTRL_REPLICA_STATUS;
				else
				{
					ctx->replica_command = RAMCTRL_REPLICA_UNKNOWN;
					optind--; /* Don't consume unknown subcommand */
				}
				if (ctx->replica_command != RAMCTRL_REPLICA_UNKNOWN)
					optind++;
			}
			else
			{
				ctx->replica_command = RAMCTRL_REPLICA_UNKNOWN;
			}
		}
		else if (strcmp(argv[optind], "backup") == 0)
		{
			ctx->command = RAMCTRL_CMD_BACKUP;
			optind++;
			if (optind < argc)
			{
				if (strcmp(argv[optind], "create") == 0)
					ctx->backup_command = RAMCTRL_BACKUP_CREATE;
				else if (strcmp(argv[optind], "restore") == 0)
					ctx->backup_command = RAMCTRL_BACKUP_RESTORE;
				else if (strcmp(argv[optind], "list") == 0)
					ctx->backup_command = RAMCTRL_BACKUP_LIST;
				else if (strcmp(argv[optind], "delete") == 0)
					ctx->backup_command = RAMCTRL_BACKUP_DELETE;
				else
				{
					ctx->backup_command = RAMCTRL_BACKUP_UNKNOWN;
					optind--; /* Don't consume unknown subcommand */
				}
				if (ctx->backup_command != RAMCTRL_BACKUP_UNKNOWN)
					optind++;
			}
			else
			{
				ctx->backup_command = RAMCTRL_BACKUP_UNKNOWN;
			}
		}
		else if (strcmp(argv[optind], "bootstrap") == 0)
		{
			ctx->command = RAMCTRL_CMD_BOOTSTRAP;
			optind++;
			if (optind < argc)
			{
				if (strcmp(argv[optind], "init") == 0)
					ctx->bootstrap_command = RAMCTRL_BOOTSTRAP_INIT;
				else if (strcmp(argv[optind], "run") == 0)
					ctx->bootstrap_command = RAMCTRL_BOOTSTRAP_RUN;
				else if (strcmp(argv[optind], "validate") == 0)
					ctx->bootstrap_command = RAMCTRL_BOOTSTRAP_VALIDATE;
				else
				{
					ctx->bootstrap_command = RAMCTRL_BOOTSTRAP_UNKNOWN;
					optind--; /* Don't consume unknown subcommand */
				}
				if (ctx->bootstrap_command != RAMCTRL_BOOTSTRAP_UNKNOWN)
					optind++;
			}
			else
			{
				ctx->bootstrap_command = RAMCTRL_BOOTSTRAP_UNKNOWN;
			}
		}
		else if (strcmp(argv[optind], "watch") == 0)
		{
			ctx->command = RAMCTRL_CMD_WATCH;
			optind++;
			if (optind < argc)
			{
				if (strcmp(argv[optind], "cluster") == 0)
					ctx->watch_command = RAMCTRL_WATCH_CLUSTER;
				else if (strcmp(argv[optind], "nodes") == 0)
					ctx->watch_command = RAMCTRL_WATCH_NODES;
				else if (strcmp(argv[optind], "replication") == 0)
					ctx->watch_command = RAMCTRL_WATCH_REPLICATION;
				else if (strcmp(argv[optind], "status") == 0)
					ctx->watch_command = RAMCTRL_WATCH_STATUS;
				else
				{
					ctx->watch_command = RAMCTRL_WATCH_CLUSTER; /* Default to cluster */
					optind--; /* Don't consume unknown subcommand */
				}
				if (ctx->watch_command != RAMCTRL_WATCH_UNKNOWN)
					optind++;
			}
			else
			{
				ctx->watch_command = RAMCTRL_WATCH_CLUSTER; /* Default to cluster */
			}
		}
	}

	/* Store remaining arguments */
	ctx->command_argc = 0;
	for (int i = optind; i < argc && ctx->command_argc < 16; i++)
	{
		strncpy(ctx->command_args[ctx->command_argc], argv[i],
		        sizeof(ctx->command_args[ctx->command_argc]) - 1);
		ctx->command_args[ctx->command_argc]
		                 [sizeof(ctx->command_args[ctx->command_argc]) - 1] =
		    '\0';
		ctx->command_argc++;
	}

	return true;
}


int ramctrl_execute_command(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	switch (ctx->command)
	{
	case RAMCTRL_CMD_STATUS:
		return ramctrl_cmd_status(ctx);
	case RAMCTRL_CMD_START:
		return ramctrl_cmd_start(ctx);
	case RAMCTRL_CMD_STOP:
		return ramctrl_cmd_stop(ctx);
	case RAMCTRL_CMD_RESTART:
		return ramctrl_cmd_restart(ctx);
	case RAMCTRL_CMD_PROMOTE:
		return ramctrl_cmd_promote(ctx);
	case RAMCTRL_CMD_DEMOTE:
		return ramctrl_cmd_demote(ctx);
	case RAMCTRL_CMD_FAILOVER:
		return ramctrl_cmd_failover(ctx);
	case RAMCTRL_CMD_LOGS:
		return ramctrl_cmd_logs(ctx);
	case RAMCTRL_CMD_HELP:
		if (ctx->command_argc > 0)
		{
			ramctrl_help_show_command(ctx->command_args[0]);
		}
		else
		{
			ramctrl_help_show_general();
			ramctrl_help_show_commands();
		}
		return RAMCTRL_EXIT_SUCCESS;
	case RAMCTRL_CMD_VERSION:
		ramctrl_version();
		return RAMCTRL_EXIT_SUCCESS;
	case RAMCTRL_CMD_SHOW:
		return ramctrl_cmd_show(ctx);
	case RAMCTRL_CMD_NODE:
		return ramctrl_cmd_node(ctx);
	case RAMCTRL_CMD_WATCH:
		return ramctrl_cmd_watch_new(ctx);
	case RAMCTRL_CMD_REPLICATION:
		return ramctrl_cmd_replication(ctx);
	case RAMCTRL_CMD_REPLICA:
		return ramctrl_cmd_replica(ctx);
	case RAMCTRL_CMD_BACKUP:
		return ramctrl_cmd_backup(ctx);
	case RAMCTRL_CMD_BOOTSTRAP:
		return ramctrl_cmd_bootstrap(ctx);
	case RAMCTRL_CMD_UNKNOWN:
	default:
		ramctrl_help_show_general();
		ramctrl_help_show_commands();
		return RAMCTRL_EXIT_SUCCESS;
	}
}


int main(int argc, char* argv[])
{
	ramctrl_context_t ctx;
	int result;

	/* Initialize context */
	if (!ramctrl_init(&ctx))
	{
		fprintf(stderr, "ramctrl: failed to initialize context\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	/* Parse command line arguments */
	if (!ramctrl_parse_args(&ctx, argc, argv))
	{
		fprintf(stderr, "ramctrl: invalid command line arguments\n");
		ramctrl_usage(argv[0]);
		ramctrl_cleanup(&ctx);
		return RAMCTRL_EXIT_USAGE;
	}

	/* Execute command */
	result = ramctrl_execute_command(&ctx);

	/* Cleanup */
	ramctrl_cleanup(&ctx);

	return result;
}
