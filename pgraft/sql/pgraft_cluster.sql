-- pgraft cluster management functions
-- Copyright (c) 2024-2025, pgElephant, Inc.

-- Function to get cluster status
CREATE OR REPLACE FUNCTION pgraft_get_cluster_status() 
RETURNS TABLE(
    node_count integer,
    is_leader boolean,
    leader_id integer,
    has_quorum boolean
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        pgraft_cluster_size as node_count,
        pgraft_is_primary as is_leader,
        CASE WHEN pgraft_is_primary THEN pgraft_node_id ELSE 0 END as leader_id,
        (pgraft_cluster_size >= 1) as has_quorum;
END;
$$ LANGUAGE plpgsql;

-- Function to check if current node is leader
CREATE OR REPLACE FUNCTION pgraft_is_leader() RETURNS boolean AS $$
BEGIN
    RETURN pgraft_is_primary;
END;
$$ LANGUAGE plpgsql;

-- Function to get current node information
CREATE OR REPLACE FUNCTION pgraft_get_node_info() 
RETURNS TABLE(
    node_id integer,
    node_name text,
    node_ip text,
    is_primary boolean,
    cluster_name text
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        pgraft_node_id as node_id,
        COALESCE(pgraft_node_name, 'pgraft_node_' || pgraft_node_id::text) as node_name,
        COALESCE(pgraft_node_ip, '127.0.0.1') as node_ip,
        pgraft_is_primary as is_primary,
        COALESCE(pgraft_cluster_name, 'pgraft_cluster') as cluster_name;
END;
$$ LANGUAGE plpgsql;

-- Function to add a node to cluster (simplified)
CREATE OR REPLACE FUNCTION pgraft_add_node(
    node_id integer,
    node_ip text,
    node_port integer DEFAULT 5432
) RETURNS boolean AS $$
BEGIN
    -- Log the cluster join event
    INSERT INTO pgraft.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('NODE_JOIN', node_id, 
            format('Node %s (%s:%s) joined cluster', node_id, node_ip, node_port),
            now());
            
    RETURN true;
EXCEPTION WHEN OTHERS THEN
    RETURN false;
END;
$$ LANGUAGE plpgsql;

-- Function to bootstrap cluster
CREATE OR REPLACE FUNCTION pgraft_bootstrap_cluster() RETURNS boolean AS $$
BEGIN
    -- Log the cluster bootstrap event
    INSERT INTO pgraft.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('CLUSTER_BOOTSTRAP', pgraft_node_id, 
            format('Bootstrap node %s initializing cluster', pgraft_node_id),
            now());
    
    -- Set this node as primary if configured
    IF pgraft_is_primary THEN
        INSERT INTO pgraft.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('LEADER_ELECTION', pgraft_node_id, 
                format('Node %s elected as initial leader', pgraft_node_id),
                now());
    END IF;
    
    RETURN true;
END;
$$ LANGUAGE plpgsql;

-- Create cluster events table for logging
CREATE TABLE IF NOT EXISTS pgraft.cluster_events (
    id serial PRIMARY KEY,
    event_type text NOT NULL,
    node_id integer,
    details text,
    timestamp timestamp DEFAULT now()
);

-- Create schema if it doesn't exist
CREATE SCHEMA IF NOT EXISTS pgraft;
