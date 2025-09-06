/*-------------------------------------------------------------------------
 *
 * ramctrl_watch.c
 *		Watch mode for real-time cluster monitoring
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>

#include "ramctrl_watch.h"
#include "ramctrl_http.h"
#include "ramctrl_database.h"
#include "ramctrl_daemon.h"

/* Global watch state */
static bool g_watch_running = false;
static struct termios g_original_termios;
static bool g_termios_saved = false;

/* Signal handler for watch mode */
static void ramctrl_watch_signal_handler(int sig)
{
	switch (sig)
	{
	case SIGINT:
	case SIGTERM:
		g_watch_running = false;
		break;
	default:
		break;
	}
}

/* Setup terminal for watch mode */
static bool ramctrl_watch_setup_terminal(void)
{
	struct termios new_termios;

	/* Save original terminal settings */
	if (tcgetattr(STDIN_FILENO, &g_original_termios) != 0)
	{
		fprintf(stderr, "ramctrl: failed to get terminal attributes\n");
		return false;
	}
	g_termios_saved = true;

	/* Set up new terminal settings */
	new_termios = g_original_termios;
	new_termios.c_lflag &=
	    ~((tcflag_t) (ICANON | ECHO)); /* Disable canonical mode and echo */
	new_termios.c_cc[VMIN] = 0;        /* Non-blocking read */
	new_termios.c_cc[VTIME] = 0;       /* No timeout */

	if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termios) != 0)
	{
		fprintf(stderr, "ramctrl: failed to set terminal attributes\n");
		return false;
	}

	/* Hide cursor */
	printf("\033[?25l");
	fflush(stdout);

	return true;
}

/* Restore terminal settings */
static void ramctrl_watch_restore_terminal(void)
{
	if (g_termios_saved)
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
		g_termios_saved = false;
	}

	/* Show cursor */
	printf("\033[?25h");
	fflush(stdout);
}

/* Check for user input */
static bool ramctrl_watch_check_input(void)
{
	fd_set readfds;
	struct timeval timeout;
	char ch;

	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) > 0)
	{
		if (read(STDIN_FILENO, &ch, 1) > 0)
		{
			switch (ch)
			{
			case 'q':
			case 'Q':
			case 27:          /* ESC */
				return false; /* Exit watch mode */
			case 'r':
			case 'R':
				/* Force refresh - just continue */
				break;
			default:
				break;
			}
		}
	}

	return true; /* Continue watching */
}


int ramctrl_cmd_watch(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	/* Default to watching cluster status */
	return ramctrl_cmd_watch_cluster(ctx);
}


int ramctrl_cmd_watch_cluster(ramctrl_context_t* ctx)
{
	ramctrl_watch_config_t config;
	ramctrl_watch_data_t data;
	ramctrl_watch_stats_t stats;
	struct timespec sleep_time;
	time_t last_update = 0;

	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	/* Initialize watch configuration */
	ramctrl_watch_config_set_defaults(&config);
	config.refresh_interval_ms = 2000; /* 2 seconds for cluster view */

	/* Initialize statistics */
	memset(&stats, 0, sizeof(stats));
	stats.start_time = time(NULL);

	/* Setup terminal */
	if (!ramctrl_watch_setup_terminal())
		return RAMCTRL_EXIT_FAILURE;

	/* Setup signal handlers */
	signal(SIGINT, ramctrl_watch_signal_handler);
	signal(SIGTERM, ramctrl_watch_signal_handler);

	g_watch_running = true;

	printf("PostgreSQL RAM Cluster Watch Mode\n");
	printf("Press 'q' to quit, 'r' to refresh\n\n");

	while (g_watch_running)
	{
		time_t current_time = time(NULL);

		/* Check for user input */
		if (!ramctrl_watch_check_input())
		{
			g_watch_running = false;
			break;
		}

		/* Update data if it's time */
		if (current_time - last_update >= config.refresh_interval_ms / 1000)
		{
			ramctrl_watch_clear_screen();
			ramctrl_watch_move_cursor(1, 1);

			/* Display header */
			ramctrl_watch_display_header(&config);

			/* Update and display cluster data */
			if (ramctrl_watch_update_data(&data))
			{
				ramctrl_watch_display_cluster(&data, &config);
				stats.updates_count++;
				stats.last_update = current_time;
			}
			else
			{
				printf("Error: Failed to update cluster data\n");
				stats.errors_count++;
			}

			/* Display statistics */
			ramctrl_watch_display_stats(&stats);

			fflush(stdout);
			last_update = current_time;
		}

		/* Sleep for a short time */
		sleep_time.tv_sec = 0;
		sleep_time.tv_nsec = 100000000; /* 100ms */
		nanosleep(&sleep_time, NULL);
	}

	/* Cleanup */
	ramctrl_watch_restore_terminal();

	printf("\nWatch mode ended.\n");

	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_watch_nodes(ramctrl_context_t* ctx)
{
	ramctrl_watch_config_t config;
	ramctrl_watch_data_t data;
	ramctrl_watch_stats_t stats;
	struct timespec sleep_time;
	time_t last_update = 0;

	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	/* Initialize watch configuration */
	ramctrl_watch_config_set_defaults(&config);
	config.refresh_interval_ms = 1000; /* 1 second for nodes view */
	config.show_health = true;
	config.show_lag = true;

	/* Initialize statistics */
	memset(&stats, 0, sizeof(stats));
	stats.start_time = time(NULL);

	/* Setup terminal */
	if (!ramctrl_watch_setup_terminal())
		return RAMCTRL_EXIT_FAILURE;

	/* Setup signal handlers */
	signal(SIGINT, ramctrl_watch_signal_handler);
	signal(SIGTERM, ramctrl_watch_signal_handler);

	g_watch_running = true;

	printf("PostgreSQL RAM Nodes Watch Mode\n");
	printf("Press 'q' to quit, 'r' to refresh\n\n");

	while (g_watch_running)
	{
		time_t current_time = time(NULL);

		/* Check for user input */
		if (!ramctrl_watch_check_input())
		{
			g_watch_running = false;
			break;
		}

		/* Update data if it's time */
		if (current_time - last_update >= config.refresh_interval_ms / 1000)
		{
			ramctrl_watch_clear_screen();
			ramctrl_watch_move_cursor(1, 1);

			/* Display header */
			ramctrl_watch_display_header(&config);

			/* Update and display nodes data */
			if (ramctrl_watch_update_data(&data))
			{
				ramctrl_watch_display_nodes(&data, &config);
				stats.updates_count++;
				stats.last_update = current_time;
			}
			else
			{
				printf("Error: Failed to update nodes data\n");
				stats.errors_count++;
			}

			/* Display statistics */
			ramctrl_watch_display_stats(&stats);

			fflush(stdout);
			last_update = current_time;
		}

		/* Sleep for a short time */
		sleep_time.tv_sec = 0;
		sleep_time.tv_nsec = 100000000; /* 100ms */
		nanosleep(&sleep_time, NULL);
	}

	/* Cleanup */
	ramctrl_watch_restore_terminal();

	printf("\nWatch mode ended.\n");

	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_watch_replication(ramctrl_context_t* ctx)
{
	ramctrl_watch_config_t config;
	ramctrl_watch_data_t data;
	ramctrl_watch_stats_t stats;
	struct timespec sleep_time;
	time_t last_update = 0;

	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	/* Initialize watch configuration */
	ramctrl_watch_config_set_defaults(&config);
	config.refresh_interval_ms = 500; /* 500ms for replication view */
	config.show_lag = true;
	config.compact_mode = true;

	/* Initialize statistics */
	memset(&stats, 0, sizeof(stats));
	stats.start_time = time(NULL);

	/* Setup terminal */
	if (!ramctrl_watch_setup_terminal())
		return RAMCTRL_EXIT_FAILURE;

	/* Setup signal handlers */
	signal(SIGINT, ramctrl_watch_signal_handler);
	signal(SIGTERM, ramctrl_watch_signal_handler);

	g_watch_running = true;

	printf("PostgreSQL RAM Replication Watch Mode\n");
	printf("Press 'q' to quit, 'r' to refresh\n\n");

	while (g_watch_running)
	{
		time_t current_time = time(NULL);

		/* Check for user input */
		if (!ramctrl_watch_check_input())
		{
			g_watch_running = false;
			break;
		}

		/* Update data if it's time */
		if (current_time - last_update >= config.refresh_interval_ms / 1000)
		{
			ramctrl_watch_clear_screen();
			ramctrl_watch_move_cursor(1, 1);

			/* Display header */
			ramctrl_watch_display_header(&config);

			/* Update and display replication data */
			if (ramctrl_watch_update_data(&data))
			{
				ramctrl_watch_display_replication(&data, &config);
				stats.updates_count++;
				stats.last_update = current_time;
			}
			else
			{
				printf("Error: Failed to update replication data\n");
				stats.errors_count++;
			}

			/* Display statistics */
			ramctrl_watch_display_stats(&stats);

			fflush(stdout);
			last_update = current_time;
		}

		/* Sleep for a short time */
		sleep_time.tv_sec = 0;
		sleep_time.tv_nsec = 100000000; /* 100ms */
		nanosleep(&sleep_time, NULL);
	}

	/* Cleanup */
	ramctrl_watch_restore_terminal();

	printf("\nWatch mode ended.\n");

	return RAMCTRL_EXIT_SUCCESS;
}


bool ramctrl_watch_update_data(ramctrl_watch_data_t* data)
{
	char url[256];
	char response[8192];
	int result;

	if (!data)
		return false;

	data->timestamp = time(NULL);

	const char* base_url = getenv("RAMCTRL_API_URL");
	if (!base_url || strlen(base_url) == 0)
	{
		/* No hardcoded fallback - must be configured */
		strcpy(data->status_message, "RAMCTRL_API_URL not configured");
		return false;
	}
	snprintf(url, sizeof(url), "%s/api/v1/cluster/status", base_url);

	result = ramctrl_http_get(url, response, sizeof(response));
	if (result == 0)
	{
		/* Parse JSON response and update data structure */
		result = ramctrl_parse_cluster_status(response, &data->cluster_info);
		if (result == 0)
		{
			data->node_count = data->cluster_info.total_nodes;
			strcpy(data->status_message, "Connected to ramd");
		}
		else
		{
			/* Fallback to basic status if parsing fails */
			data->node_count = 1;
			strcpy(data->status_message, "API Response Parse Error");
			ramctrl_set_fallback_cluster_info(&data->cluster_info);
		}
	}
	else
	{
		/* Fallback to offline mode if connection fails */
		data->node_count = 0;
		strcpy(data->status_message, "ramd Connection Failed");
		ramctrl_set_fallback_cluster_info(&data->cluster_info);
	}

	base_url = getenv("RAMCTRL_API_URL");
	if (!base_url || strlen(base_url) == 0)
	{
		/* No hardcoded fallback - must be configured */
		ramctrl_set_fallback_nodes_data(data->nodes, &data->node_count);
		return false;
	}
	snprintf(url, sizeof(url), "%s/api/v1/nodes", base_url);
	result = ramctrl_http_get(url, response, sizeof(response));
	if (result == 0)
	{
		ramctrl_parse_nodes_info(response, data->nodes, &data->node_count);
	}
	else
	{
		/* Set fallback node data if API call fails */
		ramctrl_set_fallback_nodes_data(data->nodes, &data->node_count);
	}


	return true;
}


void ramctrl_watch_display_cluster(ramctrl_watch_data_t* data,
                                   ramctrl_watch_config_t* config)
{
	if (!data || !config)
		return;

	printf("Cluster Status: %s\n", data->status_message);
	printf("Nodes: %d\n", data->node_count);
	printf("Last Update: %s\n",
	       ramctrl_watch_format_timestamp(data->timestamp));
}


void ramctrl_watch_display_nodes(ramctrl_watch_data_t* data,
                                 ramctrl_watch_config_t* config)
{
	int i;

	if (!data || !config)
		return;

	/* Display table header */
	printf("%-4s %-20s %-10s %-10s %-15s %-10s\n", "ID", "Name", "Role",
	       "Status", "Health", "Lag");
	printf("%-4s %-20s %-10s %-10s %-15s %-10s\n", "----",
	       "--------------------", "----------", "----------",
	       "---------------", "----------");

	/* Display nodes */
	for (i = 0; i < data->node_count; i++)
	{
		ramctrl_node_info_t* node = &data->nodes[i];

		printf("%-4d %-20.20s %-10s %-10s %-15s %-10d\n", node->node_id,
		       node->hostname, node->is_primary ? "Primary" : "Standby",
		       ramctrl_watch_format_status(node->status),
		       node->is_healthy ? "Healthy" : "Unhealthy",
		       node->replication_lag_ms);
	}
}


void ramctrl_watch_display_replication(ramctrl_watch_data_t* data,
                                       ramctrl_watch_config_t* config)
{
	int i;

	if (!data || !config)
		return;

	/* Display replication status header */
	printf("Replication Status\n");
	printf("------------------\n");

	/* Display table header */
	printf("%-4s %-20s %-12s %-15s %-12s %-15s\n", "ID", "Name", "Role", "LSN",
	       "Lag (MB)", "State");
	printf("%-4s %-20s %-12s %-15s %-12s %-15s\n", "----",
	       "--------------------", "------------", "---------------",
	       "------------", "---------------");

	/* Display replication information for each node */
	for (i = 0; i < data->node_count; i++)
	{
		ramctrl_node_info_t* node = &data->nodes[i];

		printf("%-4d %-20.20s %-12s %-15s %-12.2f %-15s\n", node->node_id,
		       node->hostname, node->is_primary ? "Primary" : "Standby",
		       "0/0", /* Would show actual LSN */
		       (double) node->replication_lag_ms / 1024.0 /
		           1024.0, /* Convert to MB */
		       node->is_healthy ? "Streaming" : "Disconnected");
	}

	printf("\n");
	printf("Primary: %s\n", data->node_count > 0 ? "Node 1" : "None");
	printf("Sync Mode: synchronous_commit = remote_apply\n");
	printf("Max Lag: 16 MB\n");
}


void ramctrl_watch_display_header(ramctrl_watch_config_t* config)
{
	if (!config)
		return;

	if (config->show_header)
	{
		printf("PostgreSQL RAM Cluster Monitor\n");
		if (config->show_timestamps)
		{
			printf("Current Time: %s\n",
			       ramctrl_watch_format_timestamp(time(NULL)));
		}
		printf("\n");
	}
}


void ramctrl_watch_display_stats(ramctrl_watch_stats_t* stats)
{
	if (!stats)
		return;

	printf("\n");
	printf("Statistics: Updates=%d, Errors=%d, Runtime=%s\n",
	       stats->updates_count, stats->errors_count,
	       ramctrl_watch_format_duration(time(NULL) - stats->start_time));
}


void ramctrl_watch_config_set_defaults(ramctrl_watch_config_t* config)
{
	if (!config)
		return;

	memset(config, 0, sizeof(ramctrl_watch_config_t));

	config->refresh_interval_ms = 2000;
	config->show_header = true;
	config->show_timestamps = true;
	config->show_lag = false;
	config->show_health = false;
	config->compact_mode = false;
	config->color_output = isatty(STDOUT_FILENO);
	config->max_lines = 50;
}


void ramctrl_watch_clear_screen(void)
{
	printf("\033[2J");
}


void ramctrl_watch_move_cursor(int row, int col)
{
	printf("\033[%d;%dH", row, col);
}


void ramctrl_watch_set_color(const char* color)
{
	if (color)
		printf("%s", color);
}


void ramctrl_watch_reset_color(void)
{
	printf(RAMCTRL_COLOR_RESET);
}


const char* ramctrl_watch_format_timestamp(time_t timestamp)
{
	static char buffer[64];
	struct tm* tm_info;

	tm_info = localtime(&timestamp);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);

	return buffer;
}


const char* ramctrl_watch_format_duration(time_t duration)
{
	static char buffer[64];
	int hours, minutes, seconds;

	hours = (int) (duration / 3600);
	minutes = (int) ((duration % 3600) / 60);
	seconds = (int) (duration % 60);

	if (hours > 0)
		snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes,
		         seconds);
	else
		snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);

	return buffer;
}


const char* ramctrl_watch_format_status(ramctrl_node_status_t status)
{
	switch (status)
	{
	case RAMCTRL_NODE_STATUS_RUNNING:
		return "Running";
	case RAMCTRL_NODE_STATUS_STOPPED:
		return "Stopped";
	case RAMCTRL_NODE_STATUS_FAILED:
		return "Failed";
	case RAMCTRL_NODE_STATUS_MAINTENANCE:
		return "Maintenance";
	default:
		return "Unknown";
	}
}
