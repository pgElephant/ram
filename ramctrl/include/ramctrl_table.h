/*-------------------------------------------------------------------------
 *
 * ramctrl_table.h
 *		Table formatting utilities for ramctrl output.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_TABLE_H
#define RAMCTRL_TABLE_H

#include "ramctrl.h"
#include "ramctrl_daemon.h"

/* Table formatting functions */
void ramctrl_table_print_header(const char* title);
void ramctrl_table_print_separator(void);
void ramctrl_table_print_row(const char* key, const char* value);
void ramctrl_table_print_row_int(const char* key, int value);
void ramctrl_table_print_row_bool(const char* key, bool value);
void ramctrl_table_print_row_time(const char* key, time_t timestamp);
void ramctrl_table_print_footer(void);

/* Status table functions */
void ramctrl_table_print_daemon_status(const ramctrl_daemon_status_t* status);
void ramctrl_table_print_cluster_status(const ramctrl_cluster_info_t* cluster);
void ramctrl_table_print_node_status(const ramctrl_node_info_t* node);

#endif /* RAMCTRL_TABLE_H */
