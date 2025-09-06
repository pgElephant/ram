/*-------------------------------------------------------------------------
 *
 * pgram_health_types.h
 *   Shared types for pg_ram health monitoring
 *
 *------------------------------------------------------------------------- */
#ifndef PGRAM_HEALTH_TYPES_H
#define PGRAM_HEALTH_TYPES_H

#include "postgres.h"

typedef enum
{
	PGRAM_HEALTH_OK = 0,  /* Health status is good */
	PGRAM_HEALTH_WARNING, /* Health status has warnings */
	PGRAM_HEALTH_ERROR,   /* Health status has errors */
	PGRAM_HEALTH_CRITICAL /* Health status is critical */
} PgramHealthStatus;

typedef struct
{
	bool is_running;                      /* Whether the worker is running */
	int64 health_checks_performed;        /* Total health checks performed */
	int64 warnings_count;                 /* Total warnings encountered */
	int64 errors_count;                   /* Total errors encountered */
	TimestampTz last_activity;            /* Timestamp of last activity */
	PgramHealthStatus last_health_status; /* Last health status */
} health_worker_status_t;

#endif
