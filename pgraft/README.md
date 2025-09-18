# pgraft - RAFT-based PostgreSQL Extension for Distributed Consensus.

A PostgreSQL extension that provides distributed consensus capabilities using a custom Raft implementation written in C, designed for high performance and PostgreSQL integration.

## Features

- **Distributed Consensus**: Custom Raft consensus algorithm implementation
- **High Availability**: Automatic leader election and failover
- **Log Replication**: Consistent log replication across cluster nodes
- **Background Worker Process**: Dedicated worker process for handling consensus operations
- **Configuration Management**: Dynamic cluster membership changes
- **GUC Configuration**: Full PostgreSQL GUC integration for all settings
- **Monitoring**: Comprehensive status and statistics views
- **PostgreSQL Integration**: Native SQL functions and views
- **Pure C Implementation**: No external dependencies, optimized for PostgreSQL

## Architecture

pgraft uses a pure C architecture with PostgreSQL integration:

- **C Layer**: PostgreSQL extension interface and Raft consensus implementation
- **Background Worker**: Dedicated process for handling leader elections and consensus operations
- **Communication Layer**: Network communication between cluster nodes
- **Monitoring System**: Health checks and performance metrics
- **Memory Management**: PostgreSQL memory context integration

This design provides optimal performance and reliability while maintaining full PostgreSQL compatibility.

### Worker Process

The pgraft extension includes a background worker process that:

- Handles leader elections automatically
- Manages consensus operations in isolation
- Provides better performance and reliability
- Supports configuration reloading without restart
- Monitors cluster health continuously

The worker process is automatically started when the extension is loaded and runs as a PostgreSQL background worker.

## Installation

### Prerequisites

- PostgreSQL 12+ with development headers
- GCC or compatible C compiler
- Make
- pthread library

### Build and Install

```bash
# Clone the repository
git clone <repository-url>
cd pgraft

# Build the extension
make

# Install the extension
sudo make install

# Create the extension in PostgreSQL
psql -d your_database -c "CREATE EXTENSION pgraft;"
```

### Development Build

```bash
# Build with debug symbols
make DEBUG=1

# Clean build artifacts
make clean
```

## Usage

### Basic Setup

#### 1. Initialize a Single Node

```sql
-- Initialize this node
SELECT pgraft_init(1, '192.168.1.10', 5432);

-- Start the raft instance
SELECT pgraft_start();
```

#### 2. Set Up a Multi-Node Cluster

**On Node 1 (192.168.1.10:5432):**
```sql
SELECT pgraft_init(1, '192.168.1.10', 5432);
SELECT pgraft_start();
SELECT pgraft_add_node(2, '192.168.1.11', 5432);
SELECT pgraft_add_node(3, '192.168.1.12', 5432);
```

**On Node 2 (192.168.1.11:5432):**
```sql
SELECT pgraft_init(2, '192.168.1.11', 5432);
SELECT pgraft_start();
```

**On Node 3 (192.168.1.12:5432):**
```sql
SELECT pgraft_init(3, '192.168.1.12', 5432);
SELECT pgraft_start();
```

### Core Operations

#### Monitor Cluster Status

```sql
-- Get cluster health
SELECT pgraft_get_cluster_health();

-- Check if cluster is healthy
SELECT pgraft_is_cluster_healthy();

-- Get performance metrics
SELECT pgraft_get_performance_metrics();

-- Get system statistics
SELECT pgraft_get_system_stats();

-- Get quorum status
SELECT pgraft_get_quorum_status();
```

#### Append Data to Log

```sql
-- Append data (only works on leader)
SELECT pgraft_append_log('Hello, distributed world!');

-- Read log entry
SELECT pgraft_read_log(1);

-- Commit log entry
SELECT pgraft_commit_log(1);
```

#### Cluster Management

```sql
-- Add a new node to the cluster
SELECT pgraft_add_node(4, '192.168.1.13', 5432);

-- Remove a node from the cluster
SELECT pgraft_remove_node(4);

-- Check if current node is leader
SELECT pgraft_is_leader();

-- Get current leader
SELECT pgraft_get_leader();

-- Get current term
SELECT pgraft_get_term();
```

## API Reference

### Core Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `pgraft_init(node_id, address, port)` | Initialize raft instance | boolean |
| `pgraft_start()` | Start raft instance | boolean |
| `pgraft_stop()` | Stop raft instance | boolean |
| `pgraft_add_node(node_id, address, port)` | Add node to cluster | boolean |
| `pgraft_remove_node(node_id)` | Remove node from cluster | boolean |

### Status Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `pgraft_get_state()` | Get current raft state | text |
| `pgraft_get_leader()` | Get current leader node ID | integer |
| `pgraft_get_term()` | Get current term | integer |
| `pgraft_is_leader()` | Check if current node is leader | boolean |
| `pgraft_get_stats()` | Get detailed statistics | text |

### Data Operations

| Function | Description | Returns |
|----------|-------------|---------|
| `pgraft_append_log(data)` | Append data to log | boolean |
| `pgraft_read_log(index)` | Read log entry at index | text |
| `pgraft_commit_log(index)` | Commit log entries up to index | boolean |
| `pgraft_get_log()` | Get all log entries | text |

### Monitoring Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `pgraft_get_cluster_health()` | Get cluster health status | text |
| `pgraft_get_performance_metrics()` | Get performance metrics | text |
| `pgraft_is_cluster_healthy()` | Check if cluster is healthy | boolean |
| `pgraft_get_system_stats()` | Get system statistics | text |
| `pgraft_get_quorum_status()` | Get quorum status | text |
| `pgraft_reset_metrics()` | Reset metrics | boolean |

### Utility Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `pgraft_version()` | Get extension version | text |

## Configuration

### GUC Parameters

The extension provides the following configuration parameters:

#### Core Configuration
- `pgraft.node_id` - Node ID for this instance (default: 1)
- `pgraft.port` - Port for pgraft communication (default: 5432)
- `pgraft.address` - Address for pgraft communication (default: localhost)
- `pgraft.log_level` - Log level (0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR)

#### Raft Configuration
- `pgraft.heartbeat_interval` - Heartbeat interval in milliseconds (default: 1000)
- `pgraft.election_timeout` - Election timeout in milliseconds (default: 5000)
- `pgraft.worker_enabled` - Enable background worker (default: true)
- `pgraft.worker_interval` - Worker interval in milliseconds (default: 1000)

#### Cluster Configuration
- `pgraft.cluster_name` - Name of the cluster (default: pgraft_cluster)
- `pgraft.cluster_size` - Expected cluster size (default: 3)
- `pgraft.enable_auto_cluster_formation` - Enable automatic cluster formation (default: true)

#### Monitoring Configuration
- `pgraft.node_name` - Node name for identification (default: pgraft_node_1)
- `pgraft.node_ip` - Node IP address (default: 127.0.0.1)
- `pgraft.is_primary` - Whether this node is a primary (default: false)
- `pgraft.health_period_ms` - Health check period in milliseconds (default: 5000)
- `pgraft.health_verbose` - Enable verbose health logging (default: false)
- `pgraft.metrics_enabled` - Enable metrics collection (default: true)
- `pgraft.trace_enabled` - Enable trace logging (default: false)

### Example Configuration

```sql
-- Set configuration parameters
ALTER SYSTEM SET pgraft.node_id = 1;
ALTER SYSTEM SET pgraft.address = '192.168.1.10';
ALTER SYSTEM SET pgraft.port = 5432;
ALTER SYSTEM SET pgraft.cluster_name = 'my_cluster';
ALTER SYSTEM SET pgraft.cluster_size = 3;
ALTER SYSTEM SET pgraft.heartbeat_interval = 1000;
ALTER SYSTEM SET pgraft.election_timeout = 5000;

-- Reload configuration
SELECT pg_reload_conf();
```

## Monitoring and Debugging

### Health Monitoring

The extension provides comprehensive health monitoring:

```sql
-- Get detailed health status
SELECT pgraft_get_cluster_health();

-- Check if cluster is healthy
SELECT pgraft_is_cluster_healthy();

-- Get performance metrics
SELECT pgraft_get_performance_metrics();

-- Get system statistics
SELECT pgraft_get_system_stats();

-- Get quorum status
SELECT pgraft_get_quorum_status();
```

### Log Levels

The extension uses PostgreSQL's logging system:

- **INFO**: Important events (leader election, node changes)
- **DEBUG1**: Detailed operations (log appends, commits)
- **ERROR**: Error conditions

### Statistics

The monitoring functions return JSON data with comprehensive statistics:

```json
{
  "total_requests": 1000,
  "successful_requests": 995,
  "failed_requests": 5,
  "success_rate": 0.995,
  "current_connections": 2,
  "max_connections": 3,
  "heartbeat_count": 500,
  "election_count": 2,
  "log_entries_appended": 100,
  "log_entries_committed": 95,
  "last_activity": "2024-01-01T12:00:00Z",
  "uptime_seconds": 3600
}
```

## Examples

### Example 1: Simple Key-Value Store

```sql
-- Create a simple key-value table
CREATE TABLE kv_store (
    key TEXT PRIMARY KEY,
    value TEXT,
    log_index BIGINT
);

-- Function to set a value through Raft
CREATE OR REPLACE FUNCTION set_value(k TEXT, v TEXT)
RETURNS BOOLEAN AS $$
DECLARE
    log_data TEXT;
    result BOOLEAN;
BEGIN
    -- Only proceed if we're the leader
    IF NOT pgraft_is_leader() THEN
        RAISE EXCEPTION 'Not the leader, cannot set value';
    END IF;
    
    -- Create log entry
    log_data := json_build_object('op', 'set', 'key', k, 'value', v)::text;
    
    -- Append to Raft log
    result := pgraft_append_log(log_data);
    
    RETURN result;
END;
$$ LANGUAGE plpgsql;
```

### Example 2: Distributed Configuration Management

```sql
-- Create configuration table
CREATE TABLE config (
    key TEXT PRIMARY KEY,
    value TEXT,
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Function to update configuration through Raft
CREATE OR REPLACE FUNCTION update_config(k TEXT, v TEXT)
RETURNS BOOLEAN AS $$
DECLARE
    log_data TEXT;
BEGIN
    IF NOT pgraft_is_leader() THEN
        RAISE EXCEPTION 'Not the leader, cannot update config';
    END IF;
    
    log_data := json_build_object(
        'op', 'config_update',
        'key', k,
        'value', v,
        'timestamp', extract(epoch from now())
    )::text;
    
    RETURN pgraft_append_log(log_data);
END;
$$ LANGUAGE plpgsql;
```

## Troubleshooting

### Common Issues

1. **"Not the leader" errors**: Only the leader can append to the log
2. **Node connection issues**: Ensure all nodes can communicate
3. **Memory issues**: Monitor PostgreSQL memory usage
4. **Build issues**: Ensure PostgreSQL development headers are installed

### Debugging

```sql
-- Enable debug logging
SET log_min_messages = DEBUG1;

-- Check cluster health
SELECT pgraft_get_cluster_health();

-- Monitor log entries
SELECT pgraft_get_log();

-- Check worker status
SELECT pgraft_get_system_stats();
```

## Performance Considerations

- **Network Latency**: Raft performance depends on network conditions
- **Log Size**: Large logs may impact performance
- **Node Count**: Odd numbers of nodes are recommended (3, 5, 7)
- **Memory Usage**: Each node maintains a copy of the log
- **Pure C Implementation**: Optimized for performance and memory usage

## Security

- **Network Security**: Use TLS for production deployments
- **Access Control**: Restrict pgraft functions to authorized users
- **Data Encryption**: Consider encrypting log entries for sensitive data

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## License

This project is licensed under the Apache 2.0 License - see the LICENSE file for details.

## Acknowledgments

- PostgreSQL community for the extension framework
- Raft consensus algorithm authors
- PostgreSQL extension development community

## References

- [Raft Paper](https://raft.github.io/raft.pdf)
- [PostgreSQL Extension Development](https://www.postgresql.org/docs/current/extend.html)
- [PostgreSQL Background Workers](https://www.postgresql.org/docs/current/bgworker.html)