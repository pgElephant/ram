# RAM: Resilient Adaptive Manager
A true PostgreSQL clustering solution powered by Raft consensus. Providing automated failover and high availability out of the box. Designed for seamless integration and robust operation, and built with performance and reliability as core principles.

[![CI](https://github.com/pgElephant/ram/workflows/C%2FC%2B%2B%20CI/badge.svg)](https://github.com/pgElephant/ram/actions/workflows/ci.yml)
[![Build](https://github.com/pgElephant/ram/workflows/Build%20Only/badge.svg)](https://github.com/pgElephant/ram/actions/workflows/build.yml)
[![Test](https://github.com/pgElephant/ram/workflows/Comprehensive%20Testing/badge.svg)](https://github.com/pgElephant/ram/actions/workflows/test.yml)
[![CodeQL](https://github.com/pgElephant/ram/workflows/CodeQL%20Analysis/badge.svg)](https://github.com/pgElephant/ram/actions/workflows/codeql.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-12%2B-blue.svg)](https://www.postgresql.org/)
[![C](https://img.shields.io/badge/C-99-orange.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)](https://github.com/pgElephant/ram)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/pgElephant/ram)
[![Code Quality](https://img.shields.io/badge/code%20quality-A-brightgreen.svg)](https://github.com/pgElephant/ram)

This project provides distributed consensus capabilities for PostgreSQL using custom Raft implementations. It consists of multiple components working together to provide high availability and consistency across PostgreSQL clusters.

## Quick Start
1. **Install pgraft extension**: Follow installation steps in [pgraft/README.md](pgraft/README.md)
2. **Start ramd daemon**: Follow startup guide in [ramd/README.md](ramd/README.md)
3. **Use ramctrl utility**: Follow usage instructions in [ramctrl/README.md](ramctrl/README.md)

## Project Status

| Badge | Description |
|-------|-------------|
| ![CI](https://github.com/pgElephant/ram/workflows/C%2FC%2B%2B%20CI/badge.svg) | Continuous Integration - Tests across PostgreSQL 12-17 |
| ![Build](https://github.com/pgElephant/ram/workflows/Build%20Only/badge.svg) | Build Status - Compilation verification |
| ![Test](https://github.com/pgElephant/ram/workflows/Comprehensive%20Testing/badge.svg) | Test Status - Comprehensive testing with memory analysis |
| ![CodeQL](https://github.com/pgElephant/ram/workflows/CodeQL%20Analysis/badge.svg) | Security Analysis - Automated security scanning |
| ![License](https://img.shields.io/badge/license-MIT-blue.svg) | MIT License |
| ![PostgreSQL](https://img.shields.io/badge/PostgreSQL-12%2B-blue.svg) | PostgreSQL 12+ Support |
| ![C](https://img.shields.io/badge/C-99-orange.svg) | C99 Standard |
| ![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg) | Linux & macOS Support |

## Architecture

```
                   ┌───────────────────────────┐
                   │       ramctrl CLI         │
                   │    (Control Utility)      │
                   └─────────────┬─────────────┘
                                 │ REST / SQL
                   ┌─────────────┴─────────────┐
                   │   PostgreSQL + pgraft     │
                   │   + ramd (PRIMARY)        │
                   │   LEADER (Node 1)         │
                   └─────────────┬─────────────┘
                                 │
          ┌──────────────────────┼──────────────────────┐
          │                                             │
          │    Physical               Physical          │
          │    Replication            Replication       │
          │                                             │
┌─────────┴───────┐                           ┌─────────┴───────┐
│   PostgreSQL    │                           │   PostgreSQL    │
│   + pgraft      │                           │   + pgraft      │
│   + ramd        │                           │   + ramd        │
│   (REPLICA)     │                           │   (REPLICA)     │
│   FOLLOWER      │                           │   FOLLOWER      │
│   (Node 2)      │                           │   (Node 3)      │
└─────────────────┘                           └─────────────────┘
```


## Components

### [pgraft](pgraft/README.md) - PostgreSQL Extension
**Pure C PostgreSQL extension providing distributed consensus**

- Custom Raft consensus algorithm
- High availability and automatic failover  
- Log replication across cluster nodes
- Background worker process
- Comprehensive monitoring and metrics
- PostgreSQL integration with custom functions

**[Read pgraft Documentation](pgraft/README.md)**

### [ramd](ramd/README.md) - Cluster Management Daemon
**Daemon process managing PostgreSQL cluster operations**

- Cluster node management
- Health monitoring and failover
- HTTP API for external integration
- Configuration management
- Logging and monitoring

**[Read ramd Documentation](ramd/README.md)**

### [ramctrl](ramctrl/README.md) - Control Utility
**Command-line utility for cluster management**

- Cluster management commands
- Status monitoring and health checks
- Replication control
- Failover operations
- Maintenance tasks

**[Read ramctrl Documentation](ramctrl/README.md)**

## Development

### Building All Components

```bash
# Build everything
make all

# Build individual components
make build-pgraft
make build-ramd
make build-ramctrl
```



## Setup

### Prerequisites

- PostgreSQL 12+ installed and running
- C compiler (GCC or Clang)
- Make and build tools
- Root or sudo access for system-level operations

### Installation Steps

#### 1. Install pgraft Extension

```bash
# Navigate to pgraft directory
cd pgraft

# Build and install the extension
make clean
make
sudo make install

# Create the extension in your database
psql -d your_database -c "CREATE EXTENSION pgraft;"
```

#### 2. Build and Install ramd Daemon

```bash
# Navigate to ramd directory
cd ramd

# Build the daemon
make clean
make

# Install ramd (copy to system path)
sudo cp ramd /usr/local/bin/
sudo chmod +x /usr/local/bin/ramd
```

#### 3. Build and Install ramctrl Utility

```bash
# Navigate to ramctrl directory
cd ramctrl

# Build the control utility
make clean
make

# Install ramctrl (copy to system path)
sudo cp ramctrl /usr/local/bin/
sudo chmod +x /usr/local/bin/ramctrl
```

### Configuration

#### pgraft Configuration

Configure pgraft through PostgreSQL GUC parameters:

```sql
-- Set pgraft configuration
ALTER SYSTEM SET pgraft.enabled = on;
ALTER SYSTEM SET pgraft.node_id = 1;
ALTER SYSTEM SET pgraft.cluster_addresses = 'node1:5432,node2:5432,node3:5432';
ALTER SYSTEM SET pgraft.heartbeat_interval = 1000;
ALTER SYSTEM SET pgraft.election_timeout = 5000;

-- Reload configuration
SELECT pg_reload_conf();
```

#### ramd Configuration

Create configuration file `/etc/ramd.conf`:

```ini
[cluster]
node_id = 1
primary_port = 5432
replica_ports = 5433,5434
data_directory = /var/lib/postgresql/data
log_level = info

[replication]
wal_level = replica
max_wal_senders = 10
max_replication_slots = 10
```

#### ramctrl Usage

```bash
# Start ramd daemon
sudo ramd start

# Check cluster status
ramctrl status

# Add node to cluster
ramctrl add-node --node-id 2 --address node2 --port 5432

# Remove node from cluster
ramctrl remove-node --node-id 3

# Check cluster health
ramctrl health
```

### Verification

```bash
# Verify pgraft extension is loaded
psql -d your_database -c "SELECT * FROM pg_extension WHERE extname = 'pgraft';"

# Check ramd is running
ps aux | grep ramd

# Test ramctrl connectivity
ramctrl status --verbose
```

## Documentation

Each component has its own comprehensive documentation:

- **[pgraft/README.md](pgraft/README.md)** - PostgreSQL extension documentation
- **[ramd/README.md](ramd/README.md)** - Cluster daemon documentation  
- **[ramctrl/README.md](ramctrl/README.md)** - Control utility documentation

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `make test`
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

- **Issues**: [GitHub Issues](https://github.com/pgElephant/ram/issues)
- **Discussions**: [GitHub Discussions](https://github.com/pgElephant/ram/discussions)
- **Documentation**: See component-specific READMEs above