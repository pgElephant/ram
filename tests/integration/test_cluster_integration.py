#!/usr/bin/env python3
"""
Cluster Integration Tests
Copyright (c) 2024-2025, pgElephant, Inc.
"""

import unittest
import requests
import psycopg2
import time
import subprocess
import json
import os

class ClusterIntegrationTests(unittest.TestCase):
    """Integration tests for cluster operations"""
    
    @classmethod
    def setUpClass(cls):
        """Set up test class"""
        cls.cluster_created = False
        cls.ramd_url = "http://localhost:8008"
        
    def setUp(self):
        """Set up each test"""
        if not self.cluster_created:
            self.create_cluster()
            self.cluster_created = True
    
    def create_cluster(self):
        """Create test cluster"""
        try:
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'create',
                '--num-nodes=3',
                '--config=conf/cluster.json'
            ], capture_output=True, text=True, timeout=120)
            
            if result.returncode != 0:
                self.fail(f"Failed to create cluster: {result.stderr}")
            
            # Wait for cluster to be ready
            time.sleep(30)
            
        except Exception as e:
            self.fail(f"Error creating cluster: {e}")
    
    def test_cluster_health(self):
        """Test cluster health endpoint"""
        try:
            response = requests.get(f"{self.ramd_url}/api/v1/cluster/health", timeout=10)
            self.assertEqual(response.status_code, 200)
            
            health_data = response.json()
            self.assertIn('status', health_data)
            self.assertIn('nodes', health_data)
            
        except Exception as e:
            self.fail(f"Health check failed: {e}")
    
    def test_cluster_status(self):
        """Test cluster status endpoint"""
        try:
            response = requests.get(f"{self.ramd_url}/api/v1/cluster/status", timeout=10)
            self.assertEqual(response.status_code, 200)
            
            status_data = response.json()
            self.assertIn('cluster_name', status_data)
            self.assertIn('nodes', status_data)
            
        except Exception as e:
            self.fail(f"Status check failed: {e}")
    
    def test_node_operations(self):
        """Test node start/stop operations"""
        try:
            # Stop primary node
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'stop',
                '--node=primary'
            ], capture_output=True, text=True, timeout=30)
            
            self.assertEqual(result.returncode, 0, f"Stop primary failed: {result.stderr}")
            
            # Wait for failover
            time.sleep(15)
            
            # Start primary node
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'start',
                '--node=primary'
            ], capture_output=True, text=True, timeout=30)
            
            self.assertEqual(result.returncode, 0, f"Start primary failed: {result.stderr}")
            
        except Exception as e:
            self.fail(f"Node operations failed: {e}")
    
    def test_replication_consistency(self):
        """Test replication consistency across nodes"""
        try:
            # Connect to primary and insert data
            conn = psycopg2.connect(
                host='localhost',
                port=5432,
                user='postgres',
                password='postgres',
                database='postgres'
            )
            
            cursor = conn.cursor()
            cursor.execute("CREATE TABLE IF NOT EXISTS test_replication (id SERIAL PRIMARY KEY, data TEXT)")
            cursor.execute("INSERT INTO test_replication (data) VALUES ('test_data_1')")
            cursor.execute("INSERT INTO test_replication (data) VALUES ('test_data_2')")
            conn.commit()
            conn.close()
            
            # Wait for replication
            time.sleep(10)
            
            # Check replica 1
            conn = psycopg2.connect(
                host='localhost',
                port=5433,
                user='postgres',
                password='postgres',
                database='postgres'
            )
            
            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM test_replication")
            count = cursor.fetchone()[0]
            conn.close()
            
            self.assertEqual(count, 2, "Replication inconsistency on replica 1")
            
            # Check replica 2
            conn = psycopg2.connect(
                host='localhost',
                port=5434,
                user='postgres',
                password='postgres',
                database='postgres'
            )
            
            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM test_replication")
            count = cursor.fetchone()[0]
            conn.close()
            
            self.assertEqual(count, 2, "Replication inconsistency on replica 2")
            
        except Exception as e:
            self.fail(f"Replication consistency test failed: {e}")
    
    def test_automatic_failover(self):
        """Test automatic failover when primary fails"""
        try:
            # Get initial leader
            response = requests.get(f"{self.ramd_url}/api/v1/cluster/status", timeout=10)
            initial_status = response.json()
            
            # Stop primary node
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'stop',
                '--node=primary'
            ], capture_output=True, text=True, timeout=30)
            
            self.assertEqual(result.returncode, 0, f"Stop primary failed: {result.stderr}")
            
            # Wait for failover
            time.sleep(20)
            
            # Check new leader
            response = requests.get(f"{self.ramd_url}/api/v1/cluster/status", timeout=10)
            new_status = response.json()
            
            # Verify leader changed
            self.assertNotEqual(
                initial_status.get('leader_id'),
                new_status.get('leader_id'),
                "Leader did not change after primary failure"
            )
            
        except Exception as e:
            self.fail(f"Automatic failover test failed: {e}")
    
    def test_metrics_endpoint(self):
        """Test metrics endpoint"""
        try:
            response = requests.get(f"{self.ramd_url}/metrics", timeout=10)
            self.assertEqual(response.status_code, 200)
            
            # Check for Prometheus format
            self.assertIn('ramd_', response.text)
            
        except Exception as e:
            self.fail(f"Metrics endpoint test failed: {e}")
    
    @classmethod
    def tearDownClass(cls):
        """Clean up test class"""
        try:
            # Stop cluster
            subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'destroy'
            ], capture_output=True, timeout=60)
        except:
            pass

if __name__ == '__main__':
    unittest.main()
