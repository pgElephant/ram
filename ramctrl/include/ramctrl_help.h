/*-------------------------------------------------------------------------
 *
 * ramctrl_help.h
 *		PostgreSQL RAM Control Utility - Help System
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_HELP_H
#define RAMCTRL_HELP_H

#include "ramctrl.h"

/* Help system functions */
void ramctrl_help_show_general(void);
void ramctrl_help_show_command(const char* command);
void ramctrl_help_show_commands(void);
void ramctrl_help_show_examples(void);

/* Command-specific help functions */
void ramctrl_help_show_status(void);
void ramctrl_help_show_start_stop(void);
void ramctrl_help_show_cluster_management(void);
void ramctrl_help_show_replication(void);
void ramctrl_help_show_backup(void);
void ramctrl_help_show_watch(void);

/* Utility help functions */
void ramctrl_help_show_configuration(void);
void ramctrl_help_show_troubleshooting(void);

#endif /* RAMCTRL_HELP_H */
