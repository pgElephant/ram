/*-------------------------------------------------------------------------
 *
 * pgram_guc.h
 *		GUCs for configuring pg_ram librale integration
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PGRAM_GUC_H
#define PGRAM_GUC_H

#include "postgres.h"

/* Function prototypes */
extern void pgram_guc_init(void);

/* Exposed GUC variables */
extern int pgram_node_id;
extern char* pgram_node_name;
extern char* pgram_node_ip;
extern int pgram_rale_port;
extern int pgram_dstore_port;
extern char* pgram_db_path;

#endif /* PGRAM_GUC_H */
