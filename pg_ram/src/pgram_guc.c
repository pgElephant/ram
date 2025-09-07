/*-------------------------------------------------------------------------
 *
 * pgram_guc.c
 *		GUCs for configuring pg_ram librale integration
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/guc.h"

#include "pgram_guc.h"

#define DEFAULT_NODE_ID 1
#define DEFAULT_NODE_NAME "pg_ram_node_1"
#define DEFAULT_NODE_IP "" /* Must be configured */
#define DEFAULT_RALE_PORT 7400
#define DEFAULT_DSTORE_PORT 7401

int pgram_node_id = DEFAULT_NODE_ID;         /* Node ID for librale */
char* pgram_node_name = NULL;                /* Node name for librale */
char* pgram_node_ip = NULL;                  /* Node IP for librale */
int pgram_rale_port = DEFAULT_RALE_PORT;     /* RALE UDP port */
int pgram_dstore_port = DEFAULT_DSTORE_PORT; /* DStore TCP port */
char* pgram_db_path = NULL;                  /* Path for librale state/data */
bool pgram_is_primary = false;               /* Is this node the primary leader? */

void pgram_guc_init(void)
{
	DefineCustomIntVariable("pg_ram.node_id", "Node ID for librale", NULL,
	                        &pgram_node_id, DEFAULT_NODE_ID, 0, 2147483647,
	                        PGC_POSTMASTER, 0, NULL, NULL, NULL);

	DefineCustomStringVariable("pg_ram.node_name", "Node name for librale",
	                           NULL, &pgram_node_name, DEFAULT_NODE_NAME,
	                           PGC_POSTMASTER, 0, NULL, NULL, NULL);

	DefineCustomStringVariable("pg_ram.node_ip", "Node IP for librale", NULL,
	                           &pgram_node_ip, DEFAULT_NODE_IP, PGC_POSTMASTER,
	                           0, NULL, NULL, NULL);

	DefineCustomIntVariable("pg_ram.rale_port", "RALE UDP port", NULL,
	                        &pgram_rale_port, DEFAULT_RALE_PORT, 1, 65535,
	                        PGC_POSTMASTER, 0, NULL, NULL, NULL);

	DefineCustomIntVariable("pg_ram.dstore_port", "DStore TCP port", NULL,
	                        &pgram_dstore_port, DEFAULT_DSTORE_PORT, 1, 65535,
	                        PGC_POSTMASTER, 0, NULL, NULL, NULL);

	DefineCustomStringVariable(
	    "pg_ram.db_path", "Path for librale state/data under PGDATA by default",
	    NULL, &pgram_db_path, NULL, PGC_POSTMASTER, 0, NULL, NULL, NULL);

	DefineCustomBoolVariable("pg_ram.is_primary",
	                         "Is this node the primary leader?", NULL,
	                         &pgram_is_primary, false, PGC_POSTMASTER, 0,
	                         NULL, NULL, NULL);
}
