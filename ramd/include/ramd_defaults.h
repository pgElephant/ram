/*-------------------------------------------------------------------------
 *
 * ramd_defaults.h
 *		Central definition of all default values for RAMD
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_DEFAULTS_H
#define RAMD_DEFAULTS_H

/* PostgreSQL Default Paths */
#define RAMD_DEFAULT_PG_BIN_DIR "/usr/bin"
#define RAMD_DEFAULT_PG_DATA_DIR "/var/lib/postgresql/data"
#define RAMD_DEFAULT_PG_LOG_DIR "/var/log/postgresql"
#define RAMD_DEFAULT_PG_ARCHIVE_DIR "/var/lib/postgresql/archive"

/* PostgreSQL Default Connection Parameters */
#define RAMD_DEFAULT_PG_DATABASE "postgres"
#define RAMD_DEFAULT_PG_USER "postgres"
#define RAMD_DEFAULT_PG_PORT 5432

/* Network Security Defaults */
#define RAMD_DEFAULT_NETWORK_RANGE "127.0.0.1/32"

/* HTTP API Defaults */
#define RAMD_DEFAULT_HTTP_PORT 8008
#define RAMD_DEFAULT_HTTP_BIND_ADDRESS "127.0.0.1"

/* RALE (Raft-like Leader Election) Defaults */
#define RAMD_DEFAULT_RALE_PORT 7400
#define RAMD_DEFAULT_DSTORE_PORT 24000

/* Cluster Defaults */
#define RAMD_DEFAULT_CLUSTER_NAME "pgraft_cluster"
#define RAMD_DEFAULT_CLUSTER_SIZE 3
#define RAMD_DEFAULT_MAX_NODES 16

/* Timeouts and Intervals (in seconds) */
#define RAMD_DEFAULT_FAILOVER_TIMEOUT 60
#define RAMD_DEFAULT_HEALTH_CHECK_INTERVAL 5
#define RAMD_DEFAULT_CONNECTION_TIMEOUT 30
#define RAMD_DEFAULT_RECOVERY_TIMEOUT_MS 300000
#define RAMD_NODE_TIMEOUT_SECONDS 300
#define RAMD_HEALTH_SCORE_THRESHOLD 50.0f

/* Replication Defaults */
#define RAMD_DEFAULT_REPLICATION_LAG_THRESHOLD 5000 /* microseconds */
#define RAMD_DEFAULT_SYNC_TIMEOUT_MS 10000

/* File Paths */
#define RAMD_DEFAULT_CONFIG_FILE "/etc/ramd/ramd.conf"
#define RAMD_DEFAULT_PID_FILE "/var/run/ramd.pid"
#define RAMD_DEFAULT_LOG_FILE "/var/log/ramd.log"

/* Temporary/Development Paths */
#define RAMD_FALLBACK_DATA_DIR "/tmp/postgresql/data"
#define RAMD_FALLBACK_ARCHIVE_DIR "/tmp/postgresql/archive"

#endif /* RAMD_DEFAULTS_H */
