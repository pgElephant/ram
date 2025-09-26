# PostgreSQL pgraft Cluster Example

This directory contains a complete example of setting up and managing a three-node PostgreSQL cluster with pgraft consensus.

## Files

- `primary.conf` - PostgreSQL configuration for the primary node
- `replica1.conf` - PostgreSQL configuration for replica1 node  
- `replica2.conf` - PostgreSQL configuration for replica2 node
- `pgraft_cluster.py` - Modular cluster management script
- `README.md` - This documentation

## Quick Start

### Prerequisites

- PostgreSQL 15+ with pgraft extension installed
- Python 3.7+ with psycopg2
- Sufficient disk space in `/tmp/pgraft` (or custom directory)

### Initialize Cluster

```bash
# Make the script executable
chmod +x pgraft_cluster.py

# Initialize the three-node cluster
python pgraft_cluster.py --init
```

### Check Status

```bash
# Show cluster status
python pgraft_cluster.py --status
```

### Verify Health

```bash
# Verify cluster health and connectivity
python pgraft_cluster.py --verify
```

### Destroy Cluster

```bash
# Clean up and destroy cluster
python pgraft_cluster.py --destroy
```

## Cluster Architecture

The example creates a three-node cluster:

- **Primary Node**: Port 5432, pgraft port 7001, metrics port 9091
- **Replica1**: Port 5433, pgraft port 7002, metrics port 9092  
- **Replica2**: Port 5434, pgraft port 7003, metrics port 9093

## Configuration Details

### pgraft Settings

Each node is configured with:

- **Node ID**: Unique identifier for the node
- **Cluster ID**: Shared cluster identifier
- **Listen Address/Port**: Network configuration for pgraft consensus
- **Peers**: List of all cluster members
- **Consensus Parameters**: Election timeout, heartbeat interval, etc.
- **Performance Settings**: Batch size, compaction threshold
- **Monitoring**: Metrics and logging configuration

### PostgreSQL Settings

- **Streaming Replication**: Configured between primary and replicas
- **WAL Settings**: Optimized for replication
- **Connection Settings**: Tuned for cluster operation
- **Logging**: Comprehensive logging for debugging

## Script Features

The `pgraft_cluster.py` script provides:

### Modular Design

- **NodeConfig**: Configuration management for each node
- **PgraftClusterManager**: Main cluster management class
- **Error Handling**: Comprehensive error handling and logging
- **Signal Handling**: Graceful shutdown on interrupts

### Operations

- **--init**: Create and start the entire cluster
- **--verify**: Check cluster health and connectivity
- **--status**: Display detailed cluster status
- **--destroy**: Clean up all cluster resources

### Status Information

The status command shows:

- Cluster health (healthy/degraded/down)
- Node status (running/error)
- pgraft consensus state
- Replication lag for replicas
- Connection information

## Usage Examples

### Basic Operations

```bash
# Initialize cluster
python pgraft_cluster.py --init

# Check if everything is working
python pgraft_cluster.py --verify

# Monitor cluster status
python pgraft_cluster.py --status

# Clean up when done
python pgraft_cluster.py --destroy
```

### Custom Base Directory

```bash
# Use custom directory for cluster data
python pgraft_cluster.py --init --base-dir /opt/pgraft-cluster
```

### Testing Failover

```bash
# Start cluster
python pgraft_cluster.py --init

# Stop primary node (simulate failure)
pg_ctl -D /tmp/pgraft/primary stop

# Check status - should show degraded
python pgraft_cluster.py --status

# Restart primary
pg_ctl -D /tmp/pgraft/primary start

# Verify recovery
python pgraft_cluster.py --verify
```

## Troubleshooting

### Common Issues

1. **Port Conflicts**: Ensure ports 5432-5434 and 7001-7003 are available
2. **Permission Issues**: Ensure write access to the base directory
3. **pgraft Extension**: Verify pgraft is installed and available
4. **Python Dependencies**: Install psycopg2: `pip install psycopg2-binary`

### Logs

Check logs in:
- `/tmp/pgraft/{node}/postgresql.log` - PostgreSQL logs
- `/tmp/pgraft/{node}/pgraft.log` - pgraft consensus logs

### Debug Mode

For detailed debugging, modify the configuration files to set:
```
log_min_messages = debug1
pgraft.log_level = debug
```

## Advanced Configuration

### Custom pgraft Parameters

Edit the configuration files to adjust:

- `pgraft.election_timeout`: Leader election timeout (ms)
- `pgraft.heartbeat_interval`: Heartbeat frequency (ms)
- `pgraft.snapshot_interval`: Snapshot frequency (entries)
- `pgraft.batch_size`: Batch size for log entries

### Performance Tuning

- Adjust `shared_buffers` and `work_mem` in PostgreSQL config
- Modify `pgraft.batch_size` and `pgraft.max_batch_delay`
- Tune `checkpoint_timeout` and WAL settings

### Security

Enable authentication by setting:
```
pgraft.auth_enabled = true
pgraft.tls_enabled = true
```

## Integration with ramd

This example can be integrated with the ramd daemon for production use:

1. Use the configuration files as templates
2. Adjust paths and ports for production
3. Configure ramd to manage the cluster
4. Use ramctrl for cluster operations

## Support

For issues and questions:
- Check the logs for error messages
- Verify all prerequisites are met
- Ensure sufficient system resources
- Review the pgraft documentation
