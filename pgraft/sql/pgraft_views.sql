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
WHERE event_type IN ('LEADER_ELECTION', 'CLUSTER_BOOTSTRAP', 'NODE_JOIN', 'NODE_JOIN_ATTEMPT', 'NODE_JOIN_FAILED')
ORDER BY timestamp DESC;

-- View showing node health status
CREATE OR REPLACE VIEW pgraft.node_health AS
SELECT 
    node_id,
    hostname,
    port,
    is_online,
    last_heartbeat,
    replication_lag_bytes,
    replication_lag_seconds,
    health_score,
    CASE 
        WHEN health_score >= 90 THEN 'EXCELLENT'
        WHEN health_score >= 70 THEN 'GOOD'
        WHEN health_score >= 50 THEN 'FAIR'
        WHEN health_score >= 30 THEN 'POOR'
        ELSE 'CRITICAL'
    END as health_status,
    CASE 
        WHEN is_online AND health_score >= 70 THEN true
        ELSE false
    END as is_healthy
FROM pgraft.cluster_nodes 
ORDER BY health_score DESC;

-- View showing replication status
CREATE OR REPLACE VIEW pgraft.replication_status AS
SELECT 
    primary_node_id,
    standby_node_id,
    standby_hostname,
    standby_port,
    is_replicating,
    replication_lag_bytes,
    replication_lag_seconds,
    last_received_lsn,
    last_applied_lsn,
    CASE 
        WHEN replication_lag_seconds < 1 THEN 'EXCELLENT'
        WHEN replication_lag_seconds < 5 THEN 'GOOD'
        WHEN replication_lag_seconds < 30 THEN 'FAIR'
        WHEN replication_lag_seconds < 60 THEN 'POOR'
        ELSE 'CRITICAL'
    END as lag_status,
    now() as check_time
FROM pgraft.replication_slots 
WHERE is_replicating = true
ORDER BY replication_lag_seconds ASC;

-- View showing cluster metrics
CREATE OR REPLACE VIEW pgraft.cluster_metrics AS
SELECT 
    'cluster_size' as metric_name,
    pgraft_cluster_size as metric_value,
    'nodes' as unit,
    now() as timestamp
UNION ALL
SELECT 
    'healthy_nodes' as metric_name,
    (SELECT COUNT(*) FROM pgraft.node_health WHERE is_healthy = true) as metric_value,
    'nodes' as unit,
    now() as timestamp
UNION ALL
SELECT 
    'avg_replication_lag' as metric_name,
    COALESCE(AVG(replication_lag_seconds), 0) as metric_value,
    'seconds' as unit,
    now() as timestamp
FROM pgraft.replication_status
UNION ALL
SELECT 
    'total_operations' as metric_name,
    pgraft_total_operations as metric_value,
    'operations' as unit,
    now() as timestamp
UNION ALL
SELECT 
    'failed_operations' as metric_name,
    pgraft_failed_operations as metric_value,
    'operations' as unit,
    now() as timestamp;

-- View showing consensus statistics
CREATE OR REPLACE VIEW pgraft.consensus_stats AS
SELECT 
    current_term,
    voted_for,
    leader_id,
    last_log_index,
    last_log_term,
    commit_index,
    last_applied,
    votes_received,
    CASE 
        WHEN leader_id = pgraft_node_id THEN 'LEADER'
        WHEN voted_for = pgraft_node_id THEN 'CANDIDATE'
        ELSE 'FOLLOWER'
    END as current_state,
    CASE 
        WHEN last_heartbeat > 0 THEN 
            EXTRACT(EPOCH FROM (now() - to_timestamp(last_heartbeat/1000)))::int
        ELSE NULL
    END as seconds_since_heartbeat,
    now() as stats_time
FROM pgraft.raft_state;

-- View showing cluster events summary
CREATE OR REPLACE VIEW pgraft.events_summary AS
SELECT 
    event_type,
    COUNT(*) as event_count,
    MIN(timestamp) as first_occurrence,
    MAX(timestamp) as last_occurrence,
    CASE 
        WHEN event_type = 'LEADER_ELECTION' THEN 'Leadership changes'
        WHEN event_type = 'NODE_JOIN' THEN 'Node additions'
        WHEN event_type = 'NODE_JOIN_FAILED' THEN 'Failed node additions'
        WHEN event_type = 'CLUSTER_BOOTSTRAP' THEN 'Cluster initialization'
        ELSE 'Other events'
    END as description
FROM pgraft.cluster_events 
GROUP BY event_type
ORDER BY event_count DESC;

-- View showing performance metrics
CREATE OR REPLACE VIEW pgraft.performance_metrics AS
SELECT 
    'avg_operation_time' as metric_name,
    COALESCE(AVG(operation_time_ms), 0) as metric_value,
    'milliseconds' as unit,
    now() as timestamp
FROM pgraft.operation_log 
WHERE timestamp > now() - interval '1 hour'
UNION ALL
SELECT 
    'operations_per_second' as metric_name,
    COUNT(*) / 3600.0 as metric_value,
    'ops/sec' as unit,
    now() as timestamp
FROM pgraft.operation_log 
WHERE timestamp > now() - interval '1 hour'
UNION ALL
SELECT 
    'error_rate' as metric_name,
    (COUNT(*) FILTER (WHERE success = false) * 100.0 / COUNT(*)) as metric_value,
    'percent' as unit,
    now() as timestamp
FROM pgraft.operation_log 
WHERE timestamp > now() - interval '1 hour';

-- Grant permissions to pgraft role
GRANT SELECT ON pgraft.cluster_overview TO pgraft_role;
GRANT SELECT ON pgraft.cluster_formation TO pgraft_role;
GRANT SELECT ON pgraft.election_activity TO pgraft_role;
GRANT SELECT ON pgraft.node_health TO pgraft_role;
GRANT SELECT ON pgraft.replication_status TO pgraft_role;
GRANT SELECT ON pgraft.cluster_metrics TO pgraft_role;
GRANT SELECT ON pgraft.consensus_stats TO pgraft_role;
GRANT SELECT ON pgraft.events_summary TO pgraft_role;
GRANT SELECT ON pgraft.performance_metrics TO pgraft_role;

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
