/*-------------------------------------------------------------------------
 *
 * ramctrl_help.c
 *		PostgreSQL RAM Control Utility - Help System Implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "ramctrl.h"
#include "ramctrl_help.h"


void ramctrl_help_show_general(void)
{
	printf("Usage: ramctrl [OPTIONS] COMMAND [SUBCOMMAND] [ARGS...]\n\n");
	printf("PostgreSQL RAM Control Utility for managing RALE-based Auto "
	       "Manager clusters.\n");
	printf("Provides comprehensive cluster management, monitoring, and "
	       "maintenance capabilities.\n\n");
	printf("Commands:\n");
}


void ramctrl_help_show_commands(void)
{
	printf("Basic Commands:\n");
	printf("  %-20s Show cluster and daemon status\n", "status");
	printf("  %-20s Start the ramd daemon\n", "start");
	printf("  %-20s Stop the ramd daemon\n", "stop");
	printf("  %-20s Restart the ramd daemon\n", "restart");
	printf("  %-20s Promote current node to primary\n", "promote");
	printf("  %-20s Demote primary node to standby\n", "demote");
	printf("  %-20s Trigger manual failover\n", "failover");
	printf("  %-20s Show daemon logs\n", "logs");
	printf("  %-20s Show help information\n", "help [COMMAND]");
	printf("  %-20s Show version information\n", "version");
	printf("\nHierarchical Commands:\n");
	printf("  %-20s Show cluster information (try: ramctrl show)\n", "show");
	printf("  %-20s Node management operations (try: ramctrl node)\n", "node");
	printf("  %-20s Real-time monitoring (try: ramctrl watch)\n", "watch");
	printf("  %-20s Replication management (try: ramctrl replication)\n",
	       "replication");
	printf("  %-20s Backup operations (try: ramctrl backup)\n", "backup");
	printf("  %-20s Bootstrap operations (try: ramctrl bootstrap)\n",
	       "bootstrap");
	printf("\nOptions:\n");
	printf("  -h, --host HOST       PostgreSQL host\n");
	printf("  -p, --port PORT       PostgreSQL port\n");
	printf("  -d, --database DB     Database name\n");
	printf("  -U, --user USER       Database user\n");
	printf("  -c, --config FILE     Configuration file path\n");
	printf("  -v, --verbose         Verbose output\n");
	printf("  -j, --json            JSON output format\n");
	printf("  -t, --timeout SEC     Timeout in seconds\n");
	printf("      --table           Table output format\n");
	printf("      --help            Show this help message\n");
	printf("      --version         Show version information\n");
	printf("\nExamples:\n");
	printf("  ramctrl status                Show cluster status\n");
	printf("  ramctrl show cluster          Show detailed cluster info\n");
	printf("  ramctrl node add 2 host2 5432 Add node 2 to cluster\n");
	printf("  ramctrl watch cluster         Watch cluster in real-time\n");
	printf("\nFor subcommand help: ramctrl COMMAND (without arguments)\n");
}


void ramctrl_help_show_command(const char* command)
{
	if (!command)
	{
		ramctrl_help_show_general();
		return;
	}

	if (strcmp(command, "status") == 0)
		ramctrl_help_show_status();
	else if (strcmp(command, "start") == 0 || strcmp(command, "stop") == 0 ||
	         strcmp(command, "restart") == 0)
		ramctrl_help_show_start_stop();
	else if (strcmp(command, "cluster") == 0 ||
	         strcmp(command, "promote") == 0 ||
	         strcmp(command, "demote") == 0 || strcmp(command, "failover") == 0)
		ramctrl_help_show_cluster_management();
	else if (strcmp(command, "replication") == 0)
		ramctrl_help_show_replication();
	else if (strcmp(command, "backup") == 0 || strcmp(command, "wal-e") == 0)
		ramctrl_help_show_backup();
	else if (strcmp(command, "watch") == 0)
		ramctrl_help_show_watch();
	else if (strcmp(command, "examples") == 0)
		ramctrl_help_show_examples();
	else if (strcmp(command, "configuration") == 0 ||
	         strcmp(command, "config") == 0)
		ramctrl_help_show_configuration();
	else if (strcmp(command, "troubleshooting") == 0)
		ramctrl_help_show_troubleshooting();
	else
	{
		printf("Unknown command: %s\n\n", command);
		ramctrl_help_show_commands();
	}
}


void ramctrl_help_show_status(void)
{
	printf("Usage: ramctrl status [OPTIONS]\n\n");
	printf("Show cluster and daemon status information.\n\n");
	printf("Options:\n");
	printf("  --json        Output in JSON format\n");
	printf("  --verbose     Show detailed information\n\n");
	printf("This displays daemon status (running/stopped), cluster name, node "
	       "count,\n");
	printf("health status, primary and leader node information.\n");
}


void ramctrl_help_show_start_stop(void)
{
	printf("Usage: ramctrl {start|stop|restart} [OPTIONS]\n\n");
	printf("Control the ramd daemon lifecycle.\n\n");
	printf("Options:\n");
	printf("  --config FILE    Use specific configuration file\n");
	printf("  --verbose        Show detailed information\n\n");
	printf("Commands:\n");
	printf("  start     Start the daemon\n");
	printf("  stop      Stop the daemon gracefully\n");
	printf("  restart   Stop and start the daemon\n");
}


void ramctrl_help_show_cluster_management(void)
{
	printf("Usage: ramctrl {promote|demote|failover} [OPTIONS]\n\n");
	printf("Manage cluster leadership and handle failover scenarios.\n\n");
	printf("Commands:\n");
	printf("  promote     Promote current node to primary\n");
	printf("  demote      Demote primary node to standby\n");
	printf("  failover    Trigger manual failover\n\n");
	printf("Options:\n");
	printf("  --force     Force operation without safety checks\n");
	printf("  --timeout   Set operation timeout (seconds)\n");
}


void ramctrl_help_show_replication(void)
{
	printf("Usage: ramctrl {show "
	       "replication|set-replication-mode|set-lag-threshold}\n\n");
	printf("Monitor and configure PostgreSQL replication settings.\n\n");
	printf("Commands:\n");
	printf("  show replication       Show replication status and lag\n");
	printf("  set-replication-mode   Set mode (sync/async)\n");
	printf("  set-lag-threshold      Set acceptable lag threshold\n\n");
	printf("Replication modes:\n");
	printf("  async                  Asynchronous replication\n");
	printf("  sync                   Synchronous replication\n");
}


void ramctrl_help_show_backup(void)
{
	printf("Usage: ramctrl wal-e-{backup|restore|list|delete} [OPTIONS]\n\n");
	printf("Manage PostgreSQL backups using WAL-E integration.\n\n");
	printf("Commands:\n");
	printf("  wal-e-backup           Create a new backup\n");
	printf("  wal-e-restore BACKUP   Restore from specific backup\n");
	printf("  wal-e-list             List available backups\n");
	printf("  wal-e-delete BACKUP    Delete specific backup\n\n");
	printf("Examples:\n");
	printf("  ramctrl wal-e-backup --name daily_backup\n");
	printf("  ramctrl wal-e-list\n");
	printf("  ramctrl wal-e-restore backup_20240101\n");
}


void ramctrl_help_show_watch(void)
{
	printf("Usage: ramctrl watch [COMPONENT] [OPTIONS]\n\n");
	printf("Monitor cluster status in real-time with automatic refresh.\n\n");
	printf("Components:\n");
	printf("  cluster               Watch cluster information\n");
	printf("  nodes                 Watch node status\n");
	printf("  replication           Watch replication status\n\n");
	printf("Options:\n");
	printf("  --interval SECONDS    Set refresh interval (default: 2)\n");
	printf("  --count NUM           Limit number of updates\n");
	printf("  --json                Output in JSON format\n\n");
	printf("Controls:\n");
	printf("  Ctrl+C                Exit watch mode\n");
}


void ramctrl_help_show_examples(void)
{
	printf("Examples:\n\n");
	printf("Basic operations:\n");
	printf("  ramctrl status\n");
	printf("  ramctrl start --config /etc/ramd/production.conf\n");
	printf("  ramctrl show cluster --verbose\n\n");
	printf("Cluster management:\n");
	printf("  ramctrl promote\n");
	printf("  ramctrl add-node --hostname db2.example.com --port 5432\n");
	printf("  ramctrl failover\n\n");
	printf("Monitoring:\n");
	printf("  ramctrl watch cluster\n");
	printf("  ramctrl logs --lines 50\n");
	printf("  ramctrl show replication\n\n");
	printf("Backup operations:\n");
	printf("  ramctrl wal-e-backup --name daily_backup\n");
	printf("  ramctrl wal-e-list\n");
	printf("  ramctrl wal-e-restore backup_20240101\n");
}


void ramctrl_help_show_configuration(void)
{
	printf("Configuration:\n\n");
	printf("Configuration files:\n");
	printf("  /etc/ramd/ramd.conf      System-wide configuration\n");
	printf("  ~/.ramctrl/config        User configuration\n");
	printf("  --config FILE            Custom configuration file\n\n");
	printf("Environment variables:\n");
	printf("  RAMCTRL_CONFIG      Override default config file\n");
	printf("  RAMCTRL_API_URL     ramd API endpoint URL\n");
	printf("  PGHOST              PostgreSQL host\n");
	printf("  PGPORT              PostgreSQL port\n");
	printf("  PGUSER              PostgreSQL user\n");
	printf("  PGDATABASE          PostgreSQL database\n\n");
	printf("Configuration sections:\n");
	printf("  [cluster]           Cluster identification and settings\n");
	printf("  [node]              Current node configuration\n");
	printf("  [replication]       Replication parameters\n");
	printf("  [backup]            WAL-E backup settings\n");
}


void ramctrl_help_show_troubleshooting(void)
{
	printf("Troubleshooting:\n\n");
	printf("Common issues:\n\n");
	printf("Daemon not starting:\n");
	printf("  - Check if ramd binary is in PATH\n");
	printf("  - Verify configuration file syntax\n");
	printf("  - Check PostgreSQL connectivity\n");
	printf("  - Review logs: ramctrl logs\n\n");
	printf("Connection issues:\n");
	printf("  - Verify daemon is running: ramctrl status\n");
	printf("  - Check API endpoint configuration\n");
	printf("  - Test network connectivity\n");
	printf("  - Check firewall settings\n\n");
	printf("Cluster formation problems:\n");
	printf("  - Ensure all nodes can communicate\n");
	printf("  - Check cluster configuration consistency\n");
	printf("  - Verify PostgreSQL replication setup\n\n");
	printf("Diagnostic commands:\n");
	printf("  ramctrl status --verbose\n");
	printf("  ramctrl show cluster\n");
	printf("  ramctrl show nodes\n");
	printf("  ramctrl logs --lines 100\n\n");
	printf("Log locations:\n");
	printf("  /var/log/ramd/ramd.log\n");
	printf("  PostgreSQL logs (check postgresql.conf)\n");
}


void ramctrl_show_help(void)
{
	printf("Usage: ramctrl show SUBCOMMAND [OPTIONS]\n\n");
	printf("Display cluster information.\n\n");
	printf("Subcommands:\n");
	printf("  %-15s Show cluster information\n", "cluster");
	printf("  %-15s Show node status\n", "nodes");
	printf("  %-15s Show replication status\n", "replication");
	printf("  %-15s Show overall status\n", "status");
	printf("  %-15s Show configuration\n", "config");
	printf("  %-15s Show recent logs\n", "logs");
	printf("\nOptions:\n");
	printf("  --json          Output in JSON format\n");
	printf("  --verbose       Show detailed information\n");
	printf("\nExamples:\n");
	printf("  ramctrl show cluster\n");
	printf("  ramctrl show nodes --json\n");
	printf("  ramctrl show replication --verbose\n");
}


void ramctrl_node_help(void)
{
	printf("Usage: ramctrl node SUBCOMMAND [OPTIONS]\n\n");
	printf("Manage cluster nodes.\n\n");
	printf("Subcommands:\n");
	printf("  %-15s Add a node to the cluster\n", "add ID HOST PORT");
	printf("  %-15s Remove a node from the cluster\n", "remove ID");
	printf("  %-15s List all nodes\n", "list");
	printf("  %-15s Show node status\n", "status [ID]");
	printf("  %-15s Enable maintenance mode\n", "maintenance-on ID");
	printf("  %-15s Disable maintenance mode\n", "maintenance-off ID");
	printf("\nOptions:\n");
	printf("  --force         Force operation without confirmation\n");
	printf("  --timeout SEC   Set operation timeout\n");
	printf("\nExamples:\n");
	printf("  ramctrl node add 2 db2.example.com 5432\n");
	printf("  ramctrl node remove 2\n");
	printf("  ramctrl node list\n");
	printf("  ramctrl node maintenance-on 2\n");
}


void ramctrl_watch_help(void)
{
	printf("Usage: ramctrl watch [COMPONENT] [OPTIONS]\n\n");
	printf("Monitor cluster in real-time.\n\n");
	printf("Components:\n");
	printf("  %-15s Watch cluster status (default)\n", "cluster");
	printf("  %-15s Watch node status\n", "nodes");
	printf("  %-15s Watch replication status\n", "replication");
	printf("  %-15s Watch overall status\n", "status");
	printf("\nOptions:\n");
	printf("  --interval SEC  Refresh interval (default: 2)\n");
	printf("  --count NUM     Limit number of updates\n");
	printf("  --json          Output in JSON format\n");
	printf("\nControls:\n");
	printf("  Ctrl+C          Exit watch mode\n");
	printf("\nExamples:\n");
	printf("  ramctrl watch\n");
	printf("  ramctrl watch nodes --interval 5\n");
	printf("  ramctrl watch replication --count 10\n");
}


void ramctrl_replication_help(void)
{
	printf("Usage: ramctrl replication SUBCOMMAND [OPTIONS]\n\n");
	printf("Manage PostgreSQL replication.\n\n");
	printf("Subcommands:\n");
	printf("  %-15s Show replication status\n", "status");
	printf("  %-15s Set replication mode\n", "set-mode MODE");
	printf("  %-15s Set lag threshold\n", "set-lag BYTES");
	printf("  %-15s Show replication slots\n", "slots");
	printf("\nReplication modes:\n");
	printf("  async           Asynchronous replication\n");
	printf("  sync            Synchronous replication\n");
	printf("\nExamples:\n");
	printf("  ramctrl replication status\n");
	printf("  ramctrl replication set-mode sync\n");
	printf("  ramctrl replication set-lag 16MB\n");
	printf("  ramctrl replication slots\n");
}


void ramctrl_replica_help(void)
{
	printf("Usage: ramctrl replica SUBCOMMAND [OPTIONS]\n\n");
	printf("Manage PostgreSQL replicas with automatic pgraft consensus "
	       "integration.\n\n");
	printf("Subcommands:\n");
	printf("  %-15s Add new replica to cluster\n", "add HOSTNAME [PORT]");
	printf("  %-15s Remove replica from cluster\n", "remove NODE_ID");
	printf("  %-15s List all replicas\n", "list");
	printf("  %-15s Show replica status\n", "status");
	printf("\nThe 'add' command will:\n");
	printf("  - Create PostgreSQL replica with pg_basebackup\n");
	printf("  - Configure replication settings automatically\n");
	printf("  - Install and setup pgraft extension\n");
	printf("  - Join librale consensus automatically\n");
	printf("\nExamples:\n");
	printf("  ramctrl replica add replica1.example.com\n");
	printf("  ramctrl replica add 192.168.1.100 5433\n");
	printf("  ramctrl replica list\n");
	printf("  ramctrl replica status\n");
}


void ramctrl_backup_help(void)
{
	printf("Usage: ramctrl backup SUBCOMMAND [OPTIONS]\n\n");
	printf("Manage PostgreSQL backups.\n\n");
	printf("Subcommands:\n");
	printf("  %-15s Create a new backup\n", "create [NAME]");
	printf("  %-15s Restore from backup\n", "restore BACKUP");
	printf("  %-15s List available backups\n", "list");
	printf("  %-15s Delete a backup\n", "delete BACKUP");
	printf("\nOptions:\n");
	printf("  --name NAME     Set backup name\n");
	printf("  --compress      Enable compression\n");
	printf("  --target-time   Point-in-time recovery target\n");
	printf("\nExamples:\n");
	printf("  ramctrl backup create daily_backup\n");
	printf("  ramctrl backup list\n");
	printf("  ramctrl backup restore backup_20240101\n");
	printf("  ramctrl backup delete old_backup\n");
}


void ramctrl_bootstrap_help(void)
{
	printf("Usage: ramctrl bootstrap SUBCOMMAND [OPTIONS]\n\n");
	printf("Initialize and manage cluster bootstrap.\n\n");
	printf("Subcommands:\n");
	printf("  %-15s Initialize new cluster\n", "init");
	printf("  %-15s Run bootstrap process\n", "run TYPE");
	printf("  %-15s Validate bootstrap config\n", "validate");
	printf("\nBootstrap types:\n");
	printf("  primary         Bootstrap as primary node\n");
	printf("  standby         Bootstrap as standby node\n");
	printf("\nExamples:\n");
	printf("  ramctrl bootstrap init\n");
	printf("  ramctrl bootstrap run primary\n");
	printf("  ramctrl bootstrap validate\n");
}
