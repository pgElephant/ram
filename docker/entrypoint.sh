#!/bin/bash
set -e

# Function to start PostgreSQL
start_postgres() {
    echo "Starting PostgreSQL..."
    exec postgres -D /var/lib/postgresql/data \
        -c config_file=/etc/postgresql/postgresql.conf \
        -c hba_file=/etc/postgresql/pg_hba.conf
}

# Function to start ramd daemon
start_ramd() {
    echo "Starting ramd daemon..."
    ramd start --config /etc/ramd/ramd.conf &
    RAMD_PID=$!
    echo "ramd started with PID: $RAMD_PID"
}

# Function to wait for PostgreSQL to be ready
wait_for_postgres() {
    echo "Waiting for PostgreSQL to be ready..."
    until pg_isready -h localhost -p 5432 -U postgres; do
        echo "PostgreSQL is not ready yet..."
        sleep 2
    done
    echo "PostgreSQL is ready!"
}

# Function to initialize pgraft extension
init_pgraft() {
    echo "Initializing pgraft extension..."
    psql -h localhost -p 5432 -U postgres -d postgres -c "CREATE EXTENSION IF NOT EXISTS pgraft;"
    echo "pgraft extension initialized!"
}

# Function to setup cluster configuration
setup_cluster() {
    echo "Setting up cluster configuration..."
    psql -h localhost -p 5432 -U postgres -d postgres << EOF
-- Configure pgraft
ALTER SYSTEM SET pgraft.enabled = on;
ALTER SYSTEM SET pgraft.node_id = ${PGRAPT_NODE_ID:-1};
ALTER SYSTEM SET pgraft.cluster_addresses = '${PGRAPT_CLUSTER_ADDRESSES:-localhost:5432}';
ALTER SYSTEM SET pgraft.heartbeat_interval = ${PGRAPT_HEARTBEAT_INTERVAL:-1000};
ALTER SYSTEM SET pgraft.election_timeout = ${PGRAPT_ELECTION_TIMEOUT:-5000};

-- Reload configuration
SELECT pg_reload_conf();
EOF
    echo "Cluster configuration completed!"
}

# Function to handle shutdown
shutdown() {
    echo "Shutting down services..."
    if [ ! -z "$RAMD_PID" ]; then
        kill $RAMD_PID 2>/dev/null || true
    fi
    pg_ctl stop -D /var/lib/postgresql/data
    exit 0
}

# Set up signal handlers
trap shutdown SIGTERM SIGINT

# Start PostgreSQL in background
start_postgres &
POSTGRES_PID=$!

# Wait for PostgreSQL to be ready
wait_for_postgres

# Initialize pgraft extension
init_pgraft

# Setup cluster configuration
setup_cluster

# Start ramd daemon
start_ramd

# Wait for PostgreSQL process
wait $POSTGRES_PID
