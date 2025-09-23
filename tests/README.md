# Test Suite Documentation

This directory contains comprehensive tests for the pgraft/ramd/ramctrl system.

## Test Organization

### Unit Tests (`unit/`)
- `test_pgraft.c` - PostgreSQL extension unit tests
- `test_ramd.c` - RAMD daemon unit tests
- `test_ramctrl.c` - RAMCTRL CLI unit tests
- `test_shared_memory.c` - Shared memory functionality tests
- `test_raft_consensus.c` - Raft consensus algorithm tests

### Integration Tests (`integration/`)
- `test_cluster_setup.c` - Multi-node cluster setup tests
- `test_failover.c` - Failover and recovery tests
- `test_replication.c` - PostgreSQL replication tests
- `test_api_integration.c` - HTTP API integration tests
- `test_pgraft_ramd_integration.c` - pgraft-ramd integration tests

### Performance Tests (`performance/`)
- `test_throughput.c` - Throughput and latency tests
- `test_memory_usage.c` - Memory consumption tests
- `test_concurrent_operations.c` - Concurrent operation tests
- `test_large_dataset.c` - Large dataset handling tests

### Security Tests (`security/`)
- `test_authentication.c` - Authentication mechanism tests
- `test_authorization.c` - Authorization and access control tests
- `test_encryption.c` - Encryption and SSL/TLS tests
- `test_input_validation.c` - Input validation and sanitization tests
- `test_rate_limiting.c` - Rate limiting and DoS protection tests

### API Tests (`api/`)
- `test_rest_api.c` - REST API endpoint tests
- `test_http_methods.c` - HTTP method validation tests
- `test_response_formats.c` - Response format validation tests
- `test_error_handling.c` - API error handling tests

### Cluster Tests (`cluster/`)
- `test_leader_election.c` - Raft leader election tests
- `test_node_management.c` - Node addition/removal tests
- `test_network_partition.c` - Network partition handling tests
- `test_consensus.c` - Consensus algorithm tests
- `test_cluster_health.c` - Cluster health monitoring tests

## Running Tests

### All Tests
```bash
make test
```

### Specific Test Categories
```bash
make test-unit
make test-integration
make test-performance
make test-security
make test-api
make test-cluster
```

### Individual Tests
```bash
./tests/unit/test_pgraft
./tests/integration/test_cluster_setup
./tests/performance/test_throughput
```

## Test Configuration

Tests use configuration files from `conf/` directory:
- `conf/test/test_cluster.json` - Test cluster configuration
- `conf/test/test_ramd.conf` - Test RAMD configuration
- `conf/test/test_pgraft.conf` - Test pgraft configuration

## Test Data

Test data is stored in `tests/data/`:
- Sample PostgreSQL dumps
- Configuration templates
- Test datasets
- Expected output files

## Continuous Integration

Tests are designed to run in CI/CD pipelines:
- Docker-based test environments
- Automated test execution
- Coverage reporting
- Performance benchmarking
- Security scanning

## Test Standards

- All tests must be deterministic
- Tests should clean up after themselves
- Use proper test isolation
- Include both positive and negative test cases
- Document test scenarios and expected outcomes
- Maintain test coverage above 90%
