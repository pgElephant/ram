/*-------------------------------------------------------------------------
 *
 * pgram_librale.h
 *		librale integration for pg_ram distributed consensus
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGRAM_LIBRALE_H
#define PGRAM_LIBRALE_H

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"

/* Forward declaration - will include librale.h in .c file */
typedef struct librale_config_t librale_config_t;
typedef struct librale_node_t librale_node_t;

typedef enum librale_status
{
	RALE_SUCCESS = 0,
	RALE_ERROR_GENERAL = -1
} librale_status_t;

typedef struct pg_ram_librale_config
{
	librale_config_t* librale_config; /* librale configuration */
	bool initialized;                 /* Whether librale is initialized */
	int32 node_id;                    /* Node ID for this instance */
	char node_name[256];              /* Node name */
	char node_ip[256];                /* Node IP address */
	uint16 rale_port;                 /* RALE UDP port */
	uint16 dstore_port;               /* DStore TCP port */
	char db_path[512];                /* Database path */
	char log_directory[512];          /* Log directory path */
} pg_ram_librale_config_t;

extern pg_ram_librale_config_t* pg_ram_librale_config;

/* Function prototypes */
extern bool pgram_librale_init(void);
extern void pgram_librale_cleanup(void);
extern bool pgram_librale_is_leader(void);
extern int32 pgram_librale_get_leader_id(void);
extern uint32 pgram_librale_get_node_count(void);
extern librale_status_t pgram_librale_add_node(int32 node_id, const char* name,
                                               const char* ip, uint16 rale_port,
                                               uint16 dstore_port);
extern librale_status_t pgram_librale_remove_node(int32 node_id);
extern bool pgram_librale_has_quorum(void);
extern librale_status_t pgram_librale_process_consensus(void);
extern int32 pgram_librale_get_current_role(void);
extern int32 pgram_librale_get_current_term(void);
extern bool pgram_librale_is_node_healthy(int32 node_id);
extern librale_status_t pgram_librale_get_cluster_status(char* status,
                                                         size_t status_size);

#endif /* PGRAM_LIBRALE_H */
