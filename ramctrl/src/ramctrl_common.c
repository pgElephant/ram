/*-------------------------------------------------------------------------
 *
 * ramctrl_common.c
 *		Implementation of ramctrl common functionality
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ramctrl_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static const char* program_name = "ramctrl";

void keeper_cli_help(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	printf("Usage: %s <command> [options]\n\n", program_name);
	printf("Commands:\n");
	printf("  show            Show incluster (status, nodes, uri)\n");
	printf("  watch           Watch resources\n");
	printf("\nUse '%s <command> --help' for more incluster about a command.\n",
	       program_name);
}


int cli_print_version_getopts(int argc, char** argv)
{
	int opt;
	int verbose = 0;

	while ((opt = getopt(argc, argv, "v")) != -1)
	{
		switch (opt)
		{
		case 'v':
			verbose = 1;
			break;
		default:
			return -1;
		}
	}

	return verbose;
}


void keeper_cli_print_version(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	printf("ramctrl version 1.0.0\n");
	printf("Cluster management tool\n");
}


void cli_pprint_json(const char* json_str)
{
	if (json_str == NULL)
	{
		printf("null\n");
		return;
	}

	printf("%s\n", json_str);
}


bool cli_common_getenv(const char* var_name, char* buffer, size_t buffer_size)
{
	const char* value;

	if (var_name == NULL || buffer == NULL || buffer_size == 0)
		return false;

	value = getenv(var_name);
	if (value == NULL)
		return false;

	if (strlen(value) >= buffer_size)
		return false;

	strncpy(buffer, value, buffer_size - 1);
	buffer[buffer_size - 1] = '\0';
	return true;
}


bool cli_common_getenv_pgsetup(char* pgdata, char* pgport, char* pguser,
                               size_t buffer_size)
{
	bool success = true;

	if (pgdata != NULL)
	{
		if (!cli_common_getenv("PGDATA", pgdata, buffer_size))
			success = false;
	}

	if (pgport != NULL)
	{
		if (!cli_common_getenv("PGPORT", pgport, buffer_size))
			success = false;
	}

	if (pguser != NULL)
	{
		if (!cli_common_getenv("PGUSER", pguser, buffer_size))
			success = false;
	}

	return success;
}


void cli_common_get_set_pgdata_or_exit(char* pgdata, size_t buffer_size)
{
	struct stat st;

	if (pgdata == NULL || buffer_size == 0)
	{
		fprintf(stderr, "Error: Invalid pgdata buffer\n");
		exit(1);
	}

	if (!cli_common_getenv("PGDATA", pgdata, buffer_size))
	{
		fprintf(stderr, "Error: PGDATA environment variable not set\n");
		exit(1);
	}

	if (stat(pgdata, &st) != 0)
	{
		fprintf(stderr, "Error: PGDATA directory '%s' does not exist: %s\n",
		        pgdata, strerror(errno));
		exit(1);
	}

	if (!S_ISDIR(st.st_mode))
	{
		fprintf(stderr, "Error: PGDATA '%s' is not a directory\n", pgdata);
		exit(1);
	}
}


int cli_common_keeper_getopts(int argc, char** argv,
                              struct option* long_options,
                              const char* optstring, char* pgdata, char* pgport,
                              char* pguser, size_t buffer_size)
{
	int opt;
	int verbose = 0;
	int quiet = 0;
	(void) quiet; /* Suppress unused variable warning */

	while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1)
	{
		switch (opt)
		{
		case 'v':
			verbose = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'D':
			if (optarg != NULL && pgdata != NULL)
			{
				if (strlen(optarg) >= buffer_size)
				{
					fprintf(stderr, "Error: PGDATA path too long\n");
					return -1;
				}
				strncpy(pgdata, optarg, buffer_size - 1);
				pgdata[buffer_size - 1] = '\0';
			}
			break;
		case 'p':
			if (optarg != NULL && pgport != NULL)
			{
				if (strlen(optarg) >= buffer_size)
				{
					fprintf(stderr, "Error: PGPORT too long\n");
					return -1;
				}
				strncpy(pgport, optarg, buffer_size - 1);
				pgport[buffer_size - 1] = '\0';
			}
			break;
		case 'U':
			if (optarg != NULL && pguser != NULL)
			{
				if (strlen(optarg) >= buffer_size)
				{
					fprintf(stderr, "Error: PGUSER too long\n");
					return -1;
				}
				strncpy(pguser, optarg, buffer_size - 1);
				pguser[buffer_size - 1] = '\0';
			}
			break;
		default:
			return -1;
		}
	}

	return verbose;
}


int cli_create_node_getopts(int argc, char** argv, struct option* long_options,
                            const char* optstring, char* node_name,
                            char* cluster_name, size_t buffer_size)
{
	int opt;
	int verbose = 0;

	while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1)
	{
		switch (opt)
		{
		case 'v':
			verbose = 1;
			break;
		case 'n':
			if (optarg != NULL && node_name != NULL)
			{
				if (strlen(optarg) >= buffer_size)
				{
					fprintf(stderr, "Error: Node name too long\n");
					return -1;
				}
				strncpy(node_name, optarg, buffer_size - 1);
				node_name[buffer_size - 1] = '\0';
			}
			break;
		case 'f':
			if (optarg != NULL && cluster_name != NULL)
			{
				if (strlen(optarg) >= buffer_size)
				{
					fprintf(stderr, "Error: Cluster name too long\n");
					return -1;
				}
				strncpy(cluster_name, optarg, buffer_size - 1);
				cluster_name[buffer_size - 1] = '\0';
			}
			break;
		default:
			return -1;
		}
	}

	return verbose;
}
