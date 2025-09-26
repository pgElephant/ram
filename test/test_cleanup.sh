#!/bin/bash

# RAM Test Cleanup Script
# Cleans up test environment

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== RAM Test Cleanup ===${NC}"

# Configuration
PG_BIN="/usr/local/pgsql.17/bin"
BASE_PORT=5433

# Stop PostgreSQL clusters
echo -e "${YELLOW}Stopping PostgreSQL clusters...${NC}"
for i in {1..3}; do
    PORT=$((BASE_PORT + i - 1))
    echo "Stopping node$i on port $PORT"
    
    if [ -d "node$i/data" ]; then
        $PG_BIN/pg_ctl -D "node$i/data" stop -m immediate 2>/dev/null || true
        echo -e "${GREEN}✓ Node$i stopped${NC}"
    fi
done

# Kill any remaining processes
echo -e "${YELLOW}Killing remaining processes...${NC}"
pkill -f "postgres.*node[123]" 2>/dev/null || true
pkill -f "ramd" 2>/dev/null || true

# Remove test directories
echo -e "${YELLOW}Removing test directories...${NC}"
for i in {1..3}; do
    if [ -d "node$i" ]; then
        rm -rf "node$i"
        echo -e "${GREEN}✓ Removed node$i${NC}"
    fi
done

# Remove config files
rm -f ramd.conf ramd.log

echo -e "${GREEN}=== Cleanup Complete ===${NC}"
