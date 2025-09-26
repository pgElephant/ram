#!/bin/bash

# RAM Manual Testing Setup Script
# PostgreSQL 17 + RAM Cluster Testing

set -e

# Configuration
PG_BIN="/usr/local/pgsql.17/bin"
PG_VERSION="17"
CLUSTER_NAME="ram_test_cluster"
BASE_PORT=5433
RAM_BASE_PORT=8080
TEST_DIR="/tmp/ram_test_$$"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== RAM Manual Testing Setup ===${NC}"
echo "PostgreSQL Version: $PG_VERSION"
echo "PostgreSQL Binary: $PG_BIN"
echo "Test Directory: $TEST_DIR"
echo ""

# Create test directory
mkdir -p $TEST_DIR
cd $TEST_DIR

echo -e "${YELLOW}1. Creating test directories...${NC}"
for i in {1..3}; do
    mkdir -p "node$i/data"
    mkdir -p "node$i/logs"
    mkdir -p "node$i/conf"
done

echo -e "${YELLOW}2. Initializing PostgreSQL clusters...${NC}"
for i in {1..3}; do
    PORT=$((BASE_PORT + i - 1))
    echo "Initializing node$i on port $PORT"
    
    # Initialize database
    $PG_BIN/initdb -D "node$i/data" --auth-local=trust --auth-host=md5
    
    # Create postgresql.conf
    cat > "node$i/conf/postgresql.conf" << EOF
# PostgreSQL 17 Configuration for RAM Testing
port = $PORT
listen_addresses = 'localhost'
max_connections = 100
shared_buffers = 128MB
wal_level = replica
max_wal_senders = 10
max_replication_slots = 10
hot_standby = on
archive_mode = on
archive_command = 'true'
wal_sender_timeout = 60s
wal_receiver_timeout = 60s
wal_receiver_status_interval = 10s
hot_standby_feedback = on
log_destination = 'stderr'
logging_collector = on
log_directory = 'logs'
log_filename = 'postgresql-%Y-%m-%d_%H%M%S.log'
log_min_messages = info
log_checkpoints = on
log_connections = on
log_disconnections = on
log_lock_waits = on
log_temp_files = -1
log_autovacuum_min_duration = 0
log_line_prefix = '%t [%p]: [%l-1] user=%u,db=%d,app=%a,client=%h '
log_statement = 'all'
shared_preload_libraries = 'pgraft'
pgraft.port = $((RAM_BASE_PORT + i - 1))
pgraft.cluster_name = '$CLUSTER_NAME'
pgraft.node_id = 'node$i'
pgraft.raft_port = $((RAM_BASE_PORT + i - 1 + 100))
EOF

    # Create pg_hba.conf
    cat > "node$i/conf/pg_hba.conf" << EOF
# PostgreSQL 17 Authentication for RAM Testing
local   all             all                                     trust
host    all             all             127.0.0.1/32            trust
host    all             all             ::1/128                 trust
host    replication     postgres        127.0.0.1/32            trust
host    replication     postgres        ::1/128                 trust
EOF

    # Copy configs to data directory
    cp "node$i/conf/postgresql.conf" "node$i/data/"
    cp "node$i/conf/pg_hba.conf" "node$i/data/"
done

echo -e "${YELLOW}3. Installing pgraft extension...${NC}"
for i in {1..3}; do
    PORT=$((BASE_PORT + i - 1))
    echo "Installing pgraft on node$i"
    
    # Copy pgraft extension files
    cp /Users/ibrarahmed/pgelephant/pge/ram/pgraft/pgraft.dylib "node$i/data/"
    cp /Users/ibrarahmed/pgelephant/pge/ram/pgraft/pgraft.control "node$i/data/"
    cp /Users/ibrarahmed/pgelephant/pge/ram/pgraft/pgraft--1.0.sql "node$i/data/"
    
    # Create extension directory
    mkdir -p "node$i/data/extension"
    cp /Users/ibrarahmed/pgelephant/pge/ram/pgraft/pgraft.control "node$i/data/extension/"
    cp /Users/ibrarahmed/pgelephant/pge/ram/pgraft/pgraft--1.0.sql "node$i/data/extension/"
done

echo -e "${YELLOW}4. Starting PostgreSQL clusters...${NC}"
for i in {1..3}; do
    PORT=$((BASE_PORT + i - 1))
    echo "Starting node$i on port $PORT"
    
    $PG_BIN/pg_ctl -D "node$i/data" -l "node$i/logs/postgresql.log" start
    sleep 2
done

echo -e "${YELLOW}5. Verifying PostgreSQL clusters...${NC}"
for i in {1..3}; do
    PORT=$((BASE_PORT + i - 1))
    echo "Checking node$i on port $PORT"
    
    if $PG_BIN/pg_isready -p $PORT -h localhost; then
        echo -e "${GREEN}✓ Node$i is ready${NC}"
    else
        echo -e "${RED}✗ Node$i failed to start${NC}"
        exit 1
    fi
done

echo -e "${YELLOW}6. Installing pgraft extension in databases...${NC}"
for i in {1..3}; do
    PORT=$((BASE_PORT + i - 1))
    echo "Installing pgraft extension on node$i"
    
    $PG_BIN/psql -p $PORT -h localhost -d postgres -c "CREATE EXTENSION IF NOT EXISTS pgraft;"
    $PG_BIN/psql -p $PORT -h localhost -d postgres -c "SELECT * FROM pg_extension WHERE extname = 'pgraft';"
done

echo -e "${YELLOW}7. Creating RAM cluster configuration...${NC}"
cat > ramd.conf << EOF
# RAM Cluster Configuration
cluster_name = "$CLUSTER_NAME"
node_id = "node1"
http_port = $RAM_BASE_PORT
raft_port = $((RAM_BASE_PORT + 100))

# Cluster nodes
nodes = [
    {
        node_id = "node1"
        host = "localhost"
        port = $BASE_PORT
        http_port = $RAM_BASE_PORT
        raft_port = $((RAM_BASE_PORT + 100))
        role = "primary"
    },
    {
        node_id = "node2"
        host = "localhost"
        port = $((BASE_PORT + 1))
        http_port = $((RAM_BASE_PORT + 1))
        raft_port = $((RAM_BASE_PORT + 101))
        role = "standby"
    },
    {
        node_id = "node3"
        host = "localhost"
        port = $((BASE_PORT + 2))
        http_port = $((RAM_BASE_PORT + 2))
        raft_port = $((RAM_BASE_PORT + 102))
        role = "standby"
    }
]

# PostgreSQL configuration
postgresql = {
    user = "postgres"
    database = "postgres"
    connection_timeout = 30
    query_timeout = 60
}

# Raft configuration
raft = {
    election_timeout = 1000
    heartbeat_interval = 500
    max_entries_per_request = 256
    snapshot_interval = 10000
}

# Monitoring
monitoring = {
    enabled = true
    prometheus_port = 9090
    metrics_interval = 10
}

# Logging
logging = {
    level = "info"
    file = "ramd.log"
    max_size = 100
    max_files = 5
}
EOF

echo -e "${GREEN}=== RAM Test Environment Ready ===${NC}"
echo "Test Directory: $TEST_DIR"
echo "PostgreSQL Ports: $BASE_PORT, $((BASE_PORT + 1)), $((BASE_PORT + 2))"
echo "RAM HTTP Ports: $RAM_BASE_PORT, $((RAM_BASE_PORT + 1)), $((RAM_BASE_PORT + 2))"
echo ""
echo -e "${BLUE}Next Steps:${NC}"
echo "1. cd $TEST_DIR"
echo "2. Start ramd: ./ramd -config ramd.conf"
echo "3. Test with ramctrl: ./ramctrl status"
echo "4. Test failover scenarios"
echo ""
echo -e "${YELLOW}To clean up:${NC}"
echo "cd $TEST_DIR && ./cleanup.sh"
