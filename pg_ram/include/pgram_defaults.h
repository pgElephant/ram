/*-------------------------------------------------------------------------
 *
 * pgram_defaults.h
 *		Central definition of all default values for PG_RAM extension
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGRAM_DEFAULTS_H
#define PGRAM_DEFAULTS_H

/* RALE (Raft-like Leader Election) Defaults */
#define PGRAM_DEFAULT_RALE_PORT 7400
#define PGRAM_DEFAULT_DSTORE_PORT 7401

/* Health Check Defaults */
#define PGRAM_DEFAULT_HEALTH_CHECK_INTERVAL 5 /* seconds */
#define PGRAM_DEFAULT_HEALTH_TIMEOUT 30       /* seconds */

/* Worker Defaults */
#define PGRAM_DEFAULT_MAX_WORKERS 10
#define PGRAM_DEFAULT_WORKER_TIMEOUT 60 /* seconds */

/* Monitoring Defaults */
#define PGRAM_DEFAULT_MONITOR_INTERVAL 10            /* seconds */
#define PGRAM_DEFAULT_REPLICATION_LAG_THRESHOLD 5000 /* microseconds */

/* Network Defaults */
#define PGRAM_DEFAULT_NETWORK_TIMEOUT 30 /* seconds */
#define PGRAM_DEFAULT_CONNECTION_RETRIES 3

/* Quorum Defaults */
#define PGRAM_DEFAULT_QUORUM_SIZE 2
#define PGRAM_DEFAULT_ELECTION_TIMEOUT 5 /* seconds */

#endif /* PGRAM_DEFAULTS_H */
