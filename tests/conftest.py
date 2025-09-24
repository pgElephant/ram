"""
Test configuration and fixtures for RAM project
"""

import pytest
import os
import tempfile
import shutil
from pathlib import Path

@pytest.fixture(scope="session")
def test_data_dir():
    """Create temporary test data directory"""
    temp_dir = tempfile.mkdtemp(prefix="ram_test_")
    yield Path(temp_dir)
    shutil.rmtree(temp_dir, ignore_errors=True)

@pytest.fixture(scope="session")
def test_config():
    """Test configuration"""
    return {
        "pgraft": {
            "node_id": 1,
            "address": "127.0.0.1",
            "port": 5432
        },
        "ramd": {
            "api_port": 8080,
            "log_level": "debug"
        },
        "ramctrl": {
            "api_url": "http://127.0.0.1:8080",
            "timeout": 30
        }
    }

@pytest.fixture
def mock_pgraft():
    """Mock PGRaft extension"""
    # Mock implementation for testing
    pass

@pytest.fixture
def mock_ramd():
    """Mock RAMD daemon"""
    # Mock implementation for testing
    pass

@pytest.fixture
def mock_ramctrl():
    """Mock RAMCTRL CLI"""
    # Mock implementation for testing
    pass
