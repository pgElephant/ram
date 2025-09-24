# RAM Test Suite

Comprehensive test suite for the RAM PostgreSQL High Availability System.

## Overview

This test suite provides comprehensive testing for all RAM components:
- **PGRaft Extension**: PostgreSQL Raft consensus extension
- **RAMD Daemon**: High availability daemon
- **RAMCTRL CLI**: Command-line interface
- **Integration**: Cross-component functionality
- **Performance**: Load and stress testing
- **Security**: Authentication and authorization

## Test Categories

### Unit Tests (`unit/`)
Individual component testing with isolated test cases.

- `test_pgraft_unit.py` - PGRaft extension unit tests
- `test_ramd_unit.py` - RAMD daemon unit tests
- `test_ramctrl_unit.py` - RAMCTRL CLI unit tests

### Integration Tests (`integration/`)
Cross-component integration testing.

- `test_cluster_integration.py` - Cluster integration tests
- `test_api_integration.py` - API integration tests
- `test_backup_integration.py` - Backup integration tests

### Performance Tests (`performance/`)
Load and stress testing.

- `test_performance.py` - Performance benchmarks
- `test_load.py` - Load testing
- `test_stress.py` - Stress testing

### Security Tests (`security/`)
Authentication and authorization testing.

- `test_security.py` - Security tests
- `test_auth.py` - Authentication tests
- `test_ssl.py` - SSL/TLS tests

### API Tests (`api/`)
REST API endpoint testing.

- `test_api.py` - API endpoint tests
- `test_http.py` - HTTP protocol tests
- `test_metrics.py` - Metrics API tests

### Cluster Tests (`cluster/`)
Multi-node cluster testing.

- `test_cluster.py` - Cluster operations
- `test_failover.py` - Failover testing
- `test_replication.py` - Replication testing

## Quick Start

### Prerequisites

```bash
# Install dependencies
pip3 install psycopg2-binary requests

# Ensure PostgreSQL is available
which psql

# Ensure build tools are available
which make gcc
```

### Running Tests

```bash
# Run all tests
make test

# Run specific category
make unit
make integration
make performance
make security
make api
make cluster

# Run with Python test runner
python3 run_tests.py --category=all

# Run specific test file
python3 unit/test_pgraft_unit.py
```

### Test Runner Options

```bash
# Run tests in parallel
python3 run_tests.py --parallel

# Verbose output
python3 run_tests.py --verbose

# Generate report
python3 run_tests.py --output=test_report.json

# Setup environment only
python3 run_tests.py --setup-only

# Cleanup environment only
python3 run_tests.py --cleanup-only
```

## Test Configuration

### Environment Variables

```bash
# PostgreSQL configuration
export PGHOST=localhost
export PGPORT=5432
export PGUSER=postgres
export PGPASSWORD=postgres
export PGDATABASE=postgres

# RAMD configuration
export RAMD_HOST=localhost
export RAMD_PORT=8008
export RAMD_TOKEN=your-token

# Test configuration
export TEST_DATA_DIR=/tmp/test_data
export TEST_LOG_DIR=/tmp/test_logs
export TEST_TIMEOUT=300
```

### Test Configuration File

Create `test_config.json`:

```json
{
  "postgresql": {
    "host": "localhost",
    "port": 5432,
    "user": "postgres",
    "password": "postgres",
    "database": "postgres"
  },
  "ramd": {
    "host": "localhost",
    "port": 8008,
    "token": "your-token"
  },
  "test": {
    "data_dir": "/tmp/test_data",
    "log_dir": "/tmp/test_logs",
    "timeout": 300,
    "parallel": true
  }
}
```

## Test Structure

### TAP Protocol

Tests use the Test Anything Protocol (TAP) for standardized output:

```
TAP version 13
1..5
ok 1 - test_pgraft_extension_loading
ok 2 - test_pgraft_functions
ok 3 - test_pgraft_health_check
not ok 4 - test_pgraft_network_communication
# Error: Network timeout
ok 5 - test_pgraft_cleanup
```

### Test Result Format

```python
@dataclass
class TestResult:
    test_name: str
    category: str
    passed: bool
    message: str
    duration: float
    details: Optional[Dict] = None
    timestamp: str = ""
```

## Writing Tests

### Unit Test Example

```python
import unittest
import psycopg2

class PGRaftUnitTests(unittest.TestCase):
    def setUp(self):
        self.conn = psycopg2.connect(
            host='localhost',
            port=5432,
            user='postgres',
            password='postgres',
            database='postgres'
        )
        self.cursor = self.conn.cursor()
    
    def tearDown(self):
        self.cursor.close()
        self.conn.close()
    
    def test_pgraft_init(self):
        self.cursor.execute("SELECT pgraft_init()")
        result = self.cursor.fetchone()
        self.assertTrue(result[0], "pgraft_init failed")

if __name__ == '__main__':
    unittest.main()
```

### Integration Test Example

```python
import requests
import time

def test_cluster_health():
    """Test cluster health endpoint"""
    response = requests.get('http://localhost:8008/api/v1/cluster/health')
    assert response.status_code == 200
    
    data = response.json()
    assert data['status'] == 'success'
    assert 'cluster_status' in data['data']
```

### Performance Test Example

```python
import time
import psycopg2

def test_connection_performance():
    """Test connection performance"""
    start_time = time.time()
    
    for i in range(100):
        conn = psycopg2.connect(
            host='localhost',
            port=5432,
            user='postgres',
            password='postgres',
            database='postgres'
        )
        conn.close()
    
    duration = time.time() - start_time
    assert duration < 10.0, f"Connection performance too slow: {duration}s"
```

## Continuous Integration

### GitHub Actions

```yaml
name: RAM Test Suite

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Setup PostgreSQL
      run: |
        sudo apt-get update
        sudo apt-get install -y postgresql postgresql-contrib
    
    - name: Setup Python
      run: |
        python3 -m pip install --upgrade pip
        pip install psycopg2-binary requests
    
    - name: Build RAM
      run: make all
    
    - name: Run Tests
      run: |
        cd tests
        python3 run_tests.py --category=all --output=test_report.json
    
    - name: Upload Test Report
      uses: actions/upload-artifact@v2
      with:
        name: test-report
        path: tests/test_report.json
```

### Jenkins Pipeline

```groovy
pipeline {
    agent any
    
    stages {
        stage('Setup') {
            steps {
                sh 'make clean'
                sh 'make all'
            }
        }
        
        stage('Test') {
            steps {
                sh 'cd tests && python3 run_tests.py --category=all'
            }
        }
        
        stage('Report') {
            steps {
                publishTestResults testResultsPattern: 'tests/test_report.json'
            }
        }
    }
}
```

## Test Reports

### JSON Report

```json
{
  "summary": {
    "total_tests": 25,
    "passed_tests": 23,
    "failed_tests": 2,
    "success_rate": 92.0,
    "total_duration": 45.2,
    "timestamp": "2024-01-01T12:00:00Z"
  },
  "categories": {
    "unit": {"total": 10, "passed": 10, "duration": 5.2},
    "integration": {"total": 8, "passed": 7, "duration": 25.1},
    "performance": {"total": 4, "passed": 4, "duration": 10.5},
    "security": {"total": 3, "passed": 2, "duration": 4.4}
  },
  "results": [...]
}
```

### HTML Report

Generate HTML reports using the JSON output:

```python
import json
from jinja2 import Template

def generate_html_report(json_file, html_file):
    with open(json_file) as f:
        data = json.load(f)
    
    # Generate HTML using template
    # ... HTML generation code ...
```

## Troubleshooting

### Common Issues

1. **Tests failing to start**
   ```bash
   # Check dependencies
   python3 -c "import psycopg2, requests"
   
   # Check PostgreSQL
   psql -h localhost -p 5432 -U postgres -d postgres
   ```

2. **Permission errors**
   ```bash
   # Fix permissions
   chmod +x run_tests.py
   chmod +x test_suite.py
   ```

3. **Port conflicts**
   ```bash
   # Check port usage
   netstat -tlnp | grep -E "(5432|8008|7400)"
   
   # Kill conflicting processes
   pkill -f postgres
   pkill -f ramd
   ```

### Debug Mode

```bash
# Enable debug output
python3 run_tests.py --verbose

# Run single test with debug
python3 -u unit/test_pgraft_unit.py

# Check test logs
tail -f /tmp/test_logs/test.log
```

## Contributing

### Adding New Tests

1. Create test file in appropriate category directory
2. Follow naming convention: `test_*.py`
3. Use TAP protocol for output
4. Include proper error handling
5. Add documentation

### Test Guidelines

1. **Isolation**: Tests should not depend on each other
2. **Cleanup**: Always clean up test artifacts
3. **Timeout**: Set appropriate timeouts
4. **Error Handling**: Handle expected failures gracefully
5. **Documentation**: Document test purpose and requirements

### Code Style

```python
# Use descriptive test names
def test_pgraft_extension_loads_correctly():
    """Test that PGRaft extension loads without errors"""
    pass

# Use proper assertions
assert result is not None, "Expected non-null result"
self.assertEqual(expected, actual, "Values should match")

# Clean up resources
try:
    # Test code
    pass
finally:
    # Cleanup code
    pass
```

## Performance Benchmarks

### Baseline Performance

| Test | Duration | Memory | CPU |
|------|----------|--------|-----|
| Unit Tests | 5.2s | 50MB | 10% |
| Integration Tests | 25.1s | 200MB | 30% |
| Performance Tests | 10.5s | 500MB | 80% |
| Security Tests | 4.4s | 30MB | 5% |

### Performance Targets

- Unit tests: < 10s
- Integration tests: < 60s
- Performance tests: < 30s
- Security tests: < 10s
- Total test suite: < 120s

## Support

- **Documentation**: See main documentation
- **Issues**: Report on GitHub
- **Community**: Join discussions
- **Support**: Contact support team

---

**Ready to test?** Run `make test` to get started!