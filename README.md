# PostgreSQL Distributed Consensus Project

This project provides distributed consensus capabilities for PostgreSQL using custom Raft implementations. It consists of multiple components working together to provide high availability and consistency across PostgreSQL clusters.

## Project Structure

```
├── pgraft/                 # PostgreSQL extension for distributed consensus
│   ├── src/               # C source files
│   ├── include/           # Header files
│   ├── sql/               # SQL functions and views
│   └── README.md          # Extension documentation
├── ramd/                  # Daemon for cluster management
├── ramctrl/               # Control utility for cluster operations
├── rale/                  # Raft consensus library (legacy)
└── README.md              # This file
```

## Components

### pgraft - PostgreSQL Extension

A pure C PostgreSQL extension that provides distributed consensus capabilities using a custom Raft implementation.

**Features:**
- Custom Raft consensus algorithm
- Background worker process
- Comprehensive monitoring and metrics
- GUC configuration management
- SQL function interface

**Documentation:** [pgraft/README.md](pgraft/README.md)

### ramd - Cluster Management Daemon

A daemon that manages PostgreSQL clusters and provides HTTP API for cluster operations.

**Features:**
- Cluster health monitoring
- HTTP API for cluster management
- Integration with pgraft extension
- Node discovery and management

### ramctrl - Control Utility

A command-line utility for managing PostgreSQL clusters and performing cluster operations.

**Features:**
- Cluster status monitoring
- Node management operations
- Configuration management
- Health checking

## Quick Start

### 1. Build pgraft Extension

```bash
cd pgraft
make
sudo make install
```

### 2. Enable Extension in PostgreSQL

```sql
-- Add to postgresql.conf
shared_preload_libraries = 'pgraft'

-- Restart PostgreSQL
-- Create extension in database
CREATE EXTENSION pgraft;
```

### 3. Initialize Cluster

```sql
-- Initialize node
SELECT pgraft_init(1, '192.168.1.10', 5432);
SELECT pgraft_start();

-- Add other nodes
SELECT pgraft_add_node(2, '192.168.1.11', 5432);
SELECT pgraft_add_node(3, '192.168.1.12', 5432);
```

### 4. Monitor Cluster

```sql
-- Check cluster health
SELECT pgraft_get_cluster_health();

-- Get performance metrics
SELECT pgraft_get_performance_metrics();

-- Check if healthy
SELECT pgraft_is_cluster_healthy();
```

## Development

### Prerequisites

- PostgreSQL 12+ with development headers
- GCC or compatible C compiler
- Make
- pthread library

### Building

```bash
# Build all components
make -C pgraft
make -C ramd
make -C ramctrl

# Clean all
make -C pgraft clean
make -C ramd clean
make -C ramctrl clean
```

### Testing

```bash
# Test pgraft extension
cd pgraft
make test

# Test cluster operations
psql -d testdb -c "SELECT pgraft_version();"
```

## Architecture

The project uses a modular architecture:

1. **pgraft Extension**: Core consensus logic in PostgreSQL
2. **ramd Daemon**: Cluster management and HTTP API
3. **ramctrl Utility**: Command-line interface for operations
4. **Communication Layer**: Network communication between components

## Configuration

### pgraft Extension

Configure using PostgreSQL GUC parameters:

```sql
-- Core settings
ALTER SYSTEM SET pgraft.node_id = 1;
ALTER SYSTEM SET pgraft.address = '192.168.1.10';
ALTER SYSTEM SET pgraft.port = 5432;

-- Cluster settings
ALTER SYSTEM SET pgraft.cluster_name = 'my_cluster';
ALTER SYSTEM SET pgraft.cluster_size = 3;

-- Performance settings
ALTER SYSTEM SET pgraft.heartbeat_interval = 1000;
ALTER SYSTEM SET pgraft.election_timeout = 5000;

-- Reload configuration
SELECT pg_reload_conf();
```

### ramd Daemon

Configure using command-line options or configuration file:

```bash
# Start daemon
./ramd --node-id=1 --address=192.168.1.10 --port=8080

# Or use configuration file
./ramd --config=ramd.conf
```

## Monitoring

### Health Checks

```sql
-- Extension health
SELECT pgraft_get_cluster_health();

-- System statistics
SELECT pgraft_get_system_stats();

-- Performance metrics
SELECT pgraft_get_performance_metrics();
```

### HTTP API (ramd)

```bash
# Get cluster status
curl http://localhost:8080/api/cluster/status

# Get node health
curl http://localhost:8080/api/node/health

# Get metrics
curl http://localhost:8080/api/metrics
```

## Troubleshooting

### Common Issues

1. **Extension not loading**: Check `shared_preload_libraries` in postgresql.conf
2. **Node connection issues**: Verify network connectivity and firewall settings
3. **Build errors**: Ensure PostgreSQL development headers are installed
4. **Permission errors**: Check PostgreSQL user permissions

### Debugging

```sql
-- Enable debug logging
SET log_min_messages = DEBUG1;

-- Check extension status
SELECT * FROM pg_extension WHERE extname = 'pgraft';

-- Monitor cluster
SELECT pgraft_get_cluster_health();
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## License

This project is licensed under the Apache 2.0 License - see the LICENSE file for details.

## Support

For questions and support:

- Check the documentation in each component directory
- Review the troubleshooting section
- Open an issue on GitHub

## References

- [PostgreSQL Extension Development](https://www.postgresql.org/docs/current/extend.html)
- [Raft Consensus Algorithm](https://raft.github.io/raft.pdf)
- [PostgreSQL Background Workers](https://www.postgresql.org/docs/current/bgworker.html)