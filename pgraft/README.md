# PGRaft - PostgreSQL Raft Extension

A PostgreSQL extension that provides distributed consensus capabilities using the Raft algorithm. Built with 100% PostgreSQL C coding standards and enterprise-grade reliability.

## Features

### Core Functionality
- **Raft Consensus Algorithm**: Distributed consensus for PostgreSQL clusters
- **Automatic Leader Election**: Sub-second leader election with split-brain prevention
- **Log Replication**: Consistent log replication across all cluster nodes
- **Shared Memory Integration**: PostgreSQL shared memory for persistent state
- **Background Workers**: Non-blocking background processes for consensus operations
- **Health Monitoring**: Real-time health checks and status reporting

### Advanced Features
- **Go Integration**: High-performance Go-based Raft library integration
- **Configuration Management**: Dynamic configuration updates without restart
- **Metrics Collection**: Prometheus-compatible metrics export
- **Error Recovery**: Automatic recovery from network partitions and failures
- **Performance Optimization**: Optimized for minimal latency and maximum throughput

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    PostgreSQL Instance                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   Application   │  │   PGRaft Ext    │  │ Background  │  │
│  │     Layer       │  │                 │  │  Workers    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   SQL Layer     │  │   Raft Engine   │  │   Metrics   │  │
│  │                 │  │   (Go Library)  │  │  Collector  │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │  Shared Memory  │  │   Network I/O   │  │   Logging   │  │
│  │                 │  │                 │  │   System    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Installation

### Prerequisites
- PostgreSQL 12+ with development headers
- C compiler (GCC or Clang)
- Go 1.19+ (for Raft library)
- Make and build tools

### Build and Install

```bash
# Navigate to pgraft directory
cd pgraft

# Clean and build
make clean
make

# Install the extension
sudo make install

# Create extension in database
psql -d postgres -c "CREATE EXTENSION pgraft;"
```

### Verify Installation

```sql
-- Check extension is loaded
SELECT * FROM pg_extension WHERE extname = 'pgraft';

-- Check available functions
\df pgraft*

-- Check configuration parameters
SHOW pgraft.*;
```

## Configuration

### PostgreSQL Configuration

Add to `postgresql.conf`:

```ini
# PGRaft Configuration
shared_preload_libraries = 'pgraft'

# Raft Configuration
pgraft.enabled = on
pgraft.node_id = 1
pgraft.cluster_addresses = 'node1:5432,node2:5432,node3:5432'
pgraft.heartbeat_interval = 1000
pgraft.election_timeout = 5000
pgraft.log_level = info

# Performance Tuning
pgraft.max_log_entries = 10000
pgraft.snapshot_interval = 1000
pgraft.compaction_threshold = 1000
```

### Environment Variables

```bash
# Load configuration
source conf/environment.conf

# Set cluster-specific variables
export PGRaft_NODE_ID=1
export PGRaft_CLUSTER_ADDRESSES="node1:5432,node2:5432,node3:5432"
export PGRaft_LOG_LEVEL=info
```

## Usage

### Initialization

```sql
-- Initialize the Raft cluster
SELECT pgraft_init();

-- Check cluster status
SELECT pgraft_get_cluster_status();

-- Get current leader
SELECT pgraft_get_leader();
```

### Cluster Management

```sql
-- Add a node to the cluster
SELECT pgraft_add_node('node4', '192.168.1.4', 5432);

-- Remove a node from the cluster
SELECT pgraft_remove_node('node4');

-- Get cluster health
SELECT pgraft_get_cluster_health();
```

### Monitoring

```sql
-- Get detailed cluster information
SELECT * FROM pgraft_cluster_overview;

-- Get node status
SELECT * FROM pgraft_node_status;

-- Get Raft metrics
SELECT * FROM pgraft_metrics;
```

## API Reference

### Core Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `pgraft_init()` | Initialize Raft cluster | boolean |
| `pgraft_start()` | Start Raft consensus | boolean |
| `pgraft_stop()` | Stop Raft consensus | boolean |
| `pgraft_is_leader()` | Check if current node is leader | boolean |
| `pgraft_get_leader()` | Get current leader node | text |

### Cluster Management

| Function | Description | Returns |
|----------|-------------|---------|
| `pgraft_add_node(hostname, address, port)` | Add node to cluster | boolean |
| `pgraft_remove_node(hostname)` | Remove node from cluster | boolean |
| `pgraft_get_cluster_status()` | Get cluster status | json |
| `pgraft_get_cluster_health()` | Get cluster health | json |

### Monitoring

| Function | Description | Returns |
|----------|-------------|---------|
| `pgraft_get_metrics()` | Get Raft metrics | json |
| `pgraft_get_log_entries()` | Get log entries | json |
| `pgraft_get_node_info()` | Get node information | json |

## Monitoring & Metrics

### Prometheus Metrics

The extension exposes metrics in Prometheus format:

```
# Raft consensus metrics
pgraft_raft_term_total
pgraft_raft_log_entries_total
pgraft_raft_heartbeat_duration_seconds
pgraft_raft_election_duration_seconds

# Cluster health metrics
pgraft_cluster_nodes_total
pgraft_cluster_healthy_nodes_total
pgraft_cluster_leader_changes_total
```

### Health Checks

```sql
-- Basic health check
SELECT pgraft_health_check();

-- Detailed health information
SELECT * FROM pgraft_health_details;
```

## Development

### Building from Source

```bash
# Clone repository
git clone https://github.com/pgElephant/ram.git
cd ram/pgraft

# Install dependencies
go mod tidy
go mod download

# Build extension
make clean
make

# Run tests
make test
```

### Code Structure

```
pgraft/
├── src/                    # C source files
│   ├── pgraft.c           # Main extension file
│   ├── raft.c             # Raft consensus logic
│   ├── comm.c             # Network communication
│   ├── health_worker.c    # Health monitoring
│   ├── metrics.c          # Metrics collection
│   └── pgraft_go.go       # Go integration
├── include/                # Header files
│   └── pgraft.h           # Main header
├── sql/                    # SQL scripts
│   ├── pgraft_cluster.sql # Cluster functions
│   └── pgraft_views.sql   # Monitoring views
└── Makefile               # Build configuration
```

## Testing

### Unit Tests

```bash
# Run unit tests
make test

# Run specific test
make test TEST=test_raft_consensus
```

### Integration Tests

```bash
# Run integration tests
python3 tests/test_pgraft_integration.py

# Run with verbose output
python3 tests/test_pgraft_integration.py -v
```

### Performance Tests

```bash
# Run performance benchmarks
make benchmark

# Run stress tests
python3 tests/test_pgraft_stress.py
```

## Security

### Security Features
- **Input Validation**: All inputs are validated and sanitized
- **SQL Injection Protection**: Parameterized queries only
- **Memory Safety**: Bounds checking and safe memory operations
- **Access Control**: Role-based access to Raft functions
- **Audit Logging**: Complete audit trail of all operations

### Security Best Practices
- Use least privilege principle for database roles
- Enable SSL/TLS for network communications
- Regularly update dependencies
- Monitor for security vulnerabilities
- Follow PostgreSQL security guidelines

## Troubleshooting

### Common Issues

#### Extension Not Loading
```sql
-- Check if extension is in shared_preload_libraries
SHOW shared_preload_libraries;

-- Check for errors in PostgreSQL logs
-- Look for pgraft-related error messages
```

#### Raft Not Starting
```sql
-- Check configuration
SHOW pgraft.*;

-- Check if cluster addresses are correct
SELECT pgraft_get_cluster_status();
```

#### Performance Issues
```sql
-- Check metrics
SELECT * FROM pgraft_metrics;

-- Check log entries
SELECT * FROM pgraft_log_entries LIMIT 10;
```

### Debug Mode

Enable debug logging:

```sql
-- Set debug log level
ALTER SYSTEM SET pgraft.log_level = 'debug';
SELECT pg_reload_conf();
```

## Additional Resources

- [PostgreSQL Extension Development](https://www.postgresql.org/docs/current/extend.html)
- [Raft Algorithm Paper](https://raft.github.io/raft.pdf)
- [Go Raft Library](https://github.com/etcd-io/raft)
- [PostgreSQL C Coding Standards](https://www.postgresql.org/docs/current/source.html)

## Contributing

1. Fork the repository
2. Create a feature branch
3. Follow PostgreSQL C coding standards
4. Add tests for new functionality
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](../../LICENSE) file for details.

---

**PGRaft: Bringing distributed consensus to PostgreSQL.** 
