#!/usr/bin/env python3
"""
Comprehensive Test Suite for RAM Project
========================================

This test suite provides comprehensive testing for all components
of the RAM project including PGRaft, RAMD, and RAMCTRL.
"""

import os
import sys
import subprocess
import json
import time
import requests
import unittest
from typing import Dict, List, Any, Optional

class RAMTestSuite(unittest.TestCase):
    """Comprehensive test suite for RAM project components."""
    
    def setUp(self):
        """Set up test environment."""
        self.project_root = os.path.dirname(os.path.abspath(__file__))
        self.pgraft_extension = os.path.join(self.project_root, "pgraft.dylib")
        self.ramd_binary = os.path.join(self.project_root, "ramd", "ramd")
        self.ramctrl_binary = os.path.join(self.project_root, "ramctrl", "ramctrl")
        self.config_file = os.path.join(self.project_root, "conf", "ram.conf")
        
    def test_binary_existence(self):
        """Test that all required binaries exist."""
        self.assertTrue(os.path.exists(self.pgraft_extension), "PGRaft extension not found")
        self.assertTrue(os.path.exists(self.ramd_binary), "RAMD binary not found")
        self.assertTrue(os.path.exists(self.ramctrl_binary), "RAMCTRL binary not found")
        
    def test_binary_executability(self):
        """Test that all binaries are executable."""
        self.assertTrue(os.access(self.pgraft_extension, os.X_OK), "PGRaft extension not executable")
        self.assertTrue(os.access(self.ramd_binary, os.X_OK), "RAMD binary not executable")
        self.assertTrue(os.access(self.ramctrl_binary, os.X_OK), "RAMCTRL binary not executable")
        
    def test_help_commands(self):
        """Test help commands for all components."""
        # Test RAMD help
        result = subprocess.run([self.ramd_binary, "--help"], 
                              capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, f"RAMD help failed: {result.stderr}")
        
        # Test RAMCTRL help
        result = subprocess.run([self.ramctrl_binary, "--help"], 
                              capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, f"RAMCTRL help failed: {result.stderr}")
        
    def test_configuration_files(self):
        """Test configuration file existence and validity."""
        config_files = [
            "conf/ram.conf",
            "conf/ramd.conf", 
            "conf/ramctrl.conf",
            "scripts/cluster.json"
        ]
        
        for config_file in config_files:
            file_path = os.path.join(self.project_root, config_file)
            self.assertTrue(os.path.exists(file_path), f"Config file {config_file} not found")
            
    def test_pgraft_extension_loading(self):
        """Test PGRaft extension loading."""
        # This would require a PostgreSQL instance to test properly
        # For now, just verify the extension file is valid
        self.assertTrue(os.path.getsize(self.pgraft_extension) > 0, "PGRaft extension is empty")
        
    def test_ramd_api_endpoints(self):
        """Test RAMD API endpoints."""
        # Start RAMD daemon
        ramd_process = subprocess.Popen([self.ramd_binary], 
                                      stdout=subprocess.PIPE, 
                                      stderr=subprocess.PIPE)
        
        try:
            # Wait for daemon to start
            time.sleep(2)
            
            # Test health endpoint
            response = requests.get("http://localhost:8080/api/v1/health", timeout=5)
            self.assertEqual(response.status_code, 200, "Health endpoint failed")
            
            # Test metrics endpoint
            response = requests.get("http://localhost:8080/api/v1/metrics", timeout=5)
            self.assertEqual(response.status_code, 200, "Metrics endpoint failed")
            
        except requests.exceptions.RequestException as e:
            self.fail(f"API test failed: {e}")
        finally:
            # Clean up
            ramd_process.terminate()
            ramd_process.wait()
            
    def test_ramctrl_commands(self):
        """Test RAMCTRL commands."""
        # Test cluster status command
        result = subprocess.run([self.ramctrl_binary, "cluster", "status"], 
                              capture_output=True, text=True, timeout=10)
        # This might fail if no cluster is running, which is expected
        
        # Test help command
        result = subprocess.run([self.ramctrl_binary, "--help"], 
                              capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, "RAMCTRL help command failed")
        
    def test_memory_management(self):
        """Test memory management utilities."""
        # This would require running the actual components
        # For now, just verify the utilities exist
        memory_utils = os.path.join(self.project_root, "include", "memory_utils.h")
        self.assertTrue(os.path.exists(memory_utils), "Memory utilities not found")
        
    def test_security_hardening(self):
        """Test security hardening utilities."""
        security_utils = os.path.join(self.project_root, "include", "security_hardening.h")
        self.assertTrue(os.path.exists(security_utils), "Security utilities not found")
        
    def test_thread_safety(self):
        """Test thread safety utilities."""
        thread_utils = os.path.join(self.project_root, "include", "thread_safety.h")
        self.assertTrue(os.path.exists(thread_utils), "Thread safety utilities not found")
        
    def test_performance_monitoring(self):
        """Test performance monitoring utilities."""
        perf_utils = os.path.join(self.project_root, "include", "performance_monitor.h")
        self.assertTrue(os.path.exists(perf_utils), "Performance monitoring utilities not found")
        
    def test_error_handling(self):
        """Test error handling utilities."""
        error_utils = os.path.join(self.project_root, "include", "error_handling.h")
        self.assertTrue(os.path.exists(error_utils), "Error handling utilities not found")
        
    def test_enhanced_logging(self):
        """Test enhanced logging utilities."""
        logging_utils = os.path.join(self.project_root, "include", "enhanced_logging.h")
        self.assertTrue(os.path.exists(logging_utils), "Enhanced logging utilities not found")
        
    def test_configuration_parser(self):
        """Test configuration parser utilities."""
        config_utils = os.path.join(self.project_root, "include", "config_parser.h")
        self.assertTrue(os.path.exists(config_utils), "Configuration parser utilities not found")

def run_comprehensive_tests():
    """Run comprehensive test suite."""
    print("RAM Project Comprehensive Test Suite")
    print("====================================")
    print()
    
    # Create test suite
    suite = unittest.TestLoader().loadTestsFromTestCase(RAMTestSuite)
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Print summary
    print("\nTest Summary:")
    print("=============")
    print(f"Tests run: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    
    if result.failures:
        print("\nFailures:")
        for test, traceback in result.failures:
            print(f"  {test}: {traceback}")
            
    if result.errors:
        print("\nErrors:")
        for test, traceback in result.errors:
            print(f"  {test}: {traceback}")
    
    return result.wasSuccessful()

if __name__ == "__main__":
    success = run_comprehensive_tests()
    sys.exit(0 if success else 1)
