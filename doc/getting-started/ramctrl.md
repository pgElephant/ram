# RAMCTRL CLI Guide

RAMCTRL is the command-line interface for managing RAM PostgreSQL clusters. It provides a comprehensive set of commands for cluster operations, monitoring, and administration.

## Overview

RAMCTRL provides:
- **Cluster Management**: Create, destroy, start, stop clusters
- **Node Operations**: Manage individual nodes
- **Monitoring**: Real-time cluster monitoring
- **Backup Management**: Backup and restore operations
- **Configuration**: Cluster configuration management

## Installation

### Prerequisites

- RAMD daemon running
- libcurl development libraries
- JSON parsing libraries (jansson)

### Build and Install

```bash
# Navigate to ramctrl directory
cd ramctrl

# Build the CLI
make

# Install the CLI
make install

# Install configuration
cp conf/ramctrl.conf /etc/ramctrl/
```

## Configuration

RAMCTRL uses the configuration file `conf/ramctrl.conf`:

### Basic Configuration

```ini
# Connection settings
default_host = localhost
default_port = 8008
connection_timeout = 30
request_timeout = 60

# Output formatting
default_output_format = table
colored_output = true
verbose_output = false

# Authentication
auth_method = none
default_auth_token = 
```

### Advanced Configuration

```ini
# Security
verify_ssl = true
ssl_cert_file = /etc/ssl/certs/ramctrl.crt
ssl_key_file = /etc/ssl/private/ramctrl.key

# Monitoring
monitoring_enabled = false
monitoring_interval = 10
monitoring_output_format = table

# Development
development_mode = false
profiling_enabled = false
```

## Usage

### Basic Commands

```bash
# Show help
./ramctrl --help

# Show version
./ramctrl --version

# Show configuration
./ramctrl config show

# Validate configuration
./ramctrl config validate
```

### Cluster Management

#### Create Cluster

```bash
# Create 3-node cluster
./ramctrl cluster create --num-nodes=3

# Create cluster with specific configuration
./ramctrl cluster create --config=conf/cluster.json

# Create cluster with custom settings
./ramctrl cluster create \
  --num-nodes=5 \
  --primary-port=5432 \
  --replica-ports=5433,5434,5435,5436 \
  --data-dir=/var/lib/postgresql \
  --log-dir=/var/log/postgresql
```

#### Cluster Status

```bash
# Show cluster status
./ramctrl cluster status

# Show detailed cluster information
./ramctrl cluster info

# Show cluster health
./ramctrl cluster health

# Show cluster metrics
./ramctrl cluster metrics
```

#### Cluster Operations

```bash
# Start cluster
./ramctrl cluster start

# Stop cluster
./ramctrl cluster stop

# Restart cluster
./ramctrl cluster restart

# Destroy cluster
./ramctrl cluster destroy

# Destroy specific nodes
./ramctrl cluster destroy --nodes=replica1,replica2
```

### Node Management

#### Node Operations

```bash
# List all nodes
./ramctrl nodes list

# Show node information
./ramctrl nodes show --node=primary

# Start node
./ramctrl nodes start --node=replica1

# Stop node
./ramctrl nodes stop --node=replica1

# Restart node
./ramctrl nodes restart --node=replica1
```

#### Node Status

```bash
# Show node status
./ramctrl nodes status --node=primary

# Show node health
./ramctrl nodes health --node=primary

# Show node metrics
./ramctrl nodes metrics --node=primary

# Show node logs
./ramctrl nodes logs --node=primary
```

### Failover Operations

```bash
# Trigger failover
./ramctrl cluster failover

# Promote node to primary
./ramctrl cluster promote --node=replica1

# Demote primary to replica
./ramctrl cluster demote --node=primary

# Force election
./ramctrl cluster force-election
```

### Backup Management

#### Backup Operations

```bash
# Create backup
./ramctrl backup create --name=backup_001

# Create full backup
./ramctrl backup create --name=backup_001 --type=full

# Create incremental backup
./ramctrl backup create --name=backup_001 --type=incremental

# List backups
./ramctrl backup list

# Show backup information
./ramctrl backup show --name=backup_001
```

#### Restore Operations

```bash
# Restore backup
./ramctrl backup restore --name=backup_001

# Restore to specific node
./ramctrl backup restore --name=backup_001 --target-node=replica1

# Restore to specific time
./ramctrl backup restore --name=backup_001 --time="2024-01-01 12:00:00"
```

### Monitoring

#### Real-time Monitoring

```bash
# Start monitoring
./ramctrl monitor

# Monitor with specific interval
./ramctrl monitor --interval=5

# Monitor specific nodes
./ramctrl monitor --nodes=primary,replica1

# Monitor with custom output format
./ramctrl monitor --format=json
```

#### Metrics and Logs

```bash
# Show cluster metrics
./ramctrl metrics show

# Show node metrics
./ramctrl metrics show --node=primary

# Show cluster logs
./ramctrl logs show

# Show node logs
./ramctrl logs show --node=primary

# Follow logs
./ramctrl logs follow --node=primary
```

### Configuration Management

#### Configuration Operations

```bash
# Show current configuration
./ramctrl config show

# Show specific configuration section
./ramctrl config show --section=cluster

# Update configuration
./ramctrl config set --key=cluster_size --value=5

# Reload configuration
./ramctrl config reload

# Validate configuration
./ramctrl config validate
```

#### Node Configuration

```bash
# Show node configuration
./ramctrl nodes config --node=primary

# Update node configuration
./ramctrl nodes config --node=primary --key=port --value=5432

# Apply configuration to all nodes
./ramctrl nodes config --all --key=shared_buffers --value=256MB
```

## Output Formats

### Table Format (Default)

```bash
# Table output
./ramctrl cluster status
```

Output:
```
+--------+----------+--------+---------+----------+
| Node   | Hostname | Port   | Role    | Status   |
+--------+----------+--------+---------+----------+
| 1      | localhost| 5432   | primary | running  |
| 2      | localhost| 5433   | replica | running  |
| 3      | localhost| 5434   | replica | running  |
+--------+----------+--------+---------+----------+
```

### JSON Format

```bash
# JSON output
./ramctrl cluster status --format=json
```

Output:
```json
{
  "status": "success",
  "data": {
    "cluster_name": "ram_cluster",
    "nodes": [
      {
        "id": 1,
        "hostname": "localhost",
        "port": 5432,
        "role": "primary",
        "status": "running"
      }
    ]
  }
}
```

### YAML Format

```bash
# YAML output
./ramctrl cluster status --format=yaml
```

Output:
```yaml
status: success
data:
  cluster_name: ram_cluster
  nodes:
    - id: 1
      hostname: localhost
      port: 5432
      role: primary
      status: running
```

## Command Reference

### Global Options

| Option | Description | Default |
|--------|-------------|---------|
| `--config` | Configuration file path | `ramctrl.conf` |
| `--host` | RAMD server hostname | `localhost` |
| `--port` | RAMD server port | `8008` |
| `--format` | Output format (table, json, yaml, text) | `table` |
| `--verbose` | Verbose output | `false` |
| `--debug` | Debug output | `false` |
| `--timeout` | Request timeout in seconds | `60` |
| `--help` | Show help | - |
| `--version` | Show version | - |

### Cluster Commands

| Command | Description |
|---------|-------------|
| `cluster create` | Create new cluster |
| `cluster destroy` | Destroy cluster |
| `cluster start` | Start cluster |
| `cluster stop` | Stop cluster |
| `cluster restart` | Restart cluster |
| `cluster status` | Show cluster status |
| `cluster info` | Show cluster information |
| `cluster health` | Show cluster health |
| `cluster metrics` | Show cluster metrics |
| `cluster failover` | Trigger failover |
| `cluster promote` | Promote node to primary |
| `cluster demote` | Demote primary to replica |

### Node Commands

| Command | Description |
|---------|-------------|
| `nodes list` | List all nodes |
| `nodes show` | Show node information |
| `nodes start` | Start node |
| `nodes stop` | Stop node |
| `nodes restart` | Restart node |
| `nodes status` | Show node status |
| `nodes health` | Show node health |
| `nodes metrics` | Show node metrics |
| `nodes logs` | Show node logs |
| `nodes config` | Manage node configuration |

### Backup Commands

| Command | Description |
|---------|-------------|
| `backup create` | Create backup |
| `backup list` | List backups |
| `backup show` | Show backup information |
| `backup restore` | Restore backup |
| `backup delete` | Delete backup |

### Monitoring Commands

| Command | Description |
|---------|-------------|
| `monitor` | Start real-time monitoring |
| `metrics show` | Show metrics |
| `logs show` | Show logs |
| `logs follow` | Follow logs |

## Examples

### Complete Cluster Setup

```bash
# 1. Create 3-node cluster
./ramctrl cluster create --num-nodes=3

# 2. Check cluster status
./ramctrl cluster status

# 3. Start monitoring
./ramctrl monitor

# 4. Create backup
./ramctrl backup create --name=initial_backup

# 5. Test failover
./ramctrl cluster failover

# 6. Check new primary
./ramctrl cluster status
```

### Production Deployment

```bash
# 1. Create production cluster
./ramctrl cluster create \
  --num-nodes=5 \
  --primary-port=5432 \
  --replica-ports=5433,5434,5435,5436 \
  --data-dir=/var/lib/postgresql \
  --log-dir=/var/log/postgresql \
  --config=conf/production.json

# 2. Configure monitoring
./ramctrl config set --key=monitoring_enabled --value=true
./ramctrl config set --key=monitoring_interval --value=5

# 3. Start monitoring
./ramctrl monitor --format=json > cluster_monitor.log &

# 4. Create scheduled backup
./ramctrl backup create --name=daily_backup --schedule="0 2 * * *"
```

### Troubleshooting

```bash
# 1. Check cluster health
./ramctrl cluster health

# 2. Check specific node
./ramctrl nodes health --node=primary

# 3. Check logs
./ramctrl logs show --node=primary --lines=100

# 4. Check metrics
./ramctrl metrics show --node=primary

# 5. Restart problematic node
./ramctrl nodes restart --node=replica1
```

## Integration

### With RAMD

RAMCTRL communicates with RAMD via HTTP API:

```bash
# RAMCTRL calls RAMD API
./ramctrl cluster status
# Internally: GET http://localhost:8008/api/v1/cluster/status
```

### With Monitoring Systems

```bash
# Export metrics for Prometheus
./ramctrl metrics show --format=json | jq '.data' > metrics.json

# Send to monitoring system
curl -X POST http://monitoring.example.com/api/metrics \
  -H "Content-Type: application/json" \
  -d @metrics.json
```

### With CI/CD

```bash
#!/bin/bash
# CI/CD script example

# Deploy cluster
./ramctrl cluster create --num-nodes=3

# Wait for cluster to be ready
while ! ./ramctrl cluster health > /dev/null 2>&1; do
  sleep 10
done

# Run tests
./ramctrl cluster status
./ramctrl backup create --name=test_backup

# Cleanup
./ramctrl cluster destroy
```

## Troubleshooting

### Common Issues

1. **Connection refused**
   ```bash
   # Check if RAMD is running
   curl http://localhost:8008/api/v1/health
   
   # Check configuration
   ./ramctrl config show
   ```

2. **Authentication failed**
   ```bash
   # Check authentication settings
   ./ramctrl config show --section=auth
   
   # Update authentication token
   ./ramctrl config set --key=default_auth_token --value=your-token
   ```

3. **Command not found**
   ```bash
   # Check if ramctrl is in PATH
   which ramctrl
   
   # Add to PATH
   export PATH=$PATH:/usr/local/bin
   ```

### Debug Mode

Enable debug mode for detailed output:

```bash
# Enable debug output
./ramctrl --debug cluster status

# Enable verbose output
./ramctrl --verbose cluster status

# Enable both
./ramctrl --debug --verbose cluster status
```

## Best Practices

1. **Configuration Management**
   - Use version control for configuration files
   - Use environment-specific configurations
   - Validate configuration before deployment

2. **Monitoring**
   - Set up continuous monitoring
   - Use appropriate output formats for automation
   - Monitor both cluster and individual nodes

3. **Backup Strategy**
   - Regular automated backups
   - Test restore procedures
   - Document backup policies

4. **Security**
   - Use authentication tokens
   - Enable SSL/TLS
   - Restrict network access

## API Reference

For complete API documentation, see [RAMCTRL API Reference](api-reference/ramctrl-api.md).

---

**Next Steps**: Learn about [Production Deployment](deployment/production.md)!
