/*-------------------------------------------------------------------------
 *
 * ramd_callbacks.h
 *		Custom callback system for automation hooks
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_CALLBACKS_H
#define RAMD_CALLBACKS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Callback event types */
typedef enum ramd_callback_event
{
	RAMD_CALLBACK_PRE_FAILOVER,         /* Before failover starts */
	RAMD_CALLBACK_POST_FAILOVER,        /* After failover completes */
	RAMD_CALLBACK_PRE_PROMOTE,          /* Before node promotion */
	RAMD_CALLBACK_POST_PROMOTE,         /* After node promotion */
	RAMD_CALLBACK_PRE_DEMOTE,           /* Before node demotion */
	RAMD_CALLBACK_POST_DEMOTE,          /* After node demotion */
	RAMD_CALLBACK_NODE_UNHEALTHY,       /* When node becomes unhealthy */
	RAMD_CALLBACK_NODE_HEALTHY,         /* When node becomes healthy */
	RAMD_CALLBACK_CLUSTER_DEGRADED,     /* When cluster becomes degraded */
	RAMD_CALLBACK_CLUSTER_HEALTHY,      /* When cluster becomes healthy */
	RAMD_CALLBACK_PRIMARY_LOST,         /* When primary is lost */
	RAMD_CALLBACK_PRIMARY_ELECTED,      /* When new primary is elected */
	RAMD_CALLBACK_REPLICA_CONNECTED,    /* When replica connects */
	RAMD_CALLBACK_REPLICA_DISCONNECTED, /* When replica disconnects */
	RAMD_CALLBACK_MAINTENANCE_START,    /* When maintenance mode starts */
	RAMD_CALLBACK_MAINTENANCE_END,      /* When maintenance mode ends */
	RAMD_CALLBACK_CONFIG_RELOAD,        /* When configuration is reloaded */
	RAMD_CALLBACK_CUSTOM                /* Custom user-defined event */
} ramd_callback_event_t;

/* Callback execution context */
typedef struct ramd_callback_context
{
	ramd_callback_event_t event;
	int32_t node_id;
	char node_name[64];
	char event_data[1024]; /* JSON-formatted event data */
	time_t timestamp;
	int32_t cluster_id;
	char cluster_name[64];
} ramd_callback_context_t;

/* Callback configuration */
typedef struct ramd_callback_config
{
	char script_path[512];
	ramd_callback_event_t event;
	bool enabled;
	bool async;              /* Execute asynchronously */
	int32_t timeout_seconds; /* Timeout for synchronous execution */
	bool log_output;         /* Log script output */
	bool abort_on_failure;   /* Abort operation if callback fails */
} ramd_callback_config_t;

/* Maximum number of callbacks per event */
#define RAMD_MAX_CALLBACKS_PER_EVENT 10
#define RAMD_MAX_TOTAL_CALLBACKS 100

/* Function prototypes */
extern bool ramd_callbacks_init(void);
extern void ramd_callbacks_cleanup(void);
extern bool ramd_callback_register(ramd_callback_event_t event,
                                   const char* script_path, bool async,
                                   int32_t timeout_seconds,
                                   bool abort_on_failure);
extern bool ramd_callback_unregister(ramd_callback_event_t event,
                                     const char* script_path);
extern bool ramd_callback_execute(ramd_callback_event_t event,
                                  ramd_callback_context_t* context);
extern bool ramd_callback_execute_all(ramd_callback_event_t event,
                                      ramd_callback_context_t* context);
extern const char* ramd_callback_event_to_string(ramd_callback_event_t event);
extern ramd_callback_event_t
ramd_callback_string_to_event(const char* event_str);

/* Configuration helpers */
extern bool ramd_callback_config_load_from_file(const char* config_file);
extern bool ramd_callback_config_save_to_file(const char* config_file);
extern void ramd_callback_config_set_defaults(ramd_callback_config_t* config);

/* Context helpers */
extern void ramd_callback_context_init(ramd_callback_context_t* context,
                                       ramd_callback_event_t event);
extern void ramd_callback_context_set_node(ramd_callback_context_t* context,
                                           int32_t node_id,
                                           const char* node_name);
extern void ramd_callback_context_set_data(ramd_callback_context_t* context,
                                           const char* json_data);

/* Predefined callback helpers */
extern bool ramd_callback_trigger_failover(int32_t old_primary_id,
                                           int32_t new_primary_id);
extern bool ramd_callback_trigger_promotion(int32_t node_id);
extern bool ramd_callback_trigger_health_change(int32_t node_id, bool healthy);
extern bool ramd_callback_trigger_cluster_state_change(bool healthy);

#endif /* RAMD_CALLBACKS_H */
