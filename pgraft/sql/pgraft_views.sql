-- pgraft cluster monitoring views
-- Copyright (c) 2024-2025, pgElephant, Inc.

-- View showing current cluster state
CREATE OR REPLACE VIEW pgraft.cluster_overview AS
SELECT 
    pgraft_node_id as this_node_id,
    COALESCE(pgraft_node_name, 'pgraft_node_' || pgraft_node_id::text) as this_node_name,
    pgraft_is_primary as is_leader,
    CASE 
        WHEN pgraft_is_primary THEN 'LEADER'
        ELSE 'FOLLOWER'
    END as current_role,
    pgraft_cluster_size as cluster_size,
    (pgraft_cluster_size >= 1) as has_quorum,
    pgraft_port as raft_port,
    COALESCE(pgraft_node_ip, '127.0.0.1') as node_ip,
    now() as status_time;

-- View showing cluster formation progress
CREATE OR REPLACE VIEW pgraft.cluster_formation AS
SELECT 
    node_id,
    event_type,
    details,
    timestamp,
    CASE 
        WHEN event_type = 'CLUSTER_BOOTSTRAP' THEN 'Bootstrap node initialized'
        WHEN event_type = 'NODE_JOIN' THEN 'Node successfully joined'
        WHEN event_type = 'NODE_JOIN_ATTEMPT' THEN 'Node attempting to join'
        WHEN event_type = 'NODE_JOIN_FAILED' THEN 'Node failed to join'
        WHEN event_type = 'LEADER_ELECTION' THEN 'Leader elected'
        WHEN event_type = 'CLUSTER_READY' THEN 'Cluster ready for nodes'
        ELSE event_type
    END as status_description
FROM pgraft.cluster_events 
ORDER BY timestamp DESC;

-- View showing election activity
CREATE OR REPLACE VIEW pgraft.election_activity AS
SELECT 
    node_id,
    event_type,
    details,
    timestamp,
    CASE 
        WHEN event_type = 'LEADER_ELECTION' THEN 'ELECTION'
        WHEN event_type = 'CLUSTER_BOOTSTRAP' THEN 'BOOTSTRAP'
        WHEN event_type = 'NODE_JOIN' THEN 'JOIN'
        ELSE 'OTHER'
    END as activity_type
FROM pgraft.cluster_events 
WHERE event_type IN ('LEADER_ELECTION', 'CLUSTER_BOOTSTRAP', 'NODE_JOIN', 'NODE_JOIN_ATTEMPT')
ORDER BY timestamp DESC;

-- View for network status
CREATE OR REPLACE VIEW pgraft.network_status AS
SELECT 
    pgraft_node_id as node_id,
    COALESCE(pgraft_node_ip, '127.0.0.1') as node_ip,
    pgraft_port as raft_port,
    pgraft_is_primary as is_leader,
    pgraft_cluster_size as connected_nodes,
    CASE 
        WHEN pgraft_is_primary THEN 'LEADER'
        WHEN pgraft_cluster_size > 1 THEN 'FOLLOWER'
        ELSE 'STANDALONE'
    END as node_status,
    now() as check_time;

-- View for health status
CREATE OR REPLACE VIEW pgraft.health_status AS
SELECT 
    pgraft_node_id as node_id,
    'HEALTHY' as status,
    pgraft_health_period_ms as check_interval_ms,
    pgraft_health_verbose as verbose_logging,
    now() as last_check,
    'pgraft' as extension_name;
