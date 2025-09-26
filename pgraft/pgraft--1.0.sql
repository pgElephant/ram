/*
 * pgraft--1.0.sql
 * PostgreSQL extension for distributed consensus
 *
 * This file contains the SQL definitions for the pgraft extension.
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 * All rights reserved.
 */

-- Create extension schema
CREATE SCHEMA pgraft;

-- ============================================================================
-- Core Raft Functions
-- ============================================================================

-- Initialize pgraft with node configuration
CREATE OR REPLACE FUNCTION pgraft_init()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_init';

-- Start the Raft consensus process
CREATE OR REPLACE FUNCTION pgraft_start()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_start';

-- Stop the Raft consensus process
CREATE OR REPLACE FUNCTION pgraft_stop()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_stop';

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

-- Set debug logging level
CREATE OR REPLACE FUNCTION pgraft_set_debug(enabled boolean)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_set_debug';

-- Get current Raft state
CREATE OR REPLACE FUNCTION pgraft_get_state()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_state';

-- Get current leader ID
CREATE OR REPLACE FUNCTION pgraft_get_leader()
RETURNS integer
LANGUAGE C
AS 'pgraft', 'pgraft_get_leader';

-- Get cluster nodes information
CREATE OR REPLACE FUNCTION pgraft_get_nodes()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_nodes';

-- Get log information
CREATE OR REPLACE FUNCTION pgraft_get_log()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_log';

-- Get statistics
CREATE OR REPLACE FUNCTION pgraft_get_stats()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_stats';

-- Append log entry
CREATE OR REPLACE FUNCTION pgraft_append_log(data text)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_append_log';

-- Commit log entry
CREATE OR REPLACE FUNCTION pgraft_commit_log(index integer)
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_commit_log';

-- Read log entry
CREATE OR REPLACE FUNCTION pgraft_read_log(index integer)
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_read_log';

-- Get version information
CREATE OR REPLACE FUNCTION pgraft_version()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_version';

CREATE OR REPLACE FUNCTION pgraft_test()
RETURNS integer
LANGUAGE C
AS 'pgraft', 'pgraft_test';

-- Check if current node is leader
CREATE OR REPLACE FUNCTION pgraft_is_leader()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_is_leader';

-- Get current term
CREATE OR REPLACE FUNCTION pgraft_get_term()
RETURNS integer
LANGUAGE C
AS 'pgraft', 'pgraft_get_term';

-- ============================================================================
-- Monitoring Functions
-- ============================================================================

-- Get cluster health status
CREATE OR REPLACE FUNCTION pgraft_get_cluster_health()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_cluster_health';

-- Get performance metrics
CREATE OR REPLACE FUNCTION pgraft_get_performance_metrics()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_performance_metrics';

-- Check if cluster is healthy
CREATE OR REPLACE FUNCTION pgraft_is_cluster_healthy()
RETURNS boolean
LANGUAGE C
AS 'pgraft', 'pgraft_is_cluster_healthy';

-- Get system statistics
CREATE OR REPLACE FUNCTION pgraft_get_system_stats()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_system_stats';

-- Get quorum status
CREATE OR REPLACE FUNCTION pgraft_get_quorum_status()
RETURNS text
LANGUAGE C
AS 'pgraft', 'pgraft_get_quorum_status';

-- Reset search path
SET search_path = public;