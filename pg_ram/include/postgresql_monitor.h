/*-------------------------------------------------------------------------
 *
 * postgresql_monitor.h
 *		PostgreSQL monitoring and health check functions
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTGRESQL_MONITOR_H
#define POSTGRESQL_MONITOR_H

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/lwlock.h"
#include "utils/timestamp.h"
#include "datatype/timestamp.h"
#include "utils/pg_lsn.h"

/* PostgreSQL monitoring configuration */
typedef struct
{
	bool enabled;
	int32_t check_interval_seconds;
	int32_t timeout_ms;
	float health_threshold;
	bool detailed_monitoring;
} postgresql_monitor_config_t;

/* PostgreSQL health metrics */
typedef struct
{
	bool is_running;
	bool is_accepting_connections;
	bool is_in_recovery;
	bool is_primary;
	bool is_streaming_replication_active;
	int64_t database_size_bytes;
	int32_t active_connections;
	int32_t max_connections;
	float connection_usage_percentage;
	XLogRecPtr current_wal_lsn;
	XLogRecPtr received_lsn;
	XLogRecPtr replayed_lsn;
	int32_t wal_lag_seconds;
	int64_t shared_buffers_used;
	int64_t shared_buffers_total;
	float buffer_hit_ratio;
	int32_t checkpoint_segments_ready;
	int32_t background_writer_activity;
	TimestampTz last_checkpoint_time;
	TimestampTz last_health_check;
	float overall_health_score;
	char status_message[512];
} postgresql_health_t;

extern postgresql_monitor_config_t* postgresql_monitor_config;

/* PostgreSQL monitoring functions */
extern bool postgresql_monitor_init(void);
extern void postgresql_monitor_cleanup(void);
extern bool postgresql_monitor_health_check(postgresql_health_t* health_out);
extern bool postgresql_monitor_is_healthy(void);
extern float postgresql_monitor_get_health_score(void);
extern bool postgresql_monitor_can_accept_writes(void);
extern bool postgresql_monitor_is_replication_healthy(void);
extern bool postgresql_monitor_check_wal_health(void);
extern bool postgresql_monitor_check_connections(void);
extern bool postgresql_monitor_check_disk_space(void);
extern bool postgresql_monitor_check_performance_metrics(void);

/* PostgreSQL status functions */
extern char* postgresql_monitor_get_status_summary(void);
extern bool postgresql_monitor_get_replication_status(char* status_out,
                                                      size_t status_size);
extern bool postgresql_monitor_get_connection_info(char* info_out,
                                                   size_t info_size);

#endif /* POSTGRESQL_MONITOR_H */
