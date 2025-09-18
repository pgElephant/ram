# ramctrl - Cluster Control Utility

A command-line utility for managing PostgreSQL clusters with distributed consensus capabilities.

## Features

- **Cluster Management**: Add, remove, and manage cluster nodes
- **Status Monitoring**: Real-time cluster health and status monitoring
- **Replication Control**: Manage synchronous and asynchronous replication
- **Failover Operations**: Manual and automatic failover management
- **Configuration Management**: Update cluster configuration on-the-fly
- **Maintenance Operations**: Database maintenance and cleanup tasks

## Installation

```bash
# Build from source
cd ramctrl
make clean
make
sudo make install
```

## Usage

### Basic Commands

```bash
# Show cluster status
ramctrl status

# Add a node to the cluster
ramctrl add-node --host=node2.example.com --port=5432

# Remove a node from the cluster
ramctrl remove-node --node-id=2

# Show cluster health
ramctrl health

# Show replication status
ramctrl replication-status
```

### Advanced Operations

```bash
# Force failover
ramctrl failover --target-node=2

# Update cluster configuration
ramctrl update-config --config-file=cluster.conf

# Perform maintenance
ramctrl maintenance --operation=vacuum

# Watch cluster events
ramctrl watch --follow
```

## Configuration

The utility can be configured via:
- Command-line arguments
- Configuration file (`~/.ramctrl.conf`)
- Environment variables

## API Reference

### Commands

| Command | Description | Options |
|---------|-------------|---------|
| `status` | Show cluster status | `--json`, `--verbose` |
| `add-node` | Add node to cluster | `--host`, `--port`, `--priority` |
| `remove-node` | Remove node from cluster | `--node-id`, `--force` |
| `health` | Show cluster health | `--detailed`, `--json` |
| `failover` | Trigger failover | `--target-node`, `--force` |
| `replication-status` | Show replication status | `--node-id`, `--json` |
| `maintenance` | Run maintenance operations | `--operation`, `--dry-run` |
| `watch` | Watch cluster events | `--follow`, `--filter` |

### Exit Codes

- `0`: Success
- `1`: General error
- `2`: Configuration error
- `3`: Connection error
- `4`: Permission denied

## Examples

### Setting up a 3-node cluster

```bash
# Initialize the cluster
ramctrl init --primary-node=1 --host=node1.example.com

# Add second node
ramctrl add-node --host=node2.example.com --port=5432

# Add third node
ramctrl add-node --host=node3.example.com --port=5432

# Verify cluster status
ramctrl status
```

### Monitoring cluster health

```bash
# Check overall health
ramctrl health

# Get detailed status
ramctrl status --verbose

# Watch for changes
ramctrl watch --follow
```

### Handling failover

```bash
# Check current leader
ramctrl status | grep leader

# Force failover to specific node
ramctrl failover --target-node=2

# Verify new leader
ramctrl status
```

## Troubleshooting

### Common Issues

1. **Connection refused**: Check if ramd daemon is running
2. **Permission denied**: Ensure proper user permissions
3. **Node not found**: Verify node ID and cluster membership

### Debug Mode

```bash
# Enable debug logging
export RAMCTRL_DEBUG=1
ramctrl status --verbose
```

## Integration

ramctrl works with:
- **ramd**: Cluster management daemon
- **pgraft**: PostgreSQL extension for consensus
- **PostgreSQL**: Database instances

## License

MIT License - see LICENSE file for details.
