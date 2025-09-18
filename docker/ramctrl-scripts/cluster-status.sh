#!/bin/bash
# Cluster status script for ramctrl container

echo "=== RAM Cluster Status ==="
echo "Timestamp: $(date)"
echo

echo "=== Node Status ==="
ramctrl status --verbose
echo

echo "=== Cluster Health ==="
ramctrl health
echo

echo "=== Connected Nodes ==="
ramctrl list-nodes
echo

echo "=== Replication Status ==="
ramctrl replication-status
echo

echo "=== Metrics ==="
ramctrl metrics
