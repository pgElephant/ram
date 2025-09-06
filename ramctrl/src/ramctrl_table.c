/*-------------------------------------------------------------------------
 *
 * ramctrl_table.c
 *		Table formatting utilities for ramctrl output.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ramctrl_table.h"

#define MAX_KEY_WIDTH 25
#define MAX_VALUE_WIDTH 55
#define TABLE_WIDTH 85

void ramctrl_table_print_header(const char* title)
{
	int i;

	fprintf(stdout, "\n%s\n", title);
	for (i = 0; i < (int) strlen(title); i++)
	{
		fprintf(stdout, "─");
	}
	fprintf(stdout, "\n");
}


void ramctrl_table_print_separator(void)
{
	/* No separators - clean modern look */
}


void ramctrl_table_print_row(const char* key, const char* value)
{
	if (!key || !value)
		return;

	fprintf(stdout, "%-*s  %s\n", MAX_KEY_WIDTH, key, value);
}


void ramctrl_table_print_row_int(const char* key, int value)
{
	char value_str[32];

	snprintf(value_str, sizeof(value_str), "%d", value);
	ramctrl_table_print_row(key, value_str);
}


void ramctrl_table_print_row_bool(const char* key, bool value)
{
	ramctrl_table_print_row(key, value ? "Yes" : "No");
}


void ramctrl_table_print_row_time(const char* key, time_t timestamp)
{
	char time_str[64];
	struct tm* tm_info;

	if (timestamp > 0)
	{
		tm_info = localtime(&timestamp);
		strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
	}
	else
	{
		strcpy(time_str, "N/A");
	}
	ramctrl_table_print_row(key, time_str);
}


void ramctrl_table_print_footer(void)
{
	fprintf(stdout, "\n");
}


void ramctrl_table_print_daemon_status(const ramctrl_daemon_status_t* status)
{
	if (!status)
		return;

	fprintf(stdout, "\nDaemon Status:\n");
	ramctrl_table_print_row_bool("Running", status->is_running);

	if (status->is_running)
	{
		ramctrl_table_print_row_int("PID", status->pid);
		ramctrl_table_print_row_time("Started", status->start_time);
		ramctrl_table_print_row("Binary", "ramd");
		ramctrl_table_print_row("Config", "from environment/config");
		ramctrl_table_print_row("Log File", "from configuration");
		ramctrl_table_print_row("PID File", "from configuration");
	}
	else
	{
		ramctrl_table_print_row("Binary", "ramd");
		ramctrl_table_print_row("Config", "from environment/config");
		ramctrl_table_print_row("Log File", "from configuration");
		ramctrl_table_print_row("PID File", "from configuration");
	}
}


void ramctrl_table_print_cluster_status(const ramctrl_cluster_info_t* cluster)
{
	const char* state_str;

	if (!cluster)
		return;

	fprintf(stdout, "\nCluster Information:\n");
	ramctrl_table_print_row_int("ID", cluster->cluster_id);
	ramctrl_table_print_row("Name", cluster->cluster_name);
	ramctrl_table_print_row_int("Total Nodes", cluster->total_nodes);
	ramctrl_table_print_row_int("Active Nodes", cluster->active_nodes);
	ramctrl_table_print_row_int("Primary Node", cluster->primary_node_id);

	switch (cluster->status)
	{
	case RAMCTRL_CLUSTER_STATUS_HEALTHY:
		state_str = "Healthy";
		break;
	case RAMCTRL_CLUSTER_STATUS_DEGRADED:
		state_str = "Degraded";
		break;
	case RAMCTRL_CLUSTER_STATUS_FAILED:
		state_str = "Failed";
		break;
	case RAMCTRL_CLUSTER_STATUS_MAINTENANCE:
		state_str = "Maintenance";
		break;
	default:
		state_str = "Unknown";
		break;
	}
	ramctrl_table_print_row("State", state_str);
	ramctrl_table_print_row_time("Last Update", cluster->last_update);

	/* Show replication and configuration info */
	fprintf(stdout, "\nReplication & Configuration:\n");
	ramctrl_table_print_row("Replication Mode", "Synchronous");
	ramctrl_table_print_row("Sync Level", "remote_apply");
	ramctrl_table_print_row("Max Sync Standbys", "1");
	ramctrl_table_print_row("Auto Sync Mode", "Enabled");
	ramctrl_table_print_row("Config Reload", "Enabled (SIGHUP)");
	ramctrl_table_print_row("Maintenance Mode", "Available");
	ramctrl_table_print_row("Failover Strategy", "Automatic");
	ramctrl_table_print_row("HTTP API", "Enabled (port 8080)");
}


void ramctrl_table_print_node_status(const ramctrl_node_info_t* node)
{
	char lsn_str[32];
	char lag_str[32];

	if (!node)
		return;

	fprintf(stdout, "\nNode Information:\n");
	ramctrl_table_print_row_int("Node ID", node->node_id);
	ramctrl_table_print_row("Hostname", node->hostname);
	ramctrl_table_print_row_int("Port", node->port);
	ramctrl_table_print_row_bool("Is Leader", node->is_leader);
	ramctrl_table_print_row_bool("Is Healthy", node->is_healthy);
	ramctrl_table_print_row_int("Health Score", (int) node->health_score);

	if (node->wal_lsn > 0)
	{
		snprintf(lsn_str, sizeof(lsn_str), "%lld", (long long) node->wal_lsn);
		ramctrl_table_print_row("WAL LSN", lsn_str);
	}

	if (node->replication_lag_ms > 0)
	{
		snprintf(lag_str, sizeof(lag_str), "%d ms", node->replication_lag_ms);
		ramctrl_table_print_row("Replication Lag", lag_str);
	}
}
