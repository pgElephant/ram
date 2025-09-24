"""
Unit tests for PGRaft extension
"""

import pytest
import os
import sys
from pathlib import Path

@pytest.mark.unit
@pytest.mark.pgraft
class TestPGRaftUnit:
    """PGRaft unit tests"""
    
    def test_extension_file_exists(self):
        """Test that PGRaft extension file exists"""
        extension_path = Path("pgraft/pgraft.dylib")
        assert extension_path.exists(), "PGRaft extension file not found"
        assert extension_path.stat().st_size > 0, "PGRaft extension file is empty"
    
    def test_go_library_exists(self):
        """Test that Go library exists"""
        go_lib_path = Path("pgraft/src/pgraft_go.dylib")
        assert go_lib_path.exists(), "Go library file not found"
        assert go_lib_path.stat().st_size > 0, "Go library file is empty"
    
    def test_source_files_exist(self):
        """Test that all source files exist"""
        source_files = [
            "pgraft/src/pgraft.c",
            "pgraft/src/raft.c",
            "pgraft/src/comm.c",
            "pgraft/src/monitor.c",
            "pgraft/src/guc.c",
            "pgraft/src/metrics.c",
            "pgraft/src/utils.c",
            "pgraft/src/worker_manager.c",
            "pgraft/src/health_worker.c"
        ]
        
        for source_file in source_files:
            assert Path(source_file).exists(), f"Source file {source_file} not found"
    
    def test_header_files_exist(self):
        """Test that all header files exist"""
        header_files = [
            "pgraft/include/pgraft.h",
            "pgraft/pgraft_go.h"
        ]
        
        for header_file in header_files:
            assert Path(header_file).exists(), f"Header file {header_file} not found"
    
    def test_sql_files_exist(self):
        """Test that SQL files exist"""
        sql_files = [
            "pgraft/pgraft--1.0.sql",
            "pgraft/pgraft.control"
        ]
        
        for sql_file in sql_files:
            assert Path(sql_file).exists(), f"SQL file {sql_file} not found"
    
    def test_makefile_exists(self):
        """Test that Makefile exists"""
        makefile_path = Path("pgraft/Makefile")
        assert makefile_path.exists(), "Makefile not found"
    
    def test_extension_size_reasonable(self):
        """Test that extension size is reasonable"""
        extension_path = Path("pgraft/pgraft.dylib")
        if extension_path.exists():
            size = extension_path.stat().st_size
            # Extension should be between 50KB and 2MB
            assert 50 * 1024 < size < 2 * 1024 * 1024, f"Extension size {size} bytes is unreasonable"
