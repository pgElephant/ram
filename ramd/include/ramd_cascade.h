/*-------------------------------------------------------------------------
 *
 * ramd_cascade.h
 *		Cascading replication support for ramd
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_CASCADE_H
#define RAMD_CASCADE_H

#include <stdbool.h>
#include <stdint.h>
#include "ramd_config.h"

/* Cascading replication configuration */
typedef struct ramd_cascade_config
{
	bool enabled;              /* Enable cascading replication */
	int32_t upstream_node_id;  /* ID of upstream node (-1 for primary) */
	bool allow_cascading;      /* Allow this node to be upstream for others */
	int32_t max_cascade_depth; /* Maximum cascade depth allowed */
	int32_t cascade_lag_threshold;     /* Maximum lag for cascade eligibility */
	char cascade_application_name[64]; /* Application name for cascade
	                                      connections */
} ramd_cascade_config_t;

/* Cascading node information */
typedef struct ramd_cascade_node
{
	int32_t node_id;
	int32_t upstream_node_id;
	int32_t cascade_depth;
	bool is_cascade_eligible;
	int64_t cascade_lag_bytes;
	char application_name[64];
} ramd_cascade_node_t;

/* Function prototypes */
extern bool ramd_cascade_init(ramd_cascade_config_t* config);
extern void ramd_cascade_cleanup(void);
extern bool ramd_cascade_is_enabled(void);
extern bool ramd_cascade_setup_upstream(int32_t upstream_node_id);
extern bool ramd_cascade_remove_upstream(void);
extern bool ramd_cascade_add_downstream(int32_t downstream_node_id);
extern bool ramd_cascade_remove_downstream(int32_t downstream_node_id);
extern bool ramd_cascade_validate_topology(void);
extern int32_t ramd_cascade_find_best_upstream(int32_t for_node_id);
extern bool ramd_cascade_reconfigure_on_failover(int32_t failed_node_id,
                                                 int32_t new_primary_id);
extern bool ramd_cascade_get_node_info(int32_t node_id,
                                       ramd_cascade_node_t* info);
extern int32_t ramd_cascade_get_depth(int32_t node_id);
extern bool ramd_cascade_is_loop_free(int32_t node_id,
                                      int32_t proposed_upstream_id);

/* Configuration helpers */
extern bool ramd_cascade_config_parse(const char* config_str,
                                      ramd_cascade_config_t* config);
extern bool ramd_cascade_config_validate(ramd_cascade_config_t* config);
extern void ramd_cascade_config_set_defaults(ramd_cascade_config_t* config);

#endif /* RAMD_CASCADE_H */
