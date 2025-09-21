/*-------------------------------------------------------------------------
 *
 * ramctrl_watch.h
 *		Watch mode for real-time cluster monitoring
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_WATCH_H
#define RAMCTRL_WATCH_H

#include "ramctrl.h"
#include <time.h>

/* Watch mode configuration */
typedef struct ramctrl_watch_config
{
	int32_t refresh_interval_ms; /* Refresh interval in milliseconds */
	bool show_header;            /* Show column headers */
	bool show_timestamps;        /* Show timestamps */
	bool show_lag;               /* Show replication lag */
	bool show_health;            /* Show health status */
	bool compact_mode;           /* Compact display mode */
	bool color_output;           /* Use color output */
	char filter_node[RAMCTRL_MAX_HOSTNAME_LENGTH];        /* Filter by node name */
	int32_t max_lines;           /* Maximum lines to display */
} ramctrl_watch_config_t;

/* Watch mode display data */
typedef struct ramctrl_watch_data
{
	time_t timestamp;
	ramctrl_cluster_info_t cluster_info;
	ramctrl_node_info_t nodes[RAMCTRL_MAX_NODES];
	int32_t node_count;
	char status_message[RAMCTRL_MAX_HOSTNAME_LENGTH];
} ramctrl_watch_data_t;

/* Watch mode statistics */
typedef struct ramctrl_watch_stats
{
	int32_t updates_count;
	int32_t errors_count;
	time_t start_time;
	time_t last_update;
	int32_t failover_count;
	int32_t promotion_count;
} ramctrl_watch_stats_t;

/* Function prototypes */
extern int ramctrl_cmd_watch(ramctrl_context_t* ctx);
extern int ramctrl_cmd_watch_cluster(ramctrl_context_t* ctx);
extern int ramctrl_cmd_watch_nodes(ramctrl_context_t* ctx);
extern int ramctrl_cmd_watch_replication(ramctrl_context_t* ctx);

/* Watch mode internal functions */
extern bool ramctrl_watch_init(ramctrl_watch_config_t* config);
extern void ramctrl_watch_cleanup(void);
extern bool ramctrl_watch_update_data(ramctrl_watch_data_t* data);
extern void ramctrl_watch_display_cluster(ramctrl_watch_data_t* data,
                                          ramctrl_watch_config_t* config);
extern void ramctrl_watch_display_nodes(ramctrl_watch_data_t* data,
                                        ramctrl_watch_config_t* config);
extern void ramctrl_watch_display_replication(ramctrl_watch_data_t* data,
                                              ramctrl_watch_config_t* config);
extern void ramctrl_watch_display_header(ramctrl_watch_config_t* config);
extern void ramctrl_watch_display_stats(ramctrl_watch_stats_t* stats);

/* Configuration helpers */
extern void ramctrl_watch_config_set_defaults(ramctrl_watch_config_t* config);
extern bool ramctrl_watch_config_parse_args(ramctrl_watch_config_t* config,
                                            int argc, char* argv[]);

/* Display utilities */
extern void ramctrl_watch_clear_screen(void);
extern void ramctrl_watch_move_cursor(int row, int col);
extern void ramctrl_watch_set_color(const char* color);
extern void ramctrl_watch_reset_color(void);
extern const char* ramctrl_watch_format_timestamp(time_t timestamp);
extern const char* ramctrl_watch_format_duration(time_t duration);
extern const char* ramctrl_watch_format_status(ramctrl_node_status_t status);

/* Color definitions */
#define RAMCTRL_COLOR_RESET "\033[0m"
#define RAMCTRL_COLOR_RED "\033[31m"
#define RAMCTRL_COLOR_GREEN "\033[32m"
#define RAMCTRL_COLOR_YELLOW "\033[33m"
#define RAMCTRL_COLOR_BLUE "\033[34m"
#define RAMCTRL_COLOR_MAGENTA "\033[35m"
#define RAMCTRL_COLOR_CYAN "\033[36m"
#define RAMCTRL_COLOR_WHITE "\033[37m"
#define RAMCTRL_COLOR_BOLD "\033[1m"

#endif /* RAMCTRL_WATCH_H */
