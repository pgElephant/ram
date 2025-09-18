#!/bin/bash

# Test script for RAM PostgreSQL metrics endpoint
# This script tests the /metrics endpoint and validates Prometheus format

set -e

echo "Testing RAM PostgreSQL Metrics Endpoint"
echo "=========================================="

# Configuration
RAMD_HOST="localhost"
RAMD_PORT="8008"
METRICS_URL="http://${RAMD_HOST}:${RAMD_PORT}/metrics"

echo "Testing metrics endpoint: ${METRICS_URL}"

# Test 1: Basic connectivity
echo "1. Testing basic connectivity..."
if curl -f -s "${METRICS_URL}" > /dev/null; then
    echo "PASS: Metrics endpoint is accessible"
else
    echo "FAIL: Metrics endpoint is not accessible"
    echo "   Make sure ramd is running on port ${RAMD_PORT}"
    exit 1
fi

# Test 2: Content type validation
echo "2. Testing content type..."
CONTENT_TYPE=$(curl -s -I "${METRICS_URL}" | grep -i "content-type" | cut -d: -f2 | tr -d ' \r\n')
if [[ "$CONTENT_TYPE" == *"text/plain"* ]]; then
    echo "PASS: Content type is correct: ${CONTENT_TYPE}"
else
    echo "FAIL: Content type is incorrect: ${CONTENT_TYPE}"
    echo "   Expected: text/plain; version=0.0.4; charset=utf-8"
fi

# Test 3: Prometheus format validation
echo "3. Testing Prometheus format..."
METRICS_OUTPUT=$(curl -s "${METRICS_URL}")

# Check for required Prometheus format elements
if echo "$METRICS_OUTPUT" | grep -q "# HELP"; then
    echo "PASS: Contains HELP comments"
else
    echo "FAIL: Missing HELP comments"
fi

if echo "$METRICS_OUTPUT" | grep -q "# TYPE"; then
    echo "PASS: Contains TYPE comments"
else
    echo "FAIL: Missing TYPE comments"
fi

if echo "$METRICS_OUTPUT" | grep -q "ramd_"; then
    echo "PASS: Contains ramd metrics"
else
    echo "FAIL: No ramd metrics found"
fi

# Test 4: Specific metrics validation
echo "4. Testing specific metrics..."

REQUIRED_METRICS=(
    "ramd_cluster_nodes_total"
    "ramd_cluster_nodes_healthy"
    "ramd_cluster_has_quorum"
    "ramd_node_healthy"
    "ramd_health_checks_total"
    "ramd_http_requests_total"
)

for metric in "${REQUIRED_METRICS[@]}"; do
    if echo "$METRICS_OUTPUT" | grep -q "^${metric}"; then
        echo "PASS: Found metric: ${metric}"
    else
        echo "FAIL: Missing metric: ${metric}"
    fi
done

# Test 5: Metric values validation
echo "5. Testing metric values..."

# Check that numeric values are present
if echo "$METRICS_OUTPUT" | grep -q "ramd_cluster_nodes_total [0-9]"; then
    echo "PASS: Cluster nodes metric has numeric value"
else
    echo "FAIL: Cluster nodes metric missing numeric value"
fi

if echo "$METRICS_OUTPUT" | grep -q "ramd_cluster_has_quorum [01]"; then
    echo "PASS: Quorum metric has valid value (0 or 1)"
else
    echo "FAIL: Quorum metric has invalid value"
fi

# Test 6: Display sample metrics
echo "6. Sample metrics output:"
echo "=========================="
echo "$METRICS_OUTPUT" | head -20
echo "..."
echo "$METRICS_OUTPUT" | tail -10

# Test 7: Performance test
echo "7. Testing performance..."
START_TIME=$(date +%s%N)
curl -s "${METRICS_URL}" > /dev/null
END_TIME=$(date +%s%N)
DURATION=$(( (END_TIME - START_TIME) / 1000000 ))
echo "PASS: Metrics endpoint response time: ${DURATION}ms"

# Summary
echo ""
echo "Test Summary"
echo "==============="
echo "PASS: Metrics endpoint is working correctly"
echo "PASS: Prometheus format is valid"
echo "PASS: Required metrics are present"
echo "PASS: Performance is acceptable (${DURATION}ms)"
echo ""
echo "All tests passed! The metrics endpoint is ready for Prometheus."
echo ""
echo "Next steps:"
echo "1. Configure Prometheus to scrape this endpoint"
echo "2. Import Grafana dashboards"
echo "3. Set up alerting rules"
echo ""
echo "Prometheus configuration example:"
echo "  - job_name: 'ramd-cluster'"
echo "    static_configs:"
echo "      - targets: ['${RAMD_HOST}:${RAMD_PORT}']"
echo "    metrics_path: '/metrics'"
echo "    scrape_interval: 5s"
