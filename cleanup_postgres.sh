#!/bin/bash

# PostgreSQL Cleanup Script - Fixes quit issue
# This script ensures all PostgreSQL processes are properly stopped

echo "🧹 === CLEANING UP POSTGRESQL PROCESSES === 🧹"

# Step 1: Try graceful shutdown first
echo "Step 1: Attempting graceful shutdown..."
for port in 5433 5434 5435; do
    data_dir="/usr/local/pgsql.17/data$((port - 5432))"
    if [ -f "$data_dir/postmaster.pid" ]; then
        echo "  Stopping node on port $port..."
        /usr/local/pgsql.17/bin/pg_ctl -D "$data_dir" stop -m fast -w -t 10 2>/dev/null || true
    fi
done

# Step 2: Force kill any remaining processes
echo "Step 2: Force killing remaining PostgreSQL processes..."
pkill -9 postgres 2>/dev/null || true
sleep 2

# Step 3: Clean up lock files
echo "Step 3: Cleaning up lock files..."
rm -f /usr/local/pgsql.17/data*/postmaster.pid
rm -f /usr/local/pgsql.17/data*/postmaster.opts

# Step 4: Clean up shared memory segments
echo "Step 4: Cleaning up shared memory..."
for shm_id in $(ipcs -m | grep -E "^0x|^m" | awk '{print $2}' | grep -E "^[0-9]+$"); do
    ipcrm -m "$shm_id" 2>/dev/null || true
done

# Step 5: Clean up state files
echo "Step 5: Cleaning up state files..."
rm -f /tmp/pg_ram_cluster_node_*.conf

# Step 6: Verify cleanup
echo "Step 6: Verifying cleanup..."
if pgrep postgres >/dev/null; then
    echo "❌ Warning: Some PostgreSQL processes are still running"
    ps aux | grep postgres | grep -v grep
else
    echo "✅ All PostgreSQL processes stopped"
fi

echo "✅ Cleanup completed!"
