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

void
ramctrl_usage(const char *progname)
{
	fprintf(stdout, "PostgreSQL RAM Control Utility (ramctrl) %s\n\n", RAMCTRL_VERSION_STRING);
	fprintf(stdout, "Usage: %s [OPTIONS] COMMAND [ARGS...]\n\n", progname);

	fprintf(stdout, "Global Options:\n");
	fprintf(stdout, "  -h, --host HOST       PostgreSQL host (default: localhost)\n");
	fprintf(stdout, "  -p, --port PORT       PostgreSQL port (default: 5432)\n");
	fprintf(stdout, "  -d, --database DB     Database name (default: postgres)\n");
	fprintf(stdout, "  -U, --user USER       Database user (default: postgres)\n");
	fprintf(stdout, "  -W, --password PASS   Database password\n");
	fprintf(stdout, "  -c, --config FILE     Configuration file path\n");
	fprintf(stdout, "  -v, --verbose         Verbose output\n");
	fprintf(stdout, "  -j, --json            JSON output format\n");
	fprintf(stdout, "  -t, --timeout SEC     Timeout in seconds (default: 30)\n");
	fprintf(stdout, "      --table            Table output format (overrides --json)\n");
	fprintf(stdout, "      --help            Show this help message\n");
	fprintf(stdout, "      --version         Show version information\n");
	fprintf(stdout, "\n");
    
    fprintf(stdout, "Commands:\n");
    fprintf(stdout, "  status                        Show cluster status\n");
    fprintf(stdout, "  start [NODE_ID]               Start ramd daemon (optionally on specific node)\n");
    fprintf(stdout, "  stop [NODE_ID]                Stop ramd daemon (optionally on specific node)\n");
    fprintf(stdout, "  restart [NODE_ID]             Restart ramd daemon (optionally on specific node)\n");
    fprintf(stdout, "  promote NODE_ID               Promote node to primary\n");
    fprintf(stdout, "  demote NODE_ID                Demote node from primary to standby\n");
    fprintf(stdout, "  failover [NODE_ID]            Trigger manual failover (optionally to specific node)\n");
    fprintf(stdout, "  show cluster                  Show cluster information\n");
    fprintf(stdout, "  show nodes                    Show all nodes status\n");
    fprintf(stdout, "  add-node NODE_ID HOST PORT    Add node to cluster\n");
    fprintf(stdout, "  remove-node NODE_ID           Remove node from cluster\n");
    fprintf(stdout, "  enable-maintenance NODE_ID    Enable maintenance mode\n");
    fprintf(stdout, "  disable-maintenance NODE_ID   Disable maintenance mode\n");
    fprintf(stdout, "  logs [NODE_ID]                Show daemon logs\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Replication Management:\n");
    fprintf(stdout, "  show replication              Show replication status and lag\n");
    fprintf(stdout, "  set replication-mode MODE     Set replication mode (async/sync_remote_write/sync_remote_apply/auto)\n");
    fprintf(stdout, "  set lag-threshold BYTES       Set maximum lag threshold for failover\n");
    fprintf(stdout, "  wal-e backup                  Create WAL-E backup\n");
    fprintf(stdout, "  wal-e restore BACKUP_NAME     Restore from WAL-E backup\n");
    fprintf(stdout, "  wal-e list                    List available WAL-E backups\n");
    fprintf(stdout, "  wal-e delete BACKUP_NAME      Delete WAL-E backup\n");
    fprintf(stdout, "  bootstrap run NODE_TYPE       Run bootstrap script (primary/standby)\n");
    fprintf(stdout, "  bootstrap validate            Validate bootstrap script\n");
    fprintf(stdout, "\n");
    
    fprintf(stdout, "Examples:\n");
    fprintf(stdout, "  %s status                      # Show cluster status\n", progname);
    fprintf(stdout, "  %s show cluster                # Show detailed cluster info\n", progname);
    fprintf(stdout, "  %s show nodes                  # Show all nodes status\n", progname);
    fprintf(stdout, "  %s promote 2                   # Promote node 2 to primary\n", progname);
    fprintf(stdout, "  %s failover                    # Trigger automatic failover\n", progname);
    fprintf(stdout, "  %s add-node 4 host4 5432       # Add node 4 to cluster\n", progname);
    fprintf(stdout, "  %s start                       # Start local ramd daemon\n", progname);
    fprintf(stdout, "  %s logs 1                      # Show logs for node 1\n", progname);
    fprintf(stdout, "\n");
    fprintf(stdout, "Output Formats:\n");
    fprintf(stdout, "  Default: Human-readable text\n");
    fprintf(stdout, "  --json:  JSON format for scripting\n");
    fprintf(stdout, "  --table: Professional table format\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Report bugs to <support@pgelephant.com>\n");
}

void
ramctrl_version(void)
{
    fprintf(stdout, "ramctrl (PostgreSQL RAM Control Utility) %s\n", RAMCTRL_VERSION_STRING);
    fprintf(stdout, "Copyright (c) 2024-2025, pgElephant, Inc.\n");
}

bool
ramctrl_init(ramctrl_context_t *ctx)
{
	if (!ctx)
		return false;

	memset(ctx, 0, sizeof(ramctrl_context_t));

	/* Set defaults */
	strncpy(ctx->hostname, "localhost", sizeof(ctx->hostname) - 1);
	ctx->port = 5432;
	strncpy(ctx->database, "postgres", sizeof(ctx->database) - 1);
	strncpy(ctx->user, "postgres", sizeof(ctx->user) - 1);
	ctx->timeout_seconds = 30;
	ctx->verbose = false;
	ctx->json_output = false;
	ctx->table_output = false;
	ctx->command = RAMCTRL_CMD_UNKNOWN;

	return true;
}

void
ramctrl_cleanup(ramctrl_context_t *ctx)
{
	if (!ctx)
		return;

	/* Clear sensitive information */
	memset(ctx->password, 0, sizeof(ctx->password));
}

/* Removed unused ramctrl_parse_command function */

bool
ramctrl_parse_args(ramctrl_context_t *ctx, int argc, char *argv[])
{
	int			c;
	int			option_index = 0;

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
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, "h:p:d:U:W:c:vjt:T",
							long_options, &option_index)) != -1)
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
                if (ctx->timeout_seconds <= 0) {
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
            if (optind + 1 < argc)
            {
                if (strcmp(argv[optind + 1], "cluster") == 0)
                    ctx->command = RAMCTRL_CMD_SHOW_CLUSTER;
                else if (strcmp(argv[optind + 1], "nodes") == 0)
                    ctx->command = RAMCTRL_CMD_SHOW_NODES;
                else if (strcmp(argv[optind + 1], "replication") == 0)
                    ctx->command = RAMCTRL_CMD_SHOW_REPLICATION;
                optind++; /* Skip the subcommand */
            }
        }
        else if (strcmp(argv[optind], "add-node") == 0)
            ctx->command = RAMCTRL_CMD_ADD_NODE;
        else if (strcmp(argv[optind], "remove-node") == 0)
            ctx->command = RAMCTRL_CMD_REMOVE_NODE;
        else if (strcmp(argv[optind], "enable-maintenance") == 0)
            ctx->command = RAMCTRL_CMD_ENABLE_MAINTENANCE;
        else if (strcmp(argv[optind], "disable-maintenance") == 0)
            ctx->command = RAMCTRL_CMD_DISABLE_MAINTENANCE;
        else if (strcmp(argv[optind], "logs") == 0)
            ctx->command = RAMCTRL_CMD_LOGS;
        else if (strcmp(argv[optind], "help") == 0)
            ctx->command = RAMCTRL_CMD_HELP;
        else if (strcmp(argv[optind], "version") == 0)
            ctx->command = RAMCTRL_CMD_VERSION;
        else if (strcmp(argv[optind], "set") == 0)
        {
            if (optind + 2 < argc)
            {
                if (strcmp(argv[optind + 1], "replication-mode") == 0)
                {
                    ctx->command = RAMCTRL_CMD_SET_REPLICATION_MODE;
                    optind += 2; /* Skip "set" and "replication-mode" */
                    /* Store the mode argument */
                    if (optind < argc)
                    {
                        strncpy(ctx->command_args[ctx->command_argc], argv[optind], sizeof(ctx->command_args[ctx->command_argc]) - 1);
                        ctx->command_args[ctx->command_argc][sizeof(ctx->command_args[ctx->command_argc]) - 1] = '\0';
                        ctx->command_argc++;
                        optind++;
                    }
                }
                else if (strcmp(argv[optind + 1], "lag-threshold") == 0)
                {
                    ctx->command = RAMCTRL_CMD_SET_LAG_THRESHOLD;
                    optind += 2; /* Skip "set" and "lag-threshold" */
                    /* Store the threshold argument */
                    if (optind < argc)
                    {
                        strncpy(ctx->command_args[ctx->command_argc], argv[optind], sizeof(ctx->command_args[ctx->command_argc]) - 1);
                        ctx->command_args[ctx->command_argc][sizeof(ctx->command_args[ctx->command_argc]) - 1] = '\0';
                        ctx->command_argc++;
                        optind++;
                    }
                }
            }
        }
        else if (strcmp(argv[optind], "wal-e") == 0)
        {
            if (optind + 1 < argc)
            {
                if (strcmp(argv[optind + 1], "backup") == 0)
                    ctx->command = RAMCTRL_CMD_WAL_E_BACKUP;
                else if (strcmp(argv[optind + 1], "restore") == 0)
                {
                    ctx->command = RAMCTRL_CMD_WAL_E_RESTORE;
                    optind += 2; /* Skip "wal-e" and "restore" */
                    /* Store the backup name argument */
                    if (optind < argc)
                    {
                        strncpy(ctx->command_args[ctx->command_argc], argv[optind], sizeof(ctx->command_args[ctx->command_argc]) - 1);
                        ctx->command_args[ctx->command_argc][sizeof(ctx->command_args[ctx->command_argc]) - 1] = '\0';
                        ctx->command_argc++;
                        optind++;
                    }
                }
                else if (strcmp(argv[optind + 1], "list") == 0)
                    ctx->command = RAMCTRL_CMD_WAL_E_LIST;
                else if (strcmp(argv[optind + 1], "delete") == 0)
                {
                    ctx->command = RAMCTRL_CMD_WAL_E_DELETE;
                    optind += 2; /* Skip "wal-e" and "delete" */
                    /* Store the backup name argument */
                    if (optind < argc)
                    {
                        strncpy(ctx->command_args[ctx->command_argc], argv[optind], sizeof(ctx->command_args[ctx->command_argc]) - 1);
                        ctx->command_args[ctx->command_argc][sizeof(ctx->command_args[ctx->command_argc]) - 1] = '\0';
                        ctx->command_argc++;
                        optind++;
                    }
                }
                else
                {
                    optind += 2; /* Skip "wal-e" and subcommand */
                }
            }
        }
        else if (strcmp(argv[optind], "bootstrap") == 0)
        {
            if (optind + 1 < argc)
            {
                if (strcmp(argv[optind + 1], "run") == 0)
                {
                    ctx->command = RAMCTRL_CMD_BOOTSTRAP_RUN;
                    optind += 2; /* Skip "bootstrap" and "run" */
                    /* Store the node type argument */
                    if (optind < argc)
                    {
                        strncpy(ctx->command_args[ctx->command_argc], argv[optind], sizeof(ctx->command_args[ctx->command_argc]) - 1);
                        ctx->command_args[ctx->command_argc][sizeof(ctx->command_args[ctx->command_argc]) - 1] = '\0';
                        ctx->command_argc++;
                        optind++;
                    }
                }
                else if (strcmp(argv[optind + 1], "validate") == 0)
                {
                    ctx->command = RAMCTRL_CMD_BOOTSTRAP_VALIDATE;
                    optind += 2; /* Skip "bootstrap" and "validate" */
                }
            }
        }
    }
    
    /* Store remaining arguments */
    ctx->command_argc = 0;
    for (int i = optind + 1; i < argc && ctx->command_argc < 16; i++)
    {
        strncpy(ctx->command_args[ctx->command_argc], argv[i], sizeof(ctx->command_args[ctx->command_argc]) - 1);
        ctx->command_args[ctx->command_argc][sizeof(ctx->command_args[ctx->command_argc]) - 1] = '\0';
        ctx->command_argc++;
    }
    
    return true;
}

int
ramctrl_execute_command(ramctrl_context_t *ctx)
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
		case RAMCTRL_CMD_SHOW_CLUSTER:
			return ramctrl_cmd_show_cluster(ctx);
		case RAMCTRL_CMD_SHOW_NODES:
			return ramctrl_cmd_show_nodes(ctx);
		case RAMCTRL_CMD_ADD_NODE:
			return ramctrl_cmd_add_node(ctx);
		case RAMCTRL_CMD_REMOVE_NODE:
			return ramctrl_cmd_remove_node(ctx);
		case RAMCTRL_CMD_ENABLE_MAINTENANCE:
			return ramctrl_cmd_enable_maintenance(ctx);
		case RAMCTRL_CMD_DISABLE_MAINTENANCE:
			return ramctrl_cmd_disable_maintenance(ctx);
		case RAMCTRL_CMD_LOGS:
			return ramctrl_cmd_logs(ctx);
		case RAMCTRL_CMD_HELP:
			ramctrl_usage("ramctrl");
			return RAMCTRL_EXIT_SUCCESS;
		case RAMCTRL_CMD_VERSION:
			ramctrl_version();
			return RAMCTRL_EXIT_SUCCESS;
		/* New replication management commands */
		case RAMCTRL_CMD_SHOW_REPLICATION:
			return ramctrl_cmd_show_replication(ctx);
		case RAMCTRL_CMD_SET_REPLICATION_MODE:
			return ramctrl_cmd_set_replication_mode(ctx);
		case RAMCTRL_CMD_SET_LAG_THRESHOLD:
			return ramctrl_cmd_set_lag_threshold(ctx);
		case RAMCTRL_CMD_WAL_E_BACKUP:
			return ramctrl_cmd_wal_e_backup(ctx);
		case RAMCTRL_CMD_WAL_E_RESTORE:
			return ramctrl_cmd_wal_e_restore(ctx);
		case RAMCTRL_CMD_WAL_E_LIST:
			return ramctrl_cmd_wal_e_list(ctx);
		case RAMCTRL_CMD_WAL_E_DELETE:
			return ramctrl_cmd_wal_e_delete(ctx);
		case RAMCTRL_CMD_BOOTSTRAP_RUN:
			return ramctrl_cmd_bootstrap_run(ctx);
		case RAMCTRL_CMD_BOOTSTRAP_VALIDATE:
			return ramctrl_cmd_bootstrap_validate(ctx);
		default:
			fprintf(stderr, "ramctrl: internal error: unknown command\n");
			return RAMCTRL_EXIT_FAILURE;
	}
}

int
main(int argc, char *argv[])
{
	ramctrl_context_t ctx;
	int			result;

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
