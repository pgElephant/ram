# RAMD Daemon Guide

RAMD (Raft-based Auto-failover Manager Daemon) is the core high availability daemon that manages PostgreSQL cluster operations, health monitoring, and automatic failover.

## Overview

RAMD provides:
- **Health Monitoring**: Continuous monitoring of PostgreSQL nodes
- **Automatic Failover**: Seamless primary/replica transitions
- **HTTP API**: RESTful API for cluster management
- **Metrics Collection**: Prometheus-compatible metrics
- **Configuration Management**: Dynamic configuration updates

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
└─────────┬───────┘    └─────────┬───────┘    └─────────┬───────┘
          │                      │                      │
          └──────────────────────┼──────────────────────┘
                                 │
                    ┌─────────────────┐
                    │     RAMD        │
                    │   (Daemon)      │
                    │                 │
                    │  ┌───────────┐  │
                    │  │ HTTP API  │  │◄─── RAMCTRL CLI
                    │  └───────────┘  │
                    │  ┌───────────┐  │
                    │  │ Monitoring│  │
                    │  └───────────┘  │
                    │  ┌───────────┐  │
                    │  │  Metrics  │  │◄─── Prometheus
                    │  └───────────┘  │
                    └─────────────────┘
```

## Installation

### Prerequisites

- PostgreSQL 17 or later with PGRaft extension
- C compiler and build tools
- libpq development libraries

### Build and Install

```bash
# Navigate to ramd directory
cd ramd

# Build the daemon
make

# Install the daemon
make install

# Install configuration
cp conf/ramd.conf /etc/ramd/
```

## Configuration

RAMD uses the configuration file `conf/ramd.conf`:

### Basic Configuration

```ini
# Node identification
node_id = 1
hostname = localhost
cluster_name = ram_cluster
cluster_size = 3

# PostgreSQL settings
postgresql_port = 5432
postgresql_data_dir = /tmp/postgres_data
postgresql_bin_dir = /usr/local/pgsql.17/bin
database_name = postgres
database_user = postgres

# HTTP API
http_api_enabled = true
http_bind_address = 127.0.0.1
http_port = 8008

# Monitoring
monitor_interval_ms = 5000
auto_failover_enabled = true
```

### Advanced Configuration

```ini
# Security
ssl_enabled = true
ssl_cert_file = /etc/ssl/certs/ramd.crt
ssl_key_file = /etc/ssl/private/ramd.key
rate_limiting_enabled = true
rate_limit_requests_per_minute = 100

# Backup
backup_tool = pgbackrest
backup_repository = /var/lib/ramd/backups
backup_retention_days = 7

# Prometheus metrics
prometheus_enabled = true
prometheus_port = 9090
prometheus_path = /metrics
```

## Usage

### Starting RAMD

```bash
# Start with configuration file
./ramd --config=conf/ramd.conf

# Start with specific log level
./ramd --config=conf/ramd.conf --log-level=debug

# Start as daemon
./ramd --config=conf/ramd.conf --daemonize

# Start with custom PID file
./ramd --config=conf/ramd.conf --pid-file=/var/run/ramd.pid
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--config` | Configuration file path | `ramd.conf` |
| `--log-level` | Log level (debug, info, warning, error) | `info` |
| `--daemonize` | Run as daemon | `false` |
| `--pid-file` | PID file path | `/tmp/ramd.pid` |
| `--help` | Show help message | - |
| `--version` | Show version information | - |

## HTTP API

RAMD provides a comprehensive REST API for cluster management:

### Health Endpoints

```bash
# Check daemon health
curl http://localhost:8008/api/v1/health

# Check cluster health
curl http://localhost:8008/api/v1/cluster/health

# Get cluster status
curl http://localhost:8008/api/v1/cluster/status
```

### Node Management

```bash
# List all nodes
curl http://localhost:8008/api/v1/nodes

# Get node information
curl http://localhost:8008/api/v1/nodes/1

# Start node
curl -X POST http://localhost:8008/api/v1/nodes/1/start

# Stop node
curl -X POST http://localhost:8008/api/v1/nodes/1/stop
```

### Cluster Operations

```bash
# Create cluster
curl -X POST http://localhost:8008/api/v1/cluster/create \
  -H "Content-Type: application/json" \
  -d '{"num_nodes": 3}'

# Destroy cluster
curl -X POST http://localhost:8008/api/v1/cluster/destroy

# Failover cluster
curl -X POST http://localhost:8008/api/v1/cluster/failover
```

### Backup Operations

```bash
# Start backup
curl -X POST http://localhost:8008/api/v1/backup/start \
  -H "Content-Type: application/json" \
  -d '{"name": "backup_001", "type": "full"}'

# List backups
curl http://localhost:8008/api/v1/backup/list

# Restore backup
curl -X POST http://localhost:8008/api/v1/backup/restore \
  -H "Content-Type: application/json" \
  -d '{"name": "backup_001", "target_node": "replica1"}'
```

## Monitoring

### Health Monitoring

RAMD continuously monitors:
- PostgreSQL server status
- Replication lag
- Disk space usage
- Memory usage
- Network connectivity
- Raft consensus health

### Metrics

RAMD exposes Prometheus-compatible metrics:

```bash
# Get metrics
curl http://localhost:8008/metrics

# Get Prometheus metrics
curl http://localhost:9090/metrics
```

Key metrics include:
- `ramd_cluster_nodes_total` - Total number of nodes
- `ramd_cluster_healthy_nodes` - Number of healthy nodes
- `ramd_cluster_primary_node` - Current primary node ID
- `ramd_replication_lag_seconds` - Replication lag in seconds
- `ramd_failover_count_total` - Total failover count
- `ramd_http_requests_total` - HTTP request count
- `ramd_http_request_duration_seconds` - HTTP request duration

### Logging

RAMD provides structured logging:

```bash
# Enable debug logging
./ramd --config=conf/ramd.conf --log-level=debug

# Log to file
echo "log_file = /var/log/ramd/ramd.log" >> conf/ramd.conf

# Log to syslog
echo "log_to_syslog = true" >> conf/ramd.conf
```

Log levels:
- `debug` - Detailed debugging information
- `info` - General information
- `warning` - Warning messages
- `error` - Error messages

## Integration

### With PGRaft

RAMD integrates with PGRaft via shared memory:

```c
// RAMD connects to PGRaft shared memory
pgraft_shmem = ShmemInitStruct("pgraft", sizeof(pgraft_shmem_t), &found);
```

### With RAMCTRL

RAMD provides HTTP API for RAMCTRL:

```bash
# RAMCTRL communicates with RAMD via HTTP
./ramctrl cluster status
# Internally calls: GET http://localhost:8008/api/v1/cluster/status
```

### With Prometheus

RAMD exposes metrics for Prometheus:

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'ramd'
    static_configs:
      - targets: ['localhost:9090']
```

## Troubleshooting

### Common Issues

1. **RAMD won't start**
   ```bash
   # Check configuration
   ./ramd --config=conf/ramd.conf --validate
   
   # Check logs
   tail -f /var/log/ramd/ramd.log
   ```

2. **HTTP API not responding**
   ```bash
   # Check if port is in use
   netstat -tlnp | grep 8008
   
   # Check firewall
   iptables -L | grep 8008
   ```

3. **Health checks failing**
   ```bash
   # Check PostgreSQL connection
   psql -h localhost -p 5432 -U postgres -d postgres
   
   # Check PGRaft status
   psql -h localhost -p 5432 -U postgres -d postgres -c "SELECT pgraft_status();"
   ```

### Debug Mode

Enable debug mode for detailed logging:

```bash
# Start with debug logging
./ramd --config=conf/ramd.conf --log-level=debug

# Enable debug in configuration
echo "debug_mode = true" >> conf/ramd.conf
```

### Performance Tuning

```ini
# Increase connection pool
connection_pool_size = 20

# Adjust monitoring intervals
monitor_interval_ms = 3000
health_check_timeout_ms = 5000

# Optimize memory usage
memory_limit_mb = 1024
```

## Security

### Authentication

Enable HTTP authentication:

```ini
# Enable authentication
http_auth_enabled = true
http_auth_token = your-secret-token

# Use in API calls
curl -H "Authorization: Bearer your-secret-token" \
  http://localhost:8008/api/v1/cluster/status
```

### SSL/TLS

Enable SSL/TLS encryption:

```ini
# Enable SSL
ssl_enabled = true
ssl_cert_file = /etc/ssl/certs/ramd.crt
ssl_key_file = /etc/ssl/private/ramd.key
ssl_ca_file = /etc/ssl/certs/ca.crt
```

### Rate Limiting

Enable rate limiting:

```ini
# Enable rate limiting
rate_limiting_enabled = true
rate_limit_requests_per_minute = 100
```

## Best Practices

1. **Configuration Management**
   - Use version control for configuration files
   - Validate configuration before deployment
   - Use environment-specific configurations

2. **Monitoring**
   - Set up comprehensive monitoring
   - Configure alerting for critical events
   - Monitor both RAMD and PostgreSQL metrics

3. **Security**
   - Enable authentication and authorization
   - Use SSL/TLS for all communications
   - Implement proper network security

4. **Backup and Recovery**
   - Regular backup testing
   - Document recovery procedures
   - Test failover scenarios

## API Reference

For complete API documentation, see [RAMD API Reference](api-reference/ramd-api.md).

---

**Next Steps**: Learn about [RAMCTRL CLI](ramctrl.md) for cluster management!
