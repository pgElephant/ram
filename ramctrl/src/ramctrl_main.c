/*-------------------------------------------------------------------------
 *
 * ramctrl_main.c
 *		PostgreSQL RAM Control Utility - Main Entry Point
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "ramctrl.h"
#include "ramctrl_daemon.h"
#include "ramctrl_defaults.h"
#include "ramctrl_help.h"
#include "ramctrl_show.h"
#include "ramctrl_watch.h"

/* Professional CLI utilities */
static void ramctrl_print_banner(void);
static void ramctrl_print_error(const char* message);
static void ramctrl_print_progress(const char* message);

/* Enhanced error handling */
static void ramctrl_validate_api_url(ramctrl_context_t* ctx);

void
ramctrl_usage(const char *progname)
{
	ramctrl_print_banner();

	printf("USAGE:\n");
	printf("  %s [OPTIONS] COMMAND [ARGS...]\n\n", progname);

	printf("GLOBAL OPTIONS:\n");
	printf("  Connection Options:\n");
	printf("    -u, --api-url URL       ramd API endpoint (default: http://127.0.0.1:{{API_PORT}})\n");
	printf("    -c, --config FILE       Configuration file path\n");
	printf("\n");
	printf("  Output Options:\n");
	printf("    -v, --verbose           Verbose output with detailed information\n");
	printf("    -j, --json              JSON output format for scripting\n");
	printf("    --table                 Table output format (overrides --json)\n");
	printf("    --quiet                 Suppress non-error output\n");
	printf("\n");
	printf("  Behavior Options:\n");
	printf("    -t, --timeout SEC       Timeout in seconds (default: 30)\n");
	printf("    --force                 Skip confirmation prompts\n");
	printf("    --dry-run               Show what would be done without executing\n");
	printf("\n");
	printf("  Information Options:\n");
	printf("    --help                  Show this help message\n");
	printf("    --version               Show version information\n");
	printf("    --help-commands         Show detailed command help\n");
	printf("\n");

	printf("COMMANDS:\n");
	printf("  Cluster Management:\n");
	printf("    status                        Show cluster status and health\n");
	printf("    show cluster                  Show detailed cluster information\n");
	printf("    show nodes                    Show all nodes status\n");
	printf("    show replication              Show replication status and lag\n");
	printf("\n");
	printf("  Node Management:\n");
	printf("    add-node NODE_ID HOST PORT    Add node to cluster\n");
	printf("    remove-node NODE_ID           Remove node from cluster\n");
	printf("    promote NODE_ID               Promote node to primary\n");
	printf("    demote NODE_ID                Demote node from primary to standby\n");
	printf("    enable-maintenance NODE_ID    Enable maintenance mode\n");
	printf("    disable-maintenance NODE_ID   Disable maintenance mode\n");
	printf("\n");
	printf("  Daemon Control:\n");
	printf("    start [NODE_ID]               Start ramd daemon (optionally on specific node)\n");
	printf("    stop [NODE_ID]                Stop ramd daemon (optionally on specific node)\n");
	printf("    restart [NODE_ID]             Restart ramd daemon (optionally on specific node)\n");
	printf("    logs [NODE_ID]                Show daemon logs\n");
	printf("\n");
	printf("  Failover & Recovery:\n");
	printf("    failover [NODE_ID]            Trigger manual failover (optionally to specific node)\n");
	printf("    wal-e backup                  Create WAL-E backup\n");
	printf("    wal-e restore BACKUP_NAME     Restore from WAL-E backup\n");
	printf("    wal-e list                    List available WAL-E backups\n");
	printf("    wal-e delete BACKUP_NAME      Delete WAL-E backup\n");
	printf("\n");
	printf("  Configuration:\n");
	printf("    set replication-mode MODE     Set replication mode (async/sync_remote_write/sync_remote_apply/auto)\n");
	printf("    set lag-threshold BYTES       Set maximum lag threshold for failover\n");
	printf("    bootstrap run NODE_TYPE       Run bootstrap script (primary/standby)\n");
	printf("    bootstrap validate            Validate bootstrap script\n");
	printf("\n");
	printf("  Monitoring:\n");
	printf("    watch [cluster|nodes|replication] Watch mode for real-time monitoring\n");
	fprintf(stdout, "\n");

	printf("EXAMPLES:\n");
	printf("  # Basic cluster operations\n");
	printf("  %s status                              # Show cluster status\n", progname);
	printf("  %s show nodes                          # Show all nodes\n", progname);
	printf("  %s show cluster                        # Show cluster details\n", progname);
	printf("\n");
	printf("  # Node management\n");
	printf("  %s add-node 2 192.168.1.100 5432      # Add node 2\n", progname);
	printf("  %s remove-node 2                       # Remove node 2\n", progname);
	printf("  %s promote 2                           # Promote node 2 to primary\n", progname);
	printf("  %s demote 1                            # Demote node 1 to standby\n", progname);
	printf("\n");
	printf("  # Daemon control\n");
	printf("  %s start                               # Start ramd daemon\n", progname);
	printf("  %s stop 2                              # Stop daemon on node 2\n", progname);
	printf("  %s restart                             # Restart daemon\n", progname);
	printf("\n");
	printf("  # Advanced operations\n");
	printf("  %s --json show nodes                   # JSON output for scripting\n", progname);
	printf("  %s --verbose failover 2                # Verbose failover to node 2\n", progname);
	printf("  %s watch cluster                       # Real-time cluster monitoring\n", progname);
	printf("  %s --force remove-node 3               # Skip confirmation\n", progname);
	printf("\n");
	printf("  # Configuration\n");
	printf("  %s -u http://ramd.example.com:{{API_PORT}} status    # Connect to remote ramd\n", progname);
	printf("  %s -c {{ETC_DIR}}ramctrl.conf status               # Use custom config\n", progname);
	printf("\n");
	printf("OUTPUT FORMATS:\n");
	printf("  Default: Human-readable text with colors and formatting\n");
	printf("  --json:  JSON format for scripting and automation\n");
	printf("  --table: Professional table format for reports\n");
	printf("  --quiet: Minimal output (errors only)\n");
	printf("\n");
	printf("For more information, see the ramctrl manual page or use --help-commands.\n");
	printf("Report bugs to <support@pgelephant.com>\n");
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
	strncpy(ctx->api_url, "http://127.0.0.1:{{API_PORT}}", sizeof(ctx->api_url) - 1);
	ctx->api_url[sizeof(ctx->api_url) - 1] = '\0';
	ctx->timeout_seconds = RAMCTRL_DEFAULT_TIMEOUT_SECONDS;
	ctx->verbose = false;
	ctx->json_output = false;
	ctx->table_output = false;
	ctx->quiet = false;
	ctx->force = false;
	ctx->dry_run = false;
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

	/* No sensitive data to clear in HTTP-only mode */
}


bool ramctrl_parse_args(ramctrl_context_t* ctx, int argc, char* argv[])
{
	int c;
	int option_index = 0;

	static struct option long_options[] = {
	    {"api-url", required_argument, 0, 'u'},
	    {"config", required_argument, 0, 'c'},
	    {"verbose", no_argument, 0, 'v'},
	    {"json", no_argument, 0, 'j'},
	    {"timeout", required_argument, 0, 't'},
	    {"table", no_argument, 0, 'T'},
	    {"quiet", no_argument, 0, 'q'},
	    {"force", no_argument, 0, 'f'},
	    {"dry-run", no_argument, 0, 'n'},
	    {"help", no_argument, 0, 1000},
	    {"version", no_argument, 0, 1001},
	    {"help-commands", no_argument, 0, 1002},
	    {0, 0, 0, 0}};

	while ((c = getopt_long(argc, argv, "u:c:vjt:Tqfn", long_options,
	                        &option_index)) != -1)
	{
		switch (c)
		{
		case 'u':
			strncpy(ctx->api_url, optarg, sizeof(ctx->api_url) - 1);
			ctx->api_url[sizeof(ctx->api_url) - 1] = '\0';
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
		case 'q':
			ctx->quiet = true;
			break;
		case 'f':
			ctx->force = true;
			break;
		case 'n':
			ctx->dry_run = true;
			break;
		case 1000:
			if (ctx->command == RAMCTRL_CMD_UNKNOWN)
				ctx->command = RAMCTRL_CMD_HELP;
			return true;
		case 1001:
			if (ctx->command == RAMCTRL_CMD_UNKNOWN)
				ctx->command = RAMCTRL_CMD_VERSION;
			return true;
		case 1002:
			if (ctx->command == RAMCTRL_CMD_UNKNOWN)
				ctx->command = RAMCTRL_CMD_HELP;
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
				                 [sizeof(ctx->command_args[ctx->command_argc]) -
				                  1] = '\0';
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
					ctx->replication_command =
					    RAMCTRL_REPLICATION_SET_LAG_THRESHOLD;
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
					ctx->watch_command =
					    RAMCTRL_WATCH_CLUSTER; /* Default to cluster */
					optind--; /* Don't consume unknown subcommand */
				}
				if (ctx->watch_command != RAMCTRL_WATCH_UNKNOWN)
					optind++;
			}
			else
			{
				ctx->watch_command =
				    RAMCTRL_WATCH_CLUSTER; /* Default to cluster */
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
	{
		ramctrl_print_error("Context is null");
		return RAMCTRL_EXIT_FAILURE;
	}

	/* Validate API URL */
	ramctrl_validate_api_url(ctx);

	/* Show banner for interactive commands */
	if (!ctx->quiet && ctx->command != RAMCTRL_CMD_HELP && ctx->command != RAMCTRL_CMD_VERSION)
	{
		ramctrl_print_banner();
	}

	/* Execute command with professional feedback */
	ramctrl_print_progress("Executing command");
	
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

/* Professional CLI utilities implementation */

static void
ramctrl_print_banner(void)
{
	printf("\n");
	printf("╔══════════════════════════════════════════════════════════════╗\n");
	printf("║                PostgreSQL RAM Control Utility               ║\n");
	printf("║                    Professional CLI v%s                    ║\n", RAMCTRL_VERSION_STRING);
	printf("╚══════════════════════════════════════════════════════════════╝\n");
	printf("\n");
}

static void
ramctrl_print_error(const char* message)
{
	fprintf(stderr, "❌ Error: %s\n", message);
}

static void
ramctrl_print_progress(const char* message)
{
	printf("🔄 %s...\n", message);
	fflush(stdout);
}





/* Enhanced error handling implementation */


static void
ramctrl_validate_api_url(ramctrl_context_t* ctx)
{
	if (!ctx)
	{
		ramctrl_print_error("Context is null");
		return;
	}
	
	if (strlen(ctx->api_url) == 0UL)
	{
		ramctrl_print_error("API URL not specified");
		return;
	}
	
	/* Basic URL validation */
	if (strncmp(ctx->api_url, "http://", 7) != 0 && 
	    strncmp(ctx->api_url, "https://", 8) != 0)
	{
		ramctrl_print_error("API URL must start with http:// or https://");
		return;
	}
}


