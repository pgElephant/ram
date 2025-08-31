/*
 * ram/ramctrl/include/ramctrl_common.h
 *   Header for ramctrl common functionality
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 * Common CLI utilities and helper functions for ramctrl.
 */

#ifndef RAMCTRL_COMMON_H
#define RAMCTRL_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <getopt.h>

/* Function declarations */
void keeper_cli_help(int argc, char **argv);
int cli_print_version_getopts(int argc, char **argv);
void keeper_cli_print_version(int argc, char **argv);
void cli_pprint_json(const char *json_str);
bool cli_common_getenv(const char *var_name, char *buffer, size_t buffer_size);
bool cli_common_getenv_pgsetup(char *pgdata, char *pgport, char *pguser, size_t buffer_size);
void cli_common_get_set_pgdata_or_exit(char *pgdata, size_t buffer_size);
int cli_common_keeper_getopts(int argc, char **argv, struct option *long_options,
                             const char *optstring, char *pgdata, char *pgport, 
                             char *pguser, size_t buffer_size);
int cli_create_node_getopts(int argc, char **argv, struct option *long_options,
                           const char *optstring, char *node_name, char *cluster_name,
                           size_t buffer_size);

#endif /* RAMCTRL_COMMON_H */
