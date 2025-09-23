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

/* Basic Size Constants */
#define RAMD_MAX_HOSTNAME_LENGTH           256
#define RAMD_MAX_PATH_LENGTH               512
#define RAMD_MAX_COMMAND_LENGTH            1024
#define RAMD_MAX_LOG_MESSAGE               2048
#define RAMD_MAX_NODES                     16

/* Daemon Constants */
#define RAMD_MONITOR_INTERVAL_MS           5000
#define RAMD_FAILOVER_TIMEOUT_MS           30000
#define RAMD_HEALTH_CHECK_TIMEOUT_MS       10000
#define RAMD_DEFAULT_PORT                  8008

/* PostgreSQL Default Paths */
#define RAMD_DEFAULT_PG_BIN_DIR          "/usr/bin"
#define RAMD_DEFAULT_PG_DATA_DIR         "/var/lib/postgresql/data"
#define RAMD_DEFAULT_PG_LOG_DIR          "/var/log/postgresql"
#define RAMD_DEFAULT_PG_ARCHIVE_DIR      "/var/lib/postgresql/archive"

/* PostgreSQL Default Connection Parameters */
#define RAMD_DEFAULT_PG_DATABASE         "postgres"
#define RAMD_DEFAULT_PG_USER             "postgres"
#define RAMD_DEFAULT_PG_PORT             5432

/* Authentication Defaults */
#define RAMD_DEFAULT_AUTH_METHOD         "password"
#define RAMD_DEFAULT_SSL_MODE            "prefer"
#define RAMD_DEFAULT_KERBEROS_SERVICE    "postgres"
#define RAMD_DEFAULT_LDAP_PORT           389
#define RAMD_DEFAULT_PAM_SERVICE         "postgresql"
#define RAMD_DEFAULT_AUTH_CONNECTION_TIMEOUT  10

/* Network Security Defaults */
#define RAMD_DEFAULT_NETWORK_RANGE       "127.0.0.1/32"

/* HTTP API Defaults */
#define RAMD_DEFAULT_HTTP_PORT           8008
#define RAMD_DEFAULT_HTTP_BIND_ADDRESS   "127.0.0.1"

/* RALE (Raft-like Leader Election) Defaults */
#define RAMD_DEFAULT_RALE_PORT           7400
#define RAMD_DEFAULT_DSTORE_PORT         24000

/* Cluster Defaults */
#define RAMD_DEFAULT_CLUSTER_NAME        "pgraft_cluster"
#define RAMD_DEFAULT_CLUSTER_SIZE        3
#define RAMD_DEFAULT_MAX_NODES           16

/* Timeouts and Intervals (in seconds) */
#define RAMD_DEFAULT_FAILOVER_TIMEOUT    60
#define RAMD_DEFAULT_HEALTH_CHECK_INTERVAL 5
#define RAMD_DEFAULT_CONNECTION_TIMEOUT   30
#define RAMD_DEFAULT_RECOVERY_TIMEOUT_MS  300000
#define RAMD_NODE_TIMEOUT_SECONDS        300
#define RAMD_HEALTH_SCORE_THRESHOLD      50.0f
#define RAMD_MAX_HEALTH_SCORE            1.0f
#define RAMD_MIN_HEALTH_SCORE            0.0f
#define RAMD_FAILOVER_THRESHOLD          3
#define RAMD_FAILOVER_SLEEP_SECONDS      3
#define RAMD_FAILOVER_VALIDATION_SLEEP_SECONDS 5

/* Health Score Constants */
#define RAMD_HEALTH_BASE_SCORE           50.0f
#define RAMD_HEALTH_PRIMARY_BONUS        30.0f
#define RAMD_HEALTH_STANDBY_SCORE        20.0f
#define RAMD_HEALTH_WAL_SCORE            15.0f
#define RAMD_HEALTH_VACUUM_BONUS         5.0f
#define RAMD_MAX_VACUUM_PROCESSES        5
#define RAMD_POSTGRESQL_RESTART_DELAY    2

/* Logging Constants */
#define RAMD_MAX_TIMESTAMP_LENGTH        64
#define RAMD_MAX_USERNAME_LENGTH         256

/* API Path Lengths */
#define RAMD_API_PROMOTE_PATH_LEN        16
#define RAMD_API_DEMOTE_PATH_LEN         15
#define RAMD_API_NODES_PATH_LEN          14

/* Maintenance Constants */
#define RAMD_MAINTENANCE_SCHEDULE_DELAY_HOURS 1

/* Metrics Constants */
#define RAMD_METRICS_COLLECTION_INTERVAL_MS 5000
#define RAMD_KILOBYTE_TO_BYTES              1024
#define RAMD_PRIMARY_NODE_COUNT             1
#define RAMD_DEFAULT_MAINTENANCE_TIMEOUT_MS 300000
#define RAMD_MAX_LINE_LENGTH                1024
#define RAMD_HTTP_MAX_CONNECTIONS           128

/* Replication Defaults */
#define RAMD_DEFAULT_REPLICATION_LAG_THRESHOLD 5000 /* microseconds */
#define RAMD_DEFAULT_SYNC_TIMEOUT_MS     10000

/* File Paths */
#define RAMD_DEFAULT_CONFIG_FILE         "/etc/ramd/ramd.conf"
#define RAMD_DEFAULT_PID_FILE            "/var/run/ramd.pid"
#define RAMD_DEFAULT_LOG_FILE            "/var/log/ramd.log"

/* Temporary/Development Paths */
#define RAMD_FALLBACK_DATA_DIR           "/tmp/postgresql/data"
#define RAMD_FALLBACK_ARCHIVE_DIR        "/tmp/postgresql/archive"

#endif /* RAMD_DEFAULTS_H */
