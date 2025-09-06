/*-------------------------------------------------------------------------
 *
 * ramctrl_show.h
 *		PostgreSQL RAM Control Utility - Show Commands
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_SHOW_H
#define RAMCTRL_SHOW_H

#include "ramctrl.h"

/* Enhanced show command functions */
int ramctrl_show_cluster_detailed(ramctrl_context_t* ctx);
int ramctrl_show_nodes_detailed(ramctrl_context_t* ctx);
int ramctrl_show_replication_detailed(ramctrl_context_t* ctx);

/* Additional show commands */
int ramctrl_show_status_all(ramctrl_context_t* ctx);
int ramctrl_show_health(ramctrl_context_t* ctx);
int ramctrl_show_logs_summary(ramctrl_context_t* ctx);
int ramctrl_show_configuration(ramctrl_context_t* ctx);
int ramctrl_show_performance(ramctrl_context_t* ctx);

/* Show output formatting functions */
void ramctrl_show_format_cluster_table(ramctrl_cluster_info_t* cluster);
void ramctrl_show_format_nodes_table(ramctrl_node_info_t* nodes, int count);
void ramctrl_show_format_replication_table(ramctrl_context_t* ctx);

/* JSON output functions */
void ramctrl_show_cluster_json(ramctrl_cluster_info_t* cluster);
void ramctrl_show_nodes_json(ramctrl_node_info_t* nodes, int count);
void ramctrl_show_replication_json(ramctrl_context_t* ctx);

#endif /* RAMCTRL_SHOW_H */
