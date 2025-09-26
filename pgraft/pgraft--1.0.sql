/*
 * pgraft--1.0.sql
 * PostgreSQL extension for distributed consensus
 *
 * This file contains the SQL definitions for the pgraft extension.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 * All rights reserved.
 */

-- Create extension schema
CREATE SCHEMA pgraft;

-- ============================================================================
-- Core Raft Functions
-- ============================================================================

-- Initialize pgraft with node configuration
CREATE OR REPLACE FUNCTION pgraft_init(node_id integer, address text, port integer)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_init';

-- Initialize pgraft with configuration from GUC variables
CREATE OR REPLACE FUNCTION pgraft_init()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_init_guc';

-- Start pgraft consensus process
CREATE OR REPLACE FUNCTION pgraft_start()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_start';

-- Add a node to the cluster
CREATE OR REPLACE FUNCTION pgraft_add_node(node_id integer, address text, port integer)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_add_node';

-- Remove a node from the cluster
CREATE OR REPLACE FUNCTION pgraft_remove_node(node_id integer)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_remove_node';

-- Get cluster status
CREATE OR REPLACE FUNCTION pgraft_get_cluster_status()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_cluster_status';

-- Get current leader ID
CREATE OR REPLACE FUNCTION pgraft_get_leader()
RETURNS bigint
LANGUAGE C
AS 'pgraft', 'pgraft_get_leader';

-- Get current term
CREATE OR REPLACE FUNCTION pgraft_get_term()
RETURNS integer
LANGUAGE C
AS 'pgraft', 'pgraft_get_term';

-- Check if current node is leader
CREATE OR REPLACE FUNCTION pgraft_is_leader()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_is_leader';

-- Get cluster nodes information
CREATE OR REPLACE FUNCTION pgraft_get_nodes()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_nodes';

-- Get version information
CREATE OR REPLACE FUNCTION pgraft_get_version()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_version';

-- Test pgraft functionality
CREATE OR REPLACE FUNCTION pgraft_test()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_test';

-- Set debug mode
CREATE OR REPLACE FUNCTION pgraft_set_debug(enabled boolean)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_set_debug';

-- ============================================================================
-- Log Replication Functions
-- ============================================================================

-- Append log entry
CREATE OR REPLACE FUNCTION pgraft_log_append(term bigint, data text)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_log_append';

-- Commit log entry
CREATE OR REPLACE FUNCTION pgraft_log_commit(index bigint)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_log_commit';

-- Apply log entry
CREATE OR REPLACE FUNCTION pgraft_log_apply(index bigint)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_log_apply';

-- Get log entry
CREATE OR REPLACE FUNCTION pgraft_log_get_entry(index bigint)
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_log_get_entry_sql';

-- Get log statistics
CREATE OR REPLACE FUNCTION pgraft_log_get_stats()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_log_get_stats';

-- Get replication status
CREATE OR REPLACE FUNCTION pgraft_log_get_replication_status()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_log_get_replication_status_sql';

-- Sync with leader
CREATE OR REPLACE FUNCTION pgraft_log_sync_with_leader()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_log_sync_with_leader_sql';

-- Get cluster state (alias for pgraft_get_cluster_status)
CREATE OR REPLACE FUNCTION pgraft_get_state()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_cluster_status';

-- Reset search path
SET search_path = public;