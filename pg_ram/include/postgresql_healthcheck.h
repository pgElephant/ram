/*-------------------------------------------------------------------------
 *
 * postgresql_healthcheck.h
 *		PostgreSQL health check system
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTGRESQL_HEALTHCHECK_H
#define POSTGRESQL_HEALTHCHECK_H

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/timestamp.h"

/* Health check configuration */
typedef struct postgresql_healthcheck_config
{
	bool		enabled;
	int32		check_frequency_seconds;
	int32		failure_threshold;
	int32		recovery_threshold;
	bool		auto_recovery_enabled;
	bool		alert_on_failure;
} postgresql_healthcheck_config_t;

/* Health check result */
typedef struct postgresql_healthcheck_result
{
	bool		passed;
	char		check_name[64];
	char		description[256];
	float		response_time_ms;
	int32		severity_level; /* 1=INFO, 2=WARNING, 3=ERROR, 4=CRITICAL */
	timestamptz check_time;
	char		error_message[512];
} postgresql_healthcheck_result_t;

/* Health check status */
typedef struct postgresql_healthcheck_status
{
	bool		overall_healthy;
	int32		total_checks;
	int32		passed_checks;
	int32		failed_checks;
	int32		consecutive_failures;
	float		overall_score;
	timestamptz last_check_time;
	postgresql_healthcheck_result_t checks[16];	/* Support up to 16 different checks */
} postgresql_healthcheck_status_t;

extern postgresql_healthcheck_config_t *postgresql_healthcheck_config;

/* Function prototypes */

/* Health check initialization */
extern bool postgresql_healthcheck_init(void);
extern void postgresql_healthcheck_cleanup(void);

/* Core health check functions */
extern bool postgresql_healthcheck_run_all(postgresql_healthcheck_status_t *status_out);
extern bool postgresql_healthcheck_run_single(const char *check_name, postgresql_healthcheck_result_t *result_out);

/* Specific health checks */
extern bool postgresql_healthcheck_database_connectivity(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_replication_lag(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_disk_space(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_memory_usage(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_connection_limits(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_wal_archiving(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_vacuum_status(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_lock_contention(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_checkpoint_performance(postgresql_healthcheck_result_t *result_out);
extern bool postgresql_healthcheck_backup_status(postgresql_healthcheck_result_t *result_out);

/* Health check utilities */
extern bool postgresql_healthcheck_is_critical_failure(const postgresql_healthcheck_status_t *status);
extern float postgresql_healthcheck_calculate_score(const postgresql_healthcheck_status_t *status);
extern char *postgresql_healthcheck_get_summary(const postgresql_healthcheck_status_t *status);

#endif							/* POSTGRESQL_HEALTHCHECK_H */
