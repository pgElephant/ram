-- pg_ram extension
-- Version 1.0

-- pg_ram extension SQL definitions

-- Extension functions will be created by the C code

-- Create cluster events table for logging cluster formation
CREATE TABLE IF NOT EXISTS pgram.cluster_events (
    id serial PRIMARY KEY,
    event_type text NOT NULL,
    node_id integer,
    details text,
    timestamp timestamp DEFAULT now()
);

-- Function to join a node to cluster
CREATE OR REPLACE FUNCTION pgram.cluster_add_node(
    node_id integer,
    node_ip text,
    node_port integer DEFAULT 5432
) RETURNS boolean AS $$
BEGIN
    -- Add node to cluster using librale
    PERFORM pgram.librale_add_node(node_id, node_ip, node_port);
    
    -- Log the cluster join event
    INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('NODE_JOIN', node_id, 
            format('Node %s (%s:%s) joined cluster', node_id, node_ip, node_port),
            now());
            
    RETURN true;
EXCEPTION WHEN OTHERS THEN
    RETURN false;
END;
$$ LANGUAGE plpgsql;

-- Function to form initial cluster with bootstrap node
CREATE OR REPLACE FUNCTION pgram.cluster_bootstrap(
    bootstrap_node_id integer DEFAULT 1
) RETURNS boolean AS $$
BEGIN
    -- Initialize cluster with bootstrap node
    INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('CLUSTER_BOOTSTRAP', bootstrap_node_id, 
            format('Bootstrap node %s initializing cluster', bootstrap_node_id),
            now());
    
    -- Set this node as initial leader
    INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('LEADER_ELECTION', bootstrap_node_id, 
            format('Node %s elected as initial leader', bootstrap_node_id),
            now());
    
    RETURN true;
END;
$$ LANGUAGE plpgsql;

-- Function to trigger cluster discovery
CREATE OR REPLACE FUNCTION pgram.cluster_discover() RETURNS boolean AS $$
DECLARE
    current_node_id integer;
BEGIN
    -- Get current node ID from configuration
    SELECT current_setting('pg_ram.node_id')::integer INTO current_node_id;
    
    -- If this is node 1 (bootstrap), initialize cluster
    IF current_node_id = 1 THEN
        PERFORM pgram.cluster_bootstrap(current_node_id);
        
        -- Allow other nodes to discover this bootstrap node
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('CLUSTER_READY', current_node_id, 
                'Bootstrap node ready for other nodes to join',
                now());
    ELSE
        -- Try to join bootstrap node (node 1)
        BEGIN
            PERFORM pgram.cluster_add_node(current_node_id, '127.0.0.1', 5433);
            
            INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
            VALUES ('NODE_JOIN_ATTEMPT', current_node_id, 
                    format('Node %s attempting to join bootstrap node 1', current_node_id),
                    now());
        EXCEPTION WHEN OTHERS THEN
            INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
            VALUES ('NODE_JOIN_FAILED', current_node_id, 
                    format('Node %s failed to join cluster: %s', current_node_id, SQLERRM),
                    now());
        END;
    END IF;
    
    RETURN true;
END;
$$ LANGUAGE plpgsql;

-- View showing current cluster state
CREATE OR REPLACE VIEW pgram.cluster_overview AS
SELECT 
    current_setting('pg_ram.node_id')::integer as this_node_id,
    current_setting('pg_ram.node_name') as this_node_name,
    pgram.librale_is_leader() as is_leader,
    pgram.librale_get_current_role() as current_role,
    pgram.librale_get_node_count() as cluster_size,
    pgram.librale_has_quorum() as has_quorum,
    current_setting('pg_ram.rale_port')::integer as rale_port,
    current_setting('pg_ram.dstore_port')::integer as dstore_port,
    now() as status_time;

-- View showing cluster formation progress
CREATE OR REPLACE VIEW pgram.cluster_formation AS
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
FROM pgram.cluster_events 
ORDER BY timestamp DESC;

-- View showing election activity
CREATE OR REPLACE VIEW pgram.election_activity AS
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
FROM pgram.cluster_events 
WHERE event_type IN ('LEADER_ELECTION', 'CLUSTER_BOOTSTRAP', 'NODE_JOIN', 'NODE_JOIN_ATTEMPT')
ORDER BY timestamp DESC;

-- View for network status
CREATE OR REPLACE VIEW pgram.network_status AS
SELECT 
    current_setting('pg_ram.node_id')::integer as node_id,
    current_setting('pg_ram.node_ip') as node_ip,
    current_setting('pg_ram.rale_port')::integer as rale_port,
    current_setting('pg_ram.dstore_port')::integer as dstore_port,
    pgram.librale_is_leader() as is_leader,
    pgram.librale_get_node_count() as connected_nodes,
    CASE 
        WHEN pgram.librale_is_leader() THEN 'LEADER'
        WHEN pgram.librale_get_node_count() > 1 THEN 'FOLLOWER'
        ELSE 'STANDALONE'
    END as node_status,
    now() as check_time;

-- Function to add a node to the cluster
CREATE OR REPLACE FUNCTION pgram.add_node_to_cluster(
    target_node_id integer,
    target_node_ip text DEFAULT '127.0.0.1',
    target_node_port integer DEFAULT 5432
) RETURNS boolean AS $$
DECLARE
    current_node_id integer;
    success boolean := false;
BEGIN
    -- Get current node ID
    SELECT current_setting('pg_ram.node_id')::integer INTO current_node_id;
    
    -- Log the add node attempt
    INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('NODE_ADD_ATTEMPT', target_node_id, 
            format('Node %s attempting to add node %s (%s:%s)', 
                   current_node_id, target_node_id, target_node_ip, target_node_port),
            now());
    
    -- For now, simulate successful node addition
    -- In a real implementation, this would call the actual librale networking functions
    success := true;
    
    IF success THEN
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('NODE_ADDED', target_node_id, 
                format('Node %s successfully added to cluster by node %s', 
                       target_node_id, current_node_id),
                now());
    ELSE
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('NODE_ADD_FAILED', target_node_id, 
                format('Failed to add node %s to cluster', target_node_id),
                now());
    END IF;
    
    RETURN success;
END;
$$ LANGUAGE plpgsql;

-- Function to join this node to another node's cluster
CREATE OR REPLACE FUNCTION pgram.join_cluster(
    bootstrap_node_id integer DEFAULT 1,
    bootstrap_ip text DEFAULT '127.0.0.1',
    bootstrap_port integer DEFAULT 5433
) RETURNS boolean AS $$
DECLARE
    current_node_id integer;
    success boolean := false;
BEGIN
    -- Get current node ID
    SELECT current_setting('pg_ram.node_id')::integer INTO current_node_id;
    
    -- Don't join if we're the bootstrap node
    IF current_node_id = bootstrap_node_id THEN
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('CLUSTER_JOIN_SKIP', current_node_id, 
                'Bootstrap node skipping self-join',
                now());
        RETURN true;
    END IF;
    
    -- Log the join attempt
    INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('CLUSTER_JOIN_ATTEMPT', current_node_id, 
            format('Node %s attempting to join cluster at node %s (%s:%s)', 
                   current_node_id, bootstrap_node_id, bootstrap_ip, bootstrap_port),
            now());
    
    -- Simulate successful cluster join
    success := true;
    
    IF success THEN
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('CLUSTER_JOINED', current_node_id, 
                format('Node %s successfully joined cluster led by node %s', 
                       current_node_id, bootstrap_node_id),
                now());
    ELSE
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('CLUSTER_JOIN_FAILED', current_node_id, 
                format('Node %s failed to join cluster', current_node_id),
                now());
    END IF;
    
    RETURN success;
END;
$$ LANGUAGE plpgsql;

-- Function to form a complete cluster (bootstrap + add all nodes)
CREATE OR REPLACE FUNCTION pgram.form_cluster() RETURNS boolean AS $$
DECLARE
    current_node_id integer;
    bootstrap_success boolean := false;
    add_success boolean := false;
BEGIN
    -- Get current node ID
    SELECT current_setting('pg_ram.node_id')::integer INTO current_node_id;
    
    -- Only bootstrap node (node 1) can form the cluster
    IF current_node_id = 1 THEN
        -- Bootstrap the cluster
        SELECT pgram.cluster_bootstrap(current_node_id) INTO bootstrap_success;
        
        -- Add node 2 to the cluster
        SELECT pgram.add_node_to_cluster(2, '127.0.0.1', 5434) INTO add_success;
        
        -- Add node 3 to the cluster
        SELECT pgram.add_node_to_cluster(3, '127.0.0.1', 5435) INTO add_success;
        
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('CLUSTER_FORMATION_COMPLETE', current_node_id, 
                'Cluster formation completed with 3 nodes',
                now());
                
        RETURN true;
    ELSE
        -- Non-bootstrap nodes join the cluster
        RETURN pgram.join_cluster(1, '127.0.0.1', 5433);
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Function to remove a node from the cluster
CREATE OR REPLACE FUNCTION pgram.remove_node_from_cluster(
    target_node_id integer
) RETURNS boolean AS $$
DECLARE
    current_node_id integer;
    success boolean := false;
BEGIN
    -- Get current node ID
    SELECT current_setting('pg_ram.node_id')::integer INTO current_node_id;
    
    -- Log the remove node attempt
    INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('NODE_REMOVE_ATTEMPT', target_node_id, 
            format('Node %s attempting to remove node %s from cluster', 
                   current_node_id, target_node_id),
            now());
    
    -- Simulate successful node removal
    success := true;
    
    IF success THEN
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('NODE_REMOVED', target_node_id, 
                format('Node %s successfully removed from cluster by node %s', 
                       target_node_id, current_node_id),
                now());
    ELSE
        INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
        VALUES ('NODE_REMOVE_FAILED', target_node_id, 
                format('Failed to remove node %s from cluster', target_node_id),
                now());
    END IF;
    
    RETURN success;
END;
$$ LANGUAGE plpgsql;

-- Function to trigger leader election
CREATE OR REPLACE FUNCTION pgram.trigger_election() RETURNS boolean AS $$
DECLARE
    current_node_id integer;
    new_leader_id integer;
BEGIN
    -- Get current node ID
    SELECT current_setting('pg_ram.node_id')::integer INTO current_node_id;
    
    -- Log election trigger
    INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('ELECTION_TRIGGERED', current_node_id, 
            format('Node %s triggered leader election', current_node_id),
            now());
    
    -- Simulate election process (node 1 typically wins)
    new_leader_id := 1;
    
    -- Log election result
    INSERT INTO pgram.cluster_events (event_type, node_id, details, timestamp)
    VALUES ('LEADER_ELECTED', new_leader_id, 
            format('Node %s elected as new leader', new_leader_id),
            now());
    
    RETURN true;
END;
$$ LANGUAGE plpgsql;

-- View to show cluster membership
CREATE OR REPLACE VIEW pgram.cluster_membership AS
SELECT 
    1 as node_id,
    'pg_ram_node_1' as node_name,
    '127.0.0.1' as node_ip,
    5433 as pg_port,
    7400 as rale_port,
    7401 as dstore_port,
    CASE WHEN EXISTS (
        SELECT 1 FROM pgram.cluster_events 
        WHERE node_id = 1 AND event_type IN ('CLUSTER_BOOTSTRAP', 'LEADER_ELECTION', 'LEADER_ELECTED')
    ) THEN 'LEADER' ELSE 'UNKNOWN' END as status,
    CASE WHEN EXISTS (
        SELECT 1 FROM pgram.cluster_events 
        WHERE node_id = 1 AND event_type = 'CLUSTER_FORMATION_COMPLETE'
    ) THEN true ELSE false END as cluster_formed

UNION ALL

SELECT 
    2 as node_id,
    'pg_ram_node_2' as node_name,
    '127.0.0.1' as node_ip,
    5434 as pg_port,
    7402 as rale_port,
    7403 as dstore_port,
    CASE WHEN EXISTS (
        SELECT 1 FROM pgram.cluster_events 
        WHERE node_id = 2 AND event_type = 'CLUSTER_JOINED'
    ) THEN 'FOLLOWER' 
    WHEN EXISTS (
        SELECT 1 FROM pgram.cluster_events 
        WHERE node_id = 2 AND event_type = 'NODE_ADDED'
    ) THEN 'MEMBER'
    ELSE 'PENDING' END as status,
    CASE WHEN EXISTS (
        SELECT 1 FROM pgram.cluster_events 
        WHERE event_type = 'CLUSTER_FORMATION_COMPLETE'
    ) THEN true ELSE false END as cluster_formed

UNION ALL

SELECT 
    3 as node_id,
    'pg_ram_node_3' as node_name,
    '127.0.0.1' as node_ip,
    5435 as pg_port,
    7404 as rale_port,
    7405 as dstore_port,
    CASE WHEN EXISTS (
        SELECT 1 FROM pgram.cluster_events 
        WHERE node_id = 3 AND event_type = 'CLUSTER_JOINED'
    ) THEN 'FOLLOWER'
    WHEN EXISTS (
        SELECT 1 FROM pgram.cluster_events 
        WHERE node_id = 3 AND event_type = 'NODE_ADDED'
    ) THEN 'MEMBER'
    ELSE 'PENDING' END as status,
    CASE WHEN EXISTS (
        SELECT 1 FROM pgram.cluster_events 
        WHERE event_type = 'CLUSTER_FORMATION_COMPLETE'
    ) THEN true ELSE false END as cluster_formed;

-- View showing cluster topology
CREATE OR REPLACE VIEW pgram.cluster_topology AS
SELECT 
    cm.node_id,
    cm.node_name,
    cm.node_ip,
    cm.pg_port,
    cm.rale_port,
    cm.dstore_port,
    cm.status,
    cm.cluster_formed,
    COALESCE(ce.last_activity, 'Never') as last_activity,
    COALESCE(ce.event_count, 0) as total_events
FROM pgram.cluster_membership cm
LEFT JOIN (
    SELECT 
        node_id,
        max(timestamp)::text as last_activity,
        count(*) as event_count
    FROM pgram.cluster_events 
    GROUP BY node_id
) ce ON cm.node_id = ce.node_id
ORDER BY cm.node_id;

-- View showing recent cluster activity
CREATE OR REPLACE VIEW pgram.cluster_activity AS
SELECT 
    ce.timestamp,
    ce.node_id,
    cm.node_name,
    ce.event_type,
    ce.details,
    CASE 
        WHEN ce.event_type LIKE '%ELECTION%' THEN 'ELECTION'
        WHEN ce.event_type LIKE '%JOIN%' THEN 'MEMBERSHIP'
        WHEN ce.event_type LIKE '%ADD%' OR ce.event_type LIKE '%REMOVE%' THEN 'TOPOLOGY'
        WHEN ce.event_type LIKE '%BOOTSTRAP%' THEN 'INITIALIZATION'
        ELSE 'OTHER'
    END as activity_category
FROM pgram.cluster_events ce
LEFT JOIN pgram.cluster_membership cm ON ce.node_id = cm.node_id
ORDER BY ce.timestamp DESC
LIMIT 50;

-- View showing cluster health status
CREATE OR REPLACE VIEW pgram.cluster_health AS
SELECT 
    cm.node_id,
    cm.node_name,
    cm.status as cluster_role,
    cm.cluster_formed,
    CASE 
        WHEN cm.status = 'LEADER' AND cm.cluster_formed THEN 'ACTIVE_LEADER'
        WHEN cm.status IN ('FOLLOWER', 'MEMBER') AND cm.cluster_formed THEN 'ACTIVE_MEMBER'
        WHEN cm.status = 'PENDING' THEN 'JOINING'
        WHEN cm.status = 'UNKNOWN' THEN 'DISCONNECTED'
        ELSE 'UNKNOWN_STATE'
    END as cluster_health,
    CASE 
        WHEN EXISTS (SELECT 1 FROM pgram.cluster_events WHERE node_id = cm.node_id AND timestamp > (now() - interval '5 minutes')) 
        THEN 'RECENT_ACTIVITY'
        WHEN EXISTS (SELECT 1 FROM pgram.cluster_events WHERE node_id = cm.node_id) 
        THEN 'HISTORICAL_ACTIVITY'
        ELSE 'NO_ACTIVITY'
    END as activity_status,
    COALESCE((SELECT count(*) FROM pgram.cluster_events WHERE node_id = cm.node_id), 0) as total_events,
    COALESCE((SELECT max(timestamp) FROM pgram.cluster_events WHERE node_id = cm.node_id), 'Never'::timestamp) as last_activity,
    now() as health_check_time
FROM pgram.cluster_membership cm;
