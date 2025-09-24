# RAMD - Cluster Management Daemon

Enterprise-grade daemon for managing PostgreSQL clusters with automatic failover, health monitoring, and comprehensive REST API. Built with 100% PostgreSQL C coding standards and production-ready reliability.

## Features

### Core Functionality
- **Cluster Management**: Automated cluster formation and node management
- **Automatic Failover**: Sub-second failover detection and recovery
- **Health Monitoring**: Real-time health checks and status reporting
- **REST API**: Comprehensive HTTP API for external integration
- **Configuration Management**: Dynamic configuration with environment variables
- **Security**: Token-based authentication and SSL/TLS support

### Advanced Features
- **Prometheus Metrics**: Built-in metrics collection and export
- **Base Backup**: Automated PostgreSQL base backup and restore
- **Replication Management**: Streaming replication control and monitoring
- **Logging System**: Structured logging with multiple levels
- **Performance Monitoring**: System and PostgreSQL performance metrics
- **Docker Support**: Containerized deployment with Docker Compose

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    RAMD Daemon                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   HTTP API      │  │   Cluster Mgmt  │  │  Monitoring │  │
│  │   (Port 8080)   │  │                 │  │   System    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   PGRaft        │  │   PostgreSQL    │  │   Security  │  │
│  │   Integration   │  │   Management    │  │   System    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   Configuration │  │   Logging       │  │   Metrics   │  │
│  │   Management    │  │   System        │  │  Collection │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Installation

### Prerequisites
- PostgreSQL 12+ with development headers
- C compiler (GCC or Clang)
- libcurl development libraries
- Jansson JSON library
- Make and build tools

### Build and Install

```bash
# Navigate to ramd directory
cd ramd

# Clean and build
make clean
make

# Install the daemon
sudo make install

# Start the daemon
sudo ramd start
```

### Verify Installation

```bash
# Check if daemon is running
ps aux | grep ramd

# Check API health
curl http://localhost:8080/api/v1/health

# Check cluster status
curl http://localhost:8080/api/v1/cluster/status
```

## Configuration

### Configuration File

Create `/etc/ramd.conf`:

```ini
[cluster]
node_id = 1
cluster_name = pgraft_cluster
primary_host = 127.0.0.1
primary_port = 5432
replica_hosts = 127.0.0.1:5433,127.0.0.1:5434
data_directory = /var/lib/postgresql/data
log_level = info

[api]
host = 0.0.0.0
port = 8080
ssl_enabled = false
auth_token = your-secret-token

[postgresql]
host = 127.0.0.1
port = 5432
user = postgres
password = postgres
database = postgres
ssl_mode = prefer

[monitoring]
prometheus_enabled = true
prometheus_port = 9090
metrics_interval = 30

[security]
rate_limit = 100
max_connections = 1000
blocked_ips = 
```

### Environment Variables

```bash
# Load configuration
source conf/environment.conf

# Set cluster-specific variables
export RAMD_NODE_ID=1
export RAMD_CLUSTER_NAME=pgraft_cluster
export RAMD_API_PORT=8080
export RAMD_LOG_LEVEL=info
```

## Usage

### Starting the Daemon

```bash
# Start with default configuration
ramd start

# Start with custom config
ramd start --config /path/to/ramd.conf

# Start in foreground (debug mode)
ramd start --foreground --log-level debug
```

### Stopping the Daemon

```bash
# Stop gracefully
ramd stop

# Force stop
ramd stop --force

# Restart
ramd restart
```

### Status and Health

```bash
# Check daemon status
ramd status

# Check cluster health
ramd health

# Get detailed information
ramd info
```

## REST API

### Authentication

All API endpoints require authentication via token:

```bash
curl -H "Authorization: Bearer your-secret-token" \
     http://localhost:8080/api/v1/health
```

### Core Endpoints

#### Health and Status
```bash
# Health check
GET /api/v1/health

# Cluster status
GET /api/v1/cluster/status

# Node information
GET /api/v1/node/info
```

#### Cluster Management
```bash
# Add node to cluster
POST /api/v1/cluster/add-node
{
  "node_id": 2,
  "hostname": "node2",
  "address": "192.168.1.2",
  "port": 5432
}

# Remove node from cluster
DELETE /api/v1/cluster/remove-node
{
  "node_id": 2
}

# Get cluster health
GET /api/v1/cluster/health
```

#### Replication Management
```bash
# Start replication
POST /api/v1/replication/start
{
  "node_id": 2
}

# Stop replication
POST /api/v1/replication/stop
{
  "node_id": 2
}

# Get replication status
GET /api/v1/replication/status
```

#### Backup and Restore
```bash
# Create base backup
POST /api/v1/backup/create
{
  "backup_name": "backup_2024_01_01",
  "node_id": 1
}

# List backups
GET /api/v1/backup/list

# Restore from backup
POST /api/v1/backup/restore
{
  "backup_name": "backup_2024_01_01",
  "target_node": 2
}
```

### Response Format

All API responses follow this format:

```json
{
  "success": true,
  "data": { ... },
  "message": "Operation completed successfully",
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## Monitoring & Metrics

### Prometheus Metrics

RAMD exposes metrics on port 9090:

```
# Cluster metrics
ramd_cluster_nodes_total
ramd_cluster_healthy_nodes_total
ramd_cluster_leader_changes_total

# API metrics
ramd_api_requests_total
ramd_api_request_duration_seconds
ramd_api_errors_total

# PostgreSQL metrics
ramd_postgresql_connections_total
ramd_postgresql_queries_total
ramd_postgresql_replication_lag_seconds
```

### Grafana Dashboard

Import the provided Grafana dashboard for comprehensive monitoring:

```bash
# Start monitoring stack
docker-compose -f docker/docker-compose-monitoring.yml up -d

# Access Grafana
open http://localhost:3000
```

### Logging

RAMD provides structured logging:

```json
{
  "timestamp": "2024-01-01T12:00:00Z",
  "level": "INFO",
  "component": "cluster",
  "message": "Node added to cluster",
  "node_id": 2,
  "hostname": "node2"
}
```

## Development

### Building from Source

```bash
# Clone repository
git clone https://github.com/pgElephant/ram.git
cd ram/ramd

# Install dependencies
sudo apt-get install libcurl4-openssl-dev libjansson-dev

# Build daemon
make clean
make

# Run tests
make test
```

### Code Structure

```
ramd/
├── src/                    # C source files
│   ├── ramd_main.c        # Main daemon entry point
│   ├── ramd_http_api.c    # HTTP API implementation
│   ├── ramd_cluster.c     # Cluster management
│   ├── ramd_pgraft.c      # PGRaft integration
│   ├── ramd_security.c    # Security and authentication
│   ├── ramd_metrics.c     # Metrics collection
│   └── ramd_prometheus.c  # Prometheus integration
├── include/                # Header files
│   ├── ramd.h             # Main header
│   ├── ramd_http_api.h    # HTTP API definitions
│   └── ramd_cluster.h     # Cluster management
└── conf/                   # Configuration files
    └── ramd.conf          # Default configuration
```

## Testing

### Unit Tests

```bash
# Run unit tests
make test

# Run specific test
make test TEST=test_cluster_management
```

### Integration Tests

```bash
# Run integration tests
python3 tests/test_ramd_integration.py

# Run with verbose output
python3 tests/test_ramd_integration.py -v
```

### API Tests

```bash
# Test API endpoints
python3 tests/test_ramd_api.py

# Test authentication
python3 tests/test_ramd_auth.py
```

## Security

### Security Features
- **Token Authentication**: Secure API access with bearer tokens
- **Rate Limiting**: Protection against abuse and DoS attacks
- **Input Validation**: Comprehensive input sanitization
- **SSL/TLS Support**: Encrypted communications
- **Access Control**: Role-based access to API endpoints
- **Audit Logging**: Complete audit trail of all operations

### Security Best Practices
- Use strong, unique authentication tokens
- Enable SSL/TLS in production
- Regularly rotate authentication tokens
- Monitor for suspicious activity
- Keep dependencies updated
- Follow security guidelines

## Troubleshooting

### Common Issues

#### Daemon Not Starting
```bash
# Check configuration
ramd config validate

# Check logs
tail -f /var/log/ramd/ramd.log

# Check for port conflicts
netstat -tlnp | grep 8080
```

#### API Not Responding
```bash
# Check daemon status
ramd status

# Test API connectivity
curl -v http://localhost:8080/api/v1/health

# Check authentication
curl -H "Authorization: Bearer your-token" \
     http://localhost:8080/api/v1/health
```

#### Cluster Issues
```bash
# Check cluster status
curl http://localhost:8080/api/v1/cluster/status

# Check node health
curl http://localhost:8080/api/v1/node/health

# Check PGRaft status
psql -d postgres -c "SELECT pgraft_get_cluster_status();"
```

### Debug Mode

Enable debug logging:

```bash
# Start with debug logging
ramd start --log-level debug

# Check debug logs
tail -f /var/log/ramd/ramd-debug.log
```

## Additional Resources

- [REST API Documentation](doc/api-reference/rest-api.md)
- [Configuration Guide](doc/configuration/)
- [Deployment Guide](doc/deployment/)
- [Troubleshooting Guide](doc/troubleshooting/)

## Contributing

1. Fork the repository
2. Create a feature branch
3. Follow PostgreSQL C coding standards
4. Add tests for new functionality
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](../../LICENSE) file for details.

---

**RAMD: Enterprise-grade PostgreSQL cluster management.** 
