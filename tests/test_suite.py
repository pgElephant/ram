#!/usr/bin/env python3
"""
RAM PostgreSQL High Availability System - Test Suite
Copyright (c) 2024-2025, pgElephant, Inc.

This is a comprehensive Python-based TAP (Test Anything Protocol) test suite
for the RAM system components: PGRaft, RAMD, and RAMCTRL.
"""

import os
import sys
import json
import time
import subprocess
import requests
import psycopg2
import signal
import threading
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from pathlib import Path

# Test configuration
TEST_CONFIG = {
    'postgresql': {
        'host': 'localhost',
        'ports': [5432, 5433, 5434],
        'user': 'postgres',
        'password': 'postgres',
        'database': 'postgres'
    },
    'ramd': {
        'host': 'localhost',
        'ports': [8008, 8009, 8010]
    },
    'pgraft': {
        'ports': [7400, 7401, 7402]
    },
    'timeouts': {
        'connection': 30,
        'operation': 60,
        'failover': 120
    }
}

@dataclass
class TestResult:
    """Test result container"""
    test_name: str
    passed: bool
    message: str
    duration: float
    details: Optional[Dict] = None

class TAPTestSuite:
    """TAP Test Suite for RAM System"""
    
    def __init__(self):
        self.test_count = 0
        self.passed_count = 0
        self.failed_count = 0
        self.results: List[TestResult] = []
        self.start_time = time.time()
        
    def plan(self, total_tests: int):
        """Output TAP plan"""
        print(f"1..{total_tests}")
        
    def ok(self, test_name: str, passed: bool, message: str = "", details: Dict = None):
        """Output TAP test result"""
        self.test_count += 1
        duration = time.time() - self.start_time
        
        result = TestResult(
            test_name=test_name,
            passed=passed,
            message=message,
            duration=duration,
            details=details
        )
        self.results.append(result)
        
        if passed:
            self.passed_count += 1
            status = "ok"
        else:
            self.failed_count += 1
            status = "not ok"
            
        output = f"{status} {self.test_count} - {test_name}"
        if message:
            output += f" # {message}"
        print(output)
        
        if details:
            print(f"  ---")
            for key, value in details.items():
                print(f"  {key}: {value}")
            print(f"  ...")
            
    def diag(self, message: str):
        """Output TAP diagnostic message"""
        print(f"# {message}")
        
    def bail_out(self, reason: str):
        """Bail out of test suite"""
        print(f"Bail out! {reason}")
        sys.exit(1)

class RAMTestSuite(TAPTestSuite):
    """RAM System Test Suite"""
    
    def __init__(self):
        super().__init__()
        self.cluster_nodes = []
        self.ramd_processes = []
        
    def setup(self):
        """Setup test environment"""
        self.diag("Setting up test environment...")
        
        # Check if components are built
        if not self._check_components():
            self.bail_out("Required components not found. Run 'make all' first.")
            
        # Clean up any existing processes
        self._cleanup_processes()
        
        # Create test directories
        self._create_test_directories()
        
    def teardown(self):
        """Cleanup test environment"""
        self.diag("Cleaning up test environment...")
        self._cleanup_processes()
        self._cleanup_directories()
        
    def _check_components(self) -> bool:
        """Check if all components are built"""
        components = [
            'pgraft/pgraft.so',
            'ramd/ramd',
            'ramctrl/ramctrl'
        ]
        
        for component in components:
            if not os.path.exists(component):
                self.diag(f"Missing component: {component}")
                return False
        return True
        
    def _cleanup_processes(self):
        """Clean up any running processes"""
        try:
            # Kill any existing PostgreSQL processes
            subprocess.run(['pkill', '-f', 'postgres'], capture_output=True)
            time.sleep(2)
            
            # Kill any existing RAMD processes
            subprocess.run(['pkill', '-f', 'ramd'], capture_output=True)
            time.sleep(1)
            
        except Exception as e:
            self.diag(f"Warning: Error cleaning up processes: {e}")
            
    def _create_test_directories(self):
        """Create test directories"""
        dirs = [
            '/tmp/postgres_data_1',
            '/tmp/postgres_data_2', 
            '/tmp/postgres_data_3',
            '/tmp/postgres_logs_1',
            '/tmp/postgres_logs_2',
            '/tmp/postgres_logs_3',
            '/tmp/pgraft_data'
        ]
        
        for dir_path in dirs:
            os.makedirs(dir_path, exist_ok=True)
            
    def _cleanup_directories(self):
        """Clean up test directories"""
        dirs = [
            '/tmp/postgres_data_1',
            '/tmp/postgres_data_2',
            '/tmp/postgres_data_3',
            '/tmp/postgres_logs_1',
            '/tmp/postgres_logs_2',
            '/tmp/postgres_logs_3',
            '/tmp/pgraft_data'
        ]
        
        for dir_path in dirs:
            try:
                subprocess.run(['rm', '-rf', dir_path], capture_output=True)
            except Exception as e:
                self.diag(f"Warning: Error cleaning up {dir_path}: {e}")

class PGRaftTests(RAMTestSuite):
    """PGRaft Extension Tests"""
    
    def test_pgraft_extension_loading(self):
        """Test PGRaft extension loads correctly"""
        try:
            # Start PostgreSQL with PGRaft
            self._start_postgresql(5432)
            time.sleep(5)
            
            # Connect and check extension
            conn = psycopg2.connect(
                host=TEST_CONFIG['postgresql']['host'],
                port=5432,
                user=TEST_CONFIG['postgresql']['user'],
                password=TEST_CONFIG['postgresql']['password'],
                database=TEST_CONFIG['postgresql']['database']
            )
            
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM pg_extension WHERE extname = 'pgraft'")
            result = cursor.fetchone()
            
            conn.close()
            
            if result:
                self.ok("pgraft_extension_loading", True, "PGRaft extension loaded successfully")
            else:
                self.ok("pgraft_extension_loading", False, "PGRaft extension not found")
                
        except Exception as e:
            self.ok("pgraft_extension_loading", False, f"Error: {str(e)}")
            
    def test_pgraft_functions(self):
        """Test PGRaft SQL functions"""
        try:
            conn = psycopg2.connect(
                host=TEST_CONFIG['postgresql']['host'],
                port=5432,
                user=TEST_CONFIG['postgresql']['user'],
                password=TEST_CONFIG['postgresql']['password'],
                database=TEST_CONFIG['postgresql']['database']
            )
            
            cursor = conn.cursor()
            
            # Test pgraft_init
            cursor.execute("SELECT pgraft_init()")
            result = cursor.fetchone()
            self.ok("pgraft_init", result[0] if result else False, "PGRaft initialization")
            
            # Test pgraft_status
            cursor.execute("SELECT pgraft_status()")
            result = cursor.fetchone()
            self.ok("pgraft_status", result is not None, "PGRaft status check")
            
            # Test pgraft_is_healthy
            cursor.execute("SELECT pgraft_is_healthy()")
            result = cursor.fetchone()
            self.ok("pgraft_is_healthy", result[0] if result else False, "PGRaft health check")
            
            conn.close()
            
        except Exception as e:
            self.ok("pgraft_functions", False, f"Error: {str(e)}")
            
    def _start_postgresql(self, port: int):
        """Start PostgreSQL instance"""
        data_dir = f"/tmp/postgres_data_{port - 5431}"
        log_dir = f"/tmp/postgres_logs_{port - 5431}"
        
        # Initialize database
        subprocess.run([
            '/usr/local/pgsql.17/bin/initdb',
            '-D', data_dir,
            '--auth-local=trust',
            '--auth-host=md5'
        ], capture_output=True)
        
        # Configure postgresql.conf
        conf_file = f"{data_dir}/postgresql.conf"
        with open(conf_file, 'a') as f:
            f.write(f"""
port = {port}
shared_preload_libraries = 'pgraft'
wal_level = replica
max_wal_senders = 10
max_replication_slots = 10
hot_standby = on
""")
        
        # Start PostgreSQL
        subprocess.Popen([
            '/usr/local/pgsql.17/bin/postgres',
            '-D', data_dir,
            '-l', f"{log_dir}/postgresql.log"
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

class RAMDTests(RAMTestSuite):
    """RAMD Daemon Tests"""
    
    def test_ramd_startup(self):
        """Test RAMD daemon starts correctly"""
        try:
            # Start RAMD
            process = subprocess.Popen([
                './ramd/ramd',
                '--config=conf/ramd.conf',
                '--daemonize=false'
            ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            
            time.sleep(5)
            
            # Check if process is running
            if process.poll() is None:
                self.ok("ramd_startup", True, "RAMD daemon started successfully")
                self.ramd_processes.append(process)
            else:
                self.ok("ramd_startup", False, "RAMD daemon failed to start")
                
        except Exception as e:
            self.ok("ramd_startup", False, f"Error: {str(e)}")
            
    def test_ramd_http_api(self):
        """Test RAMD HTTP API"""
        try:
            # Wait for RAMD to start
            time.sleep(2)
            
            # Test health endpoint
            response = requests.get('http://localhost:8008/api/v1/health', timeout=10)
            if response.status_code == 200:
                self.ok("ramd_http_health", True, "RAMD health endpoint responding")
            else:
                self.ok("ramd_http_health", False, f"Health endpoint returned {response.status_code}")
                
            # Test cluster status endpoint
            response = requests.get('http://localhost:8008/api/v1/cluster/status', timeout=10)
            if response.status_code == 200:
                self.ok("ramd_http_cluster_status", True, "RAMD cluster status endpoint responding")
            else:
                self.ok("ramd_http_cluster_status", False, f"Cluster status endpoint returned {response.status_code}")
                
        except Exception as e:
            self.ok("ramd_http_api", False, f"Error: {str(e)}")
            
    def test_ramd_metrics(self):
        """Test RAMD metrics endpoint"""
        try:
            response = requests.get('http://localhost:8008/metrics', timeout=10)
            if response.status_code == 200:
                self.ok("ramd_metrics", True, "RAMD metrics endpoint responding")
            else:
                self.ok("ramd_metrics", False, f"Metrics endpoint returned {response.status_code}")
                
        except Exception as e:
            self.ok("ramd_metrics", False, f"Error: {str(e)}")

class RAMCTRLTests(RAMTestSuite):
    """RAMCTRL CLI Tests"""
    
    def test_ramctrl_cluster_create(self):
        """Test RAMCTRL cluster creation"""
        try:
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'create',
                '--num-nodes=3',
                '--config=conf/cluster.json'
            ], capture_output=True, text=True, timeout=60)
            
            if result.returncode == 0:
                self.ok("ramctrl_cluster_create", True, "Cluster created successfully")
            else:
                self.ok("ramctrl_cluster_create", False, f"Cluster creation failed: {result.stderr}")
                
        except Exception as e:
            self.ok("ramctrl_cluster_create", False, f"Error: {str(e)}")
            
    def test_ramctrl_cluster_status(self):
        """Test RAMCTRL cluster status"""
        try:
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'status'
            ], capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                self.ok("ramctrl_cluster_status", True, "Cluster status retrieved successfully")
            else:
                self.ok("ramctrl_cluster_status", False, f"Cluster status failed: {result.stderr}")
                
        except Exception as e:
            self.ok("ramctrl_cluster_status", False, f"Error: {str(e)}")
            
    def test_ramctrl_cluster_stop_start(self):
        """Test RAMCTRL cluster stop/start operations"""
        try:
            # Stop primary node
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'stop',
                '--node=primary'
            ], capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                self.ok("ramctrl_cluster_stop", True, "Primary node stopped successfully")
            else:
                self.ok("ramctrl_cluster_stop", False, f"Stop operation failed: {result.stderr}")
                
            time.sleep(5)
            
            # Start primary node
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'start',
                '--node=primary'
            ], capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                self.ok("ramctrl_cluster_start", True, "Primary node started successfully")
            else:
                self.ok("ramctrl_cluster_start", False, f"Start operation failed: {result.stderr}")
                
        except Exception as e:
            self.ok("ramctrl_cluster_stop_start", False, f"Error: {str(e)}")

class IntegrationTests(RAMTestSuite):
    """Integration Tests"""
    
    def test_cluster_failover(self):
        """Test automatic cluster failover"""
        try:
            # Start cluster
            subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'create',
                '--num-nodes=3'
            ], capture_output=True, timeout=60)
            
            time.sleep(10)
            
            # Stop primary node
            subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'stop',
                '--node=primary'
            ], capture_output=True, timeout=30)
            
            time.sleep(15)
            
            # Check if failover occurred
            result = subprocess.run([
                './ramctrl/ramctrl',
                'cluster', 'status'
            ], capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0 and "primary" in result.stdout.lower():
                self.ok("cluster_failover", True, "Automatic failover occurred")
            else:
                self.ok("cluster_failover", False, "Automatic failover did not occur")
                
        except Exception as e:
            self.ok("cluster_failover", False, f"Error: {str(e)}")
            
    def test_replication_consistency(self):
        """Test replication consistency"""
        try:
            # Connect to primary and insert data
            conn = psycopg2.connect(
                host=TEST_CONFIG['postgresql']['host'],
                port=5432,
                user=TEST_CONFIG['postgresql']['user'],
                password=TEST_CONFIG['postgresql']['password'],
                database=TEST_CONFIG['postgresql']['database']
            )
            
            cursor = conn.cursor()
            cursor.execute("CREATE TABLE IF NOT EXISTS test_replication (id SERIAL PRIMARY KEY, data TEXT)")
            cursor.execute("INSERT INTO test_replication (data) VALUES ('test_data')")
            conn.commit()
            conn.close()
            
            time.sleep(5)
            
            # Check replica
            conn = psycopg2.connect(
                host=TEST_CONFIG['postgresql']['host'],
                port=5433,
                user=TEST_CONFIG['postgresql']['user'],
                password=TEST_CONFIG['postgresql']['password'],
                database=TEST_CONFIG['postgresql']['database']
            )
            
            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM test_replication")
            count = cursor.fetchone()[0]
            conn.close()
            
            if count == 1:
                self.ok("replication_consistency", True, "Replication consistency maintained")
            else:
                self.ok("replication_consistency", False, f"Replication inconsistency: expected 1, got {count}")
                
        except Exception as e:
            self.ok("replication_consistency", False, f"Error: {str(e)}")

def main():
    """Main test runner"""
    print("TAP version 13")
    
    # Create test suite
    test_suite = RAMTestSuite()
    
    # Calculate total tests
    total_tests = 20  # Adjust based on actual test count
    
    # Output plan
    test_suite.plan(total_tests)
    
    try:
        # Setup
        test_suite.setup()
        
        # Run tests
        pgraft_tests = PGRaftTests()
        ramd_tests = RAMDTests()
        ramctrl_tests = RAMCTRLTests()
        integration_tests = IntegrationTests()
        
        # PGRaft tests
        pgraft_tests.test_pgraft_extension_loading()
        pgraft_tests.test_pgraft_functions()
        
        # RAMD tests
        ramd_tests.test_ramd_startup()
        ramd_tests.test_ramd_http_api()
        ramd_tests.test_ramd_metrics()
        
        # RAMCTRL tests
        ramctrl_tests.test_ramctrl_cluster_create()
        ramctrl_tests.test_ramctrl_cluster_status()
        ramctrl_tests.test_ramctrl_cluster_stop_start()
        
        # Integration tests
        integration_tests.test_cluster_failover()
        integration_tests.test_replication_consistency()
        
    finally:
        # Teardown
        test_suite.teardown()
        
    # Summary
    print(f"# Tests run: {test_suite.test_count}")
    print(f"# Passed: {test_suite.passed_count}")
    print(f"# Failed: {test_suite.failed_count}")
    
    if test_suite.failed_count > 0:
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    main()
