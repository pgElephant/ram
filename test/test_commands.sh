#!/bin/bash

# RAM Manual Testing Commands
# Collection of test commands for RAM functionality

set -e

# Configuration
PG_BIN="/usr/local/pgsql.17/bin"
BASE_PORT=5433
RAM_BASE_PORT=8080

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== RAM Manual Testing Commands ===${NC}"

# Function to run SQL command on all nodes
run_sql_all() {
    local sql="$1"
    echo -e "${YELLOW}Running SQL on all nodes: $sql${NC}"
    
    for i in {1..3}; do
        PORT=$((BASE_PORT + i - 1))
        echo -e "${BLUE}Node$i (port $PORT):${NC}"
        $PG_BIN/psql -p $PORT -h localhost -d postgres -c "$sql"
        echo ""
    done
}

# Function to check cluster status
check_cluster_status() {
    echo -e "${YELLOW}=== Cluster Status Check ===${NC}"
    
    # Check PostgreSQL status
    echo -e "${BLUE}PostgreSQL Status:${NC}"
    for i in {1..3}; do
        PORT=$((BASE_PORT + i - 1))
        if $PG_BIN/pg_isready -p $PORT -h localhost; then
            echo -e "${GREEN}✓ Node$i (port $PORT): Ready${NC}"
        else
            echo -e "${RED}✗ Node$i (port $PORT): Not ready${NC}"
        fi
    done
    
    # Check pgraft status
    echo -e "${BLUE}pgraft Extension Status:${NC}"
    for i in {1..3}; do
        PORT=$((BASE_PORT + i - 1))
        echo -e "${BLUE}Node$i:${NC}"
        $PG_BIN/psql -p $PORT -h localhost -d postgres -c "SELECT * FROM pg_extension WHERE extname = 'pgraft';"
    done
    
    # Check RAM API status (if ramd is running)
    echo -e "${BLUE}RAM API Status:${NC}"
    for i in {1..3}; do
        HTTP_PORT=$((RAM_BASE_PORT + i - 1))
        if curl -s http://localhost:$HTTP_PORT/health >/dev/null 2>&1; then
            echo -e "${GREEN}✓ RAM API Node$i (port $HTTP_PORT): Ready${NC}"
        else
            echo -e "${RED}✗ RAM API Node$i (port $HTTP_PORT): Not ready${NC}"
        fi
    done
}

# Function to test basic functionality
test_basic_functionality() {
    echo -e "${YELLOW}=== Basic Functionality Test ===${NC}"
    
    # Create test table
    echo -e "${BLUE}Creating test table...${NC}"
    run_sql_all "CREATE TABLE IF NOT EXISTS test_table (id SERIAL PRIMARY KEY, data TEXT, created_at TIMESTAMP DEFAULT NOW());"
    
    # Insert test data
    echo -e "${BLUE}Inserting test data...${NC}"
    run_sql_all "INSERT INTO test_table (data) VALUES ('test_data_1'), ('test_data_2'), ('test_data_3');"
    
    # Query test data
    echo -e "${BLUE}Querying test data...${NC}"
    run_sql_all "SELECT * FROM test_table ORDER BY id;"
    
    # Check replication status
    echo -e "${BLUE}Checking replication status...${NC}"
    for i in {1..3}; do
        PORT=$((BASE_PORT + i - 1))
        echo -e "${BLUE}Node$i replication status:${NC}"
        $PG_BIN/psql -p $PORT -h localhost -d postgres -c "SELECT * FROM pg_stat_replication;"
        echo ""
    done
}

# Function to test failover
test_failover() {
    echo -e "${YELLOW}=== Failover Test ===${NC}"
    
    # Get current primary
    echo -e "${BLUE}Current cluster state:${NC}"
    check_cluster_status
    
    # Simulate primary failure
    echo -e "${BLUE}Simulating primary failure...${NC}"
    echo "Press Enter to continue with failover test..."
    read
    
    # Check which node is primary
    echo -e "${BLUE}Checking primary node...${NC}"
    for i in {1..3}; do
        PORT=$((BASE_PORT + i - 1))
        echo -e "${BLUE}Node$i:${NC}"
        $PG_BIN/psql -p $PORT -h localhost -d postgres -c "SELECT pg_is_in_recovery();"
    done
    
    # Test write operations
    echo -e "${BLUE}Testing write operations...${NC}"
    for i in {1..3}; do
        PORT=$((BASE_PORT + i - 1))
        echo -e "${BLUE}Testing writes on Node$i:${NC}"
        $PG_BIN/psql -p $PORT -h localhost -d postgres -c "INSERT INTO test_table (data) VALUES ('failover_test_$(date +%s)');" 2>/dev/null || echo "Write failed (expected for standby)"
        echo ""
    done
}

# Function to test RAM API
test_ram_api() {
    echo -e "${YELLOW}=== RAM API Test ===${NC}"
    
    for i in {1..3}; do
        HTTP_PORT=$((RAM_BASE_PORT + i - 1))
        echo -e "${BLUE}Testing RAM API on Node$i (port $HTTP_PORT):${NC}"
        
        # Health check
        echo "Health check:"
        curl -s http://localhost:$HTTP_PORT/health | jq . 2>/dev/null || echo "Health check failed"
        
        # Cluster status
        echo "Cluster status:"
        curl -s http://localhost:$HTTP_PORT/api/v1/cluster/status | jq . 2>/dev/null || echo "Cluster status failed"
        
        # Node info
        echo "Node info:"
        curl -s http://localhost:$HTTP_PORT/api/v1/node/info | jq . 2>/dev/null || echo "Node info failed"
        
        echo ""
    done
}

# Function to show monitoring data
show_monitoring() {
    echo -e "${YELLOW}=== Monitoring Data ===${NC}"
    
    # PostgreSQL statistics
    echo -e "${BLUE}PostgreSQL Statistics:${NC}"
    for i in {1..3}; do
        PORT=$((BASE_PORT + i - 1))
        echo -e "${BLUE}Node$i:${NC}"
        $PG_BIN/psql -p $PORT -h localhost -d postgres -c "SELECT datname, numbackends, xact_commit, xact_rollback, blks_read, blks_hit FROM pg_stat_database WHERE datname = 'postgres';"
        echo ""
    done
    
    # RAM metrics (if available)
    echo -e "${BLUE}RAM Metrics:${NC}"
    for i in {1..3}; do
        HTTP_PORT=$((RAM_BASE_PORT + i - 1))
        echo -e "${BLUE}Node$i metrics:${NC}"
        curl -s http://localhost:$HTTP_PORT/metrics 2>/dev/null | grep -E "(ram_|pgraft_)" | head -10 || echo "Metrics not available"
        echo ""
    done
}

# Main menu
show_menu() {
    echo -e "${BLUE}=== RAM Testing Menu ===${NC}"
    echo "1. Check cluster status"
    echo "2. Test basic functionality"
    echo "3. Test failover"
    echo "4. Test RAM API"
    echo "5. Show monitoring data"
    echo "6. Run all tests"
    echo "7. Exit"
    echo ""
    echo -n "Choose an option (1-7): "
}

# Main execution
if [ "$1" = "all" ]; then
    check_cluster_status
    test_basic_functionality
    test_ram_api
    show_monitoring
elif [ "$1" = "status" ]; then
    check_cluster_status
elif [ "$1" = "basic" ]; then
    test_basic_functionality
elif [ "$1" = "failover" ]; then
    test_failover
elif [ "$1" = "api" ]; then
    test_ram_api
elif [ "$1" = "monitor" ]; then
    show_monitoring
else
    while true; do
        show_menu
        read choice
        case $choice in
            1) check_cluster_status ;;
            2) test_basic_functionality ;;
            3) test_failover ;;
            4) test_ram_api ;;
            5) show_monitoring ;;
            6) 
                check_cluster_status
                test_basic_functionality
                test_ram_api
                show_monitoring
                ;;
            7) echo "Exiting..."; exit 0 ;;
            *) echo "Invalid option. Please try again." ;;
        esac
        echo ""
        echo "Press Enter to continue..."
        read
    done
fi
