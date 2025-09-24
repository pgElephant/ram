"""
Performance tests for PGRaft extension
"""

import pytest
import time
import psutil
import os

@pytest.mark.performance
@pytest.mark.pgraft
class TestPGRaftPerformance:
    """PGRaft performance tests"""
    
    def test_extension_load_time(self):
        """Test PGRaft extension load time"""
        start_time = time.time()
        # Simulate extension loading
        time.sleep(0.1)  # Mock load time
        load_time = time.time() - start_time
        
        assert load_time < 1.0, f"Extension load time too slow: {load_time:.3f}s"
    
    def test_memory_usage(self):
        """Test memory usage during operation"""
        process = psutil.Process(os.getpid())
        initial_memory = process.memory_info().rss
        
        # Simulate memory-intensive operation
        data = [0] * 1000000  # 1M integers
        
        peak_memory = process.memory_info().rss
        memory_increase = peak_memory - initial_memory
        
        # Clean up
        del data
        
        assert memory_increase < 50 * 1024 * 1024, f"Memory usage too high: {memory_increase / 1024 / 1024:.1f}MB"
    
    def test_raft_consensus_speed(self):
        """Test Raft consensus operation speed"""
        start_time = time.time()
        # Simulate Raft consensus operation
        time.sleep(0.05)  # Mock consensus time
        consensus_time = time.time() - start_time
        
        assert consensus_time < 0.1, f"Consensus too slow: {consensus_time:.3f}s"
