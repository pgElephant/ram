"""
Integration tests for cluster management
"""

import pytest
import subprocess
import time
import json
from pathlib import Path

@pytest.mark.integration
@pytest.mark.slow
class TestClusterIntegration:
    """Cluster integration tests"""
    
    def test_cluster_script_exists(self):
        """Test that cluster management script exists"""
        cluster_script = Path("scripts/cluster.py")
        assert cluster_script.exists(), "Cluster management script not found"
    
    def test_cluster_config_exists(self):
        """Test that cluster configuration exists"""
        cluster_config = Path("scripts/cluster.json")
        assert cluster_config.exists(), "Cluster configuration not found"
        
        # Validate JSON format
        with open(cluster_config, 'r') as f:
            config = json.load(f)
            assert "cluster_name" in config, "Cluster name not found in config"
            assert "nodes" in config, "Nodes not found in config"
            assert len(config["nodes"]) > 0, "No nodes defined in config"
    
    def test_cluster_script_help(self):
        """Test that cluster script shows help"""
        result = subprocess.run(
            ["python3", "scripts/cluster.py", "--help"],
            capture_output=True,
            text=True
        )
        assert result.returncode == 0, f"Cluster script help failed: {result.stderr}"
        assert "usage:" in result.stdout.lower(), "Help not showing usage"
    
    def test_cluster_script_commands(self):
        """Test that cluster script supports required commands"""
        result = subprocess.run(
            ["python3", "scripts/cluster.py", "--help"],
            capture_output=True,
            text=True
        )
        
        help_text = result.stdout.lower()
        required_commands = [
            "create",
            "destroy", 
            "start",
            "stop",
            "status"
        ]
        
        for command in required_commands:
            assert command in help_text, f"Required command '{command}' not found in help"
    
    def test_ramd_binary_executable(self):
        """Test that RAMD binary is executable"""
        ramd_path = Path("ramd/ramd")
        assert ramd_path.exists(), "RAMD binary not found"
        assert os.access(ramd_path, os.X_OK), "RAMD binary not executable"
    
    def test_ramctrl_binary_executable(self):
        """Test that RAMCTRL binary is executable"""
        ramctrl_path = Path("ramctrl/ramctrl")
        assert ramctrl_path.exists(), "RAMCTRL binary not found"
        assert os.access(ramctrl_path, os.X_OK), "RAMCTRL binary not executable"
    
    def test_ramd_help_command(self):
        """Test RAMD help command"""
        result = subprocess.run(
            ["./ramd/ramd", "--help"],
            capture_output=True,
            text=True
        )
        assert result.returncode == 0, f"RAMD help failed: {result.stderr}"
        assert "usage:" in result.stdout.lower(), "RAMD help not showing usage"
    
    def test_ramctrl_help_command(self):
        """Test RAMCTRL help command"""
        result = subprocess.run(
            ["./ramctrl/ramctrl", "--help"],
            capture_output=True,
            text=True
        )
        assert result.returncode == 0, f"RAMCTRL help failed: {result.stderr}"
        assert "usage:" in result.stdout.lower(), "RAMCTRL help not showing usage"
