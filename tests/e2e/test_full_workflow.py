"""
End-to-end tests for complete RAM workflow
"""

import pytest
import subprocess
import time
import os
from pathlib import Path

@pytest.mark.e2e
@pytest.mark.slow
class TestFullWorkflow:
    """Complete workflow tests"""
    
    def test_build_all_components(self):
        """Test building all components"""
        # Test that all components can be built
        result = subprocess.run(["make", "clean"], capture_output=True, text=True)
        assert result.returncode == 0, f"Clean failed: {result.stderr}"
        
        result = subprocess.run(["make", "-j4"], capture_output=True, text=True)
        assert result.returncode == 0, f"Build failed: {result.stderr}"
        
        # Verify binaries exist
        assert Path("pgraft/pgraft.dylib").exists(), "PGRaft extension not built"
        assert Path("ramd/ramd").exists(), "RAMD daemon not built"
        assert Path("ramctrl/ramctrl").exists(), "RAMCTRL CLI not built"
    
    def test_component_help_commands(self):
        """Test that all components show help"""
        # Test RAMD help
        result = subprocess.run(["./ramd/ramd", "--help"], capture_output=True, text=True)
        assert result.returncode == 0, f"RAMD help failed: {result.stderr}"
        assert "Usage:" in result.stdout, "RAMD help not showing usage"
        
        # Test RAMCTRL help
        result = subprocess.run(["./ramctrl/ramctrl", "--help"], capture_output=True, text=True)
        assert result.returncode == 0, f"RAMCTRL help failed: {result.stderr}"
        assert "USAGE:" in result.stdout, "RAMCTRL help not showing usage"
    
    def test_configuration_validation(self):
        """Test configuration file validation"""
        # Test that configuration files exist and are valid
        config_files = [
            "conf/ramd.conf",
            "conf/ramctrl.conf",
            "conf/cluster.json"
        ]
        
        for config_file in config_files:
            if Path(config_file).exists():
                # Basic file validation
                with open(config_file, 'r') as f:
                    content = f.read()
                    assert len(content) > 0, f"Configuration file {config_file} is empty"
                    assert not content.startswith("ERROR"), f"Configuration file {config_file} has errors"
    
    def test_documentation_completeness(self):
        """Test that all documentation is present"""
        doc_files = [
            "README.md",
            "INSTALL.md",
            "doc/README.md",
            "tests/README.md"
        ]
        
        for doc_file in doc_files:
            assert Path(doc_file).exists(), f"Documentation file {doc_file} missing"
            
            with open(doc_file, 'r') as f:
                content = f.read()
                assert len(content) > 100, f"Documentation file {doc_file} too short"
