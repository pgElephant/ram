# RAMCTRL - Professional Control Utility

Advanced command-line utility for managing PostgreSQL clusters with professional-grade features, comprehensive monitoring, and enterprise-ready reliability. Built with 100% PostgreSQL C coding standards and production-ready quality.

## Features

### Core Functionality
- **Professional CLI**: Advanced command-line interface with color-coded output
- **Cluster Management**: Complete cluster lifecycle management
- **Status Monitoring**: Real-time cluster and node status monitoring
- **Health Checks**: Comprehensive health monitoring and diagnostics
- **Replication Control**: Advanced replication management and control
- **Failover Operations**: Automated and manual failover capabilities

### Advanced Features
- **Multiple Output Formats**: JSON, table, and custom output formats
- **Interactive Mode**: Interactive cluster management interface
- **Configuration Management**: Dynamic configuration updates
- **Logging Integration**: Structured logging with multiple levels
- **Performance Monitoring**: System and cluster performance metrics
- **Docker Support**: Containerized deployment and management

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    RAMCTRL CLI                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   Command       │  │   HTTP Client   │  │   Output    │  │
│  │   Parser        │  │   (libcurl)     │  │   Formatter │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   Cluster       │  │   Node          │  │   Security  │  │
│  │   Management    │  │   Management    │  │   System    │  │
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
- Make and build tools

### Build and Install

```bash
# Navigate to ramctrl directory
cd ramctrl

# Clean and build
make clean
make

# Install the utility
sudo make install

# Verify installation
ramctrl --version
```

### Verify Installation

```bash
# Check version
ramctrl --version

# Check help
ramctrl --help

# Test connectivity
ramctrl status
```

## Configuration

### Configuration File

Create `~/.ramctrl.conf`:

```ini
[api]
host = 127.0.0.1
port = 8080
ssl_enabled = false
auth_token = your-secret-token
timeout = 30

[output]
format = table
color = true
verbose = false
log_level = info

[cluster]
default_cluster = pgraft_cluster
auto_connect = true
```

### Environment Variables

```bash
# Load configuration
source conf/environment.conf

# Set API-specific variables
export RAMCTRL_API_HOST=127.0.0.1
export RAMCTRL_API_PORT=8080
export RAMCTRL_AUTH_TOKEN=your-secret-token
```

## Usage

### Basic Commands

```bash
# Check cluster status
ramctrl status

# Get cluster health
ramctrl health

# Show cluster information
ramctrl info

# List all nodes
ramctrl nodes list
```

### Cluster Management

```bash
# Create a new cluster
ramctrl cluster create --name my-cluster --nodes 3

# Add node to cluster
ramctrl cluster add-node --node-id 2 --hostname node2 --port 5432

# Remove node from cluster
ramctrl cluster remove-node --node-id 2

# Destroy cluster
ramctrl cluster destroy --name my-cluster
```

### Node Management

```bash
# Start node
ramctrl node start --node-id 1

# Stop node
ramctrl node stop --node-id 1

# Restart node
ramctrl node restart --node-id 1

# Get node status
ramctrl node status --node-id 1
```

### Replication Management

```bash
# Start replication
ramctrl replication start --node-id 2

# Stop replication
ramctrl replication stop --node-id 2

# Get replication status
ramctrl replication status

# Promote replica to primary
ramctrl replication promote --node-id 2
```

### Backup and Restore

```bash
# Create backup
ramctrl backup create --name backup-2024-01-01

# List backups
ramctrl backup list

# Restore from backup
ramctrl backup restore --name backup-2024-01-01 --target-node 2
```

## Output Formats

### Table Format (Default)

```bash
# Default table output
ramctrl status

# Verbose table output
ramctrl status --verbose

# Custom table columns
ramctrl status --columns node_id,hostname,status,role
```

### JSON Format

```bash
# JSON output
ramctrl status --format json

# Pretty JSON output
ramctrl status --format json --pretty

# JSON with specific fields
ramctrl status --format json --fields node_id,hostname,status
```

### Custom Format

```bash
# Custom format with template
ramctrl status --format custom --template "Node {node_id}: {status}"

# CSV output
ramctrl status --format csv

# YAML output
ramctrl status --format yaml
```

## Interactive Mode

### Interactive Cluster Management

```bash
# Start interactive mode
ramctrl interactive

# Interactive commands
ramctrl> status
ramctrl> nodes list
ramctrl> cluster create --name test-cluster
ramctrl> help
ramctrl> exit
```

### Interactive Features
- **Command History**: Up/down arrow navigation
- **Auto-completion**: Tab completion for commands and options
- **Context Help**: Built-in help system
- **Command Aliases**: Short aliases for common commands

## Monitoring and Diagnostics

### Health Monitoring

```bash
# Comprehensive health check
ramctrl health --comprehensive

# Health check for specific node
ramctrl health --node-id 1

# Health check with metrics
ramctrl health --metrics
```

### Performance Monitoring

```bash
# Performance metrics
ramctrl metrics

# System metrics
ramctrl metrics --system

# PostgreSQL metrics
ramctrl metrics --postgresql
```

### Diagnostics

```bash
# Cluster diagnostics
ramctrl diagnose

# Node diagnostics
ramctrl diagnose --node-id 1

# Network diagnostics
ramctrl diagnose --network
```

## Development

### Building from Source

```bash
# Clone repository
git clone https://github.com/pgElephant/ram.git
cd ram/ramctrl

# Install dependencies
sudo apt-get install libcurl4-openssl-dev

# Build utility
make clean
make

# Run tests
make test
```

### Code Structure

```
ramctrl/
├── src/                    # C source files
│   ├── ramctrl_main.c     # Main entry point
│   ├── ramctrl_http.c     # HTTP client implementation
│   ├── ramctrl_cluster.c  # Cluster management
│   ├── ramctrl_node.c     # Node management
│   ├── ramctrl_show.c     # Status display
│   └── ramctrl_formation.c # Cluster formation
├── include/                # Header files
│   ├── ramctrl.h          # Main header
│   ├── ramctrl_http.h     # HTTP client definitions
│   └── ramctrl_cluster.h  # Cluster management
└── conf/                   # Configuration files
    └── ramctrl.conf       # Default configuration
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
python3 tests/test_ramctrl_integration.py

# Run with verbose output
python3 tests/test_ramctrl_integration.py -v
```

### CLI Tests

```bash
# Test command-line interface
python3 tests/test_ramctrl_cli.py

# Test output formats
python3 tests/test_ramctrl_output.py
```

## Security

### Security Features
- **Token Authentication**: Secure API access with bearer tokens
- **SSL/TLS Support**: Encrypted communications
- **Input Validation**: Comprehensive input sanitization
- **Secure Configuration**: Encrypted configuration storage
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

#### Connection Issues
```bash
# Check API connectivity
ramctrl status --verbose

# Test API endpoint
curl -v http://localhost:8080/api/v1/health

# Check authentication
ramctrl status --debug
```

#### Command Issues
```bash
# Check command syntax
ramctrl --help

# Check specific command help
ramctrl cluster --help

# Enable debug mode
ramctrl status --debug
```

#### Output Issues
```bash
# Check output format
ramctrl status --format json

# Check verbose output
ramctrl status --verbose

# Check log output
ramctrl status --log-level debug
```

### Debug Mode

Enable debug logging:

```bash
# Debug mode
ramctrl status --debug

# Verbose debug mode
ramctrl status --debug --verbose

# Log to file
ramctrl status --debug --log-file /tmp/ramctrl-debug.log
```

## Additional Resources

- [Command Reference](doc/api-reference/ramctrl-commands.md)
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

**RAMCTRL: Professional PostgreSQL cluster management made simple.** 
