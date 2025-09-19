#!/bin/bash
set -e

# Function to wait for PostgreSQL to be ready
wait_for_postgres() {
    local host=$1
    local port=$2
    local max_attempts=30
    local attempt=1
    
    echo "Waiting for PostgreSQL at $host:$port to be ready..."
    
    while [ $attempt -le $max_attempts ]; do
        if pg_isready -h "$host" -p "$port" -U postgres >/dev/null 2>&1; then
            echo "PostgreSQL is ready at $host:$port"
            return 0
        fi
        
        echo "Attempt $attempt/$max_attempts: PostgreSQL not ready yet, waiting..."
        sleep 2
        attempt=$((attempt + 1))
    done
    
    echo "PostgreSQL failed to become ready after $max_attempts attempts"
    return 1
}

# Function to initialize pgraft extension
init_pgraft() {
    local db_name=${1:-postgres}
    
    echo "Initializing pgraft extension in database $db_name..."
    
    # Wait for PostgreSQL to be ready
    wait_for_postgres "localhost" "5432"
    
    # Create extension
    psql -U postgres -d "$db_name" -c "CREATE EXTENSION IF NOT EXISTS pgraft;"
    
    # Initialize cluster if this is the primary node
    if [ "$PGRaft_NODE_ID" = "1" ]; then
        echo "Initializing cluster as primary node..."
        psql -U postgres -d "$db_name" -c "SELECT pgraft_init_cluster('$PGRaft_CLUSTER_NAME', $PGRaft_NODE_ID, '$PGRaft_NODE_NAME');"
    else
        echo "Joining cluster as node $PGRaft_NODE_ID..."
        psql -U postgres -d "$db_name" -c "SELECT pgraft_join_cluster('$PGRaft_CLUSTER_NAME', $PGRaft_NODE_ID, '$PGRaft_NODE_NAME');"
    fi
}

# Function to start ramd daemon
start_ramd() {
    echo "Starting ramd daemon..."
    
    # Start ramd in background
    ramd --config /etc/ramd/ramd.conf --daemon &
    
    # Wait for ramd to be ready
    local max_attempts=30
    local attempt=1
    
    while [ $attempt -le $max_attempts ]; do
        if curl -s http://localhost:8080/health >/dev/null 2>&1; then
            echo "ramd daemon is ready"
            return 0
        fi
        
        echo "Attempt $attempt/$max_attempts: ramd not ready yet, waiting..."
        sleep 2
        attempt=$((attempt + 1))
    done
    
    echo "ramd daemon failed to become ready after $max_attempts attempts"
    return 1
}

# Main execution
case "$1" in
    postgres)
        echo "Starting PostgreSQL with pgraft extension..."
        
        # Start PostgreSQL
        exec postgres "$@"
        ;;
    init)
        echo "Initializing pgraft cluster..."
        init_pgraft "${2:-postgres}"
        ;;
    ramd)
        echo "Starting ramd daemon..."
        start_ramd
        ;;
    *)
        echo "Starting PostgreSQL with pgraft extension and ramd daemon..."
        
        # Start PostgreSQL in background
        postgres "$@" &
        POSTGRES_PID=$!
        
        # Wait for PostgreSQL to be ready
        wait_for_postgres "localhost" "5432"
        
        # Initialize pgraft
        init_pgraft "${POSTGRES_DB:-postgres}"
        
        # Start ramd daemon
        start_ramd
        
        # Wait for PostgreSQL process
        wait $POSTGRES_PID
        ;;
esac