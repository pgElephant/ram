# PGRaft Extension Guide

PGRaft is a PostgreSQL extension that provides Raft consensus algorithm integration for high availability and automatic failover.

## Overview

PGRaft extends PostgreSQL with:
- **Raft Consensus**: Distributed consensus algorithm for leader election
- **Shared Memory Integration**: Persistent state across PostgreSQL processes
- **Automatic Failover**: Seamless primary/replica transitions
- **Network Communication**: Inter-node communication for consensus

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   PostgreSQL    │    │   PostgreSQL    │    │   PostgreSQL    │
│   (Primary)     │    │   (Replica)     │    │   (Replica)     │
│                 │    │                 │    │                 │
│  ┌───────────┐  │    │  ┌───────────┐  │    │  ┌───────────┐  │
│  │  PGRaft   │  │◄──►│  │  PGRaft   │  │◄──►│  │  PGRaft   │  │
│  │ Extension │  │    │  │ Extension │  │    │  │ Extension │  │
│  └───────────┘  │    │  └───────────┘  │    │  └───────────┘  │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                    ┌─────────────────┐
                    │   Raft Network  │
                    │   (Consensus)   │
                    └─────────────────┘
```

## Installation

### Prerequisites

- PostgreSQL 17 or later
- Go 1.21 or later
- Build tools (make, gcc)

### Build and Install

```bash
# Navigate to pgraft directory
cd pgraft

# Build the extension
make

# Install the extension
make install

# Install Go dependencies
go mod tidy
go mod download
```

### PostgreSQL Configuration

Add PGRaft to your PostgreSQL configuration:

```bash
# Edit postgresql.conf
echo "shared_preload_libraries = 'pgraft'" >> /usr/local/var/postgresql@17/postgresql.conf

# Restart PostgreSQL
brew services restart postgresql@17
```

## Configuration

PGRaft uses the configuration file `conf/pgraft.conf`:

```ini
# Raft Configuration
raft_node_id = 1
raft_cluster_id = ram_cluster
raft_data_dir = /tmp/pgraft_data
raft_log_level = info

# Network Configuration
raft_port = 7400
raft_bind_address = 0.0.0.0
raft_peer_addresses = localhost:7401,localhost:7402

# PostgreSQL Integration
postgresql_integration_enabled = true
postgresql_shmem_key = 12345
postgresql_shmem_size = 1048576
```

## SQL Functions

PGRaft provides several SQL functions for cluster management:

### Cluster Management

```sql
-- Initialize Raft cluster
SELECT pgraft_init();

-- Start Raft consensus
SELECT pgraft_start();

-- Stop Raft consensus
SELECT pgraft_stop();

-- Get cluster status
SELECT pgraft_status();

-- Check if node is leader
SELECT pgraft_is_leader();
```

### Node Management

```sql
-- Add node to cluster
SELECT pgraft_add_node('localhost:7401', 2);

-- Remove node from cluster
SELECT pgraft_remove_node(2);

-- Get node information
SELECT pgraft_get_nodes();

-- Get leader information
SELECT pgraft_get_leader();
```

### Health and Monitoring

```sql
-- Check cluster health
SELECT pgraft_is_healthy();

-- Get cluster metrics
SELECT pgraft_get_metrics();

-- Enable/disable debug logging
SELECT pgraft_set_debug(true);
```

## Usage Examples

### Basic Cluster Setup

```sql
-- 1. Initialize the cluster
SELECT pgraft_init();

-- 2. Start Raft consensus
SELECT pgraft_start();

-- 3. Add replica nodes
SELECT pgraft_add_node('localhost:7401', 2);
SELECT pgraft_add_node('localhost:7402', 3);

-- 4. Check cluster status
SELECT pgraft_status();
```

### Monitoring Cluster Health

```sql
-- Check if cluster is healthy
SELECT pgraft_is_healthy();

-- Get detailed cluster information
SELECT * FROM pgraft_status();

-- Monitor leader elections
SELECT pgraft_get_leader();
```

### Handling Failovers

```sql
-- Check current leader
SELECT pgraft_get_leader();

-- Force leader election (if needed)
SELECT pgraft_force_election();

-- Check if this node is leader
SELECT pgraft_is_leader();
```

## Integration with RAMD

PGRaft integrates seamlessly with the RAMD daemon:

```bash
# Start RAMD daemon
./ramd --config=conf/ramd.conf

# RAMD will automatically:
# 1. Connect to PGRaft via shared memory
# 2. Monitor cluster health
# 3. Handle failover operations
# 4. Provide HTTP API for cluster management
```

## Troubleshooting

### Common Issues

1. **Extension not loading**
   ```sql
   -- Check if extension is loaded
   SELECT * FROM pg_extension WHERE extname = 'pgraft';
   
   -- Check shared_preload_libraries
   SHOW shared_preload_libraries;
   ```

2. **Raft consensus not starting**
   ```sql
   -- Check Raft status
   SELECT pgraft_status();
   
   -- Check logs
   SELECT pgraft_get_logs();
   ```

3. **Network connectivity issues**
   ```bash
   # Test network connectivity
   telnet localhost 7400
   telnet localhost 7401
   telnet localhost 7402
   ```

### Debug Mode

Enable debug logging for troubleshooting:

```sql
-- Enable debug logging
SELECT pgraft_set_debug(true);

-- Check debug logs
SELECT pgraft_get_logs();

-- Disable debug logging
SELECT pgraft_set_debug(false);
```

## Performance Considerations

### Memory Usage
- Shared memory: 1MB default (configurable)
- Raft log: Grows with cluster operations
- Network buffers: 256KB per connection

### Network Requirements
- Low latency network recommended
- Bandwidth: 1Mbps per node minimum
- Ports: 7400-7500 range (configurable)

### PostgreSQL Impact
- Minimal impact on PostgreSQL performance
- Shared memory access is atomic
- Network I/O is asynchronous

## Security

### Network Security
- Bind to specific interfaces only
- Use firewall rules to restrict access
- Consider VPN for remote clusters

### Authentication
- No built-in authentication (relies on network security)
- Use PostgreSQL authentication for SQL functions
- Consider SSL/TLS for network communication

## Monitoring

### Metrics Available
- Raft consensus metrics
- Network communication stats
- Leader election counts
- Health check results

### Integration with Prometheus
```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'pgraft'
    static_configs:
      - targets: ['localhost:9091']
```

## Best Practices

1. **Cluster Size**: Use odd numbers (3, 5, 7) for better consensus
2. **Network**: Ensure low latency between nodes
3. **Monitoring**: Set up comprehensive monitoring
4. **Backups**: Regular backups of Raft data
5. **Testing**: Test failover scenarios regularly

## API Reference

For complete API documentation, see [PGRaft API Reference](api-reference/pgraft-api.md).

---

**Next Steps**: Learn about [RAMD Daemon](ramd.md) for complete cluster management!
