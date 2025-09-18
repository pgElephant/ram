# PostgreSQL Distributed Consensus Project

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

## 🚀 Quick Start

1. **Install pgraft extension**: See [pgraft/README.md](pgraft/README.md)
2. **Start ramd daemon**: See [ramd/README.md](ramd/README.md)  
3. **Use ramctrl utility**: See [ramctrl/README.md](ramctrl/README.md)

## 📊 Project Status

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

## 🏗️ Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   PostgreSQL    │    │   PostgreSQL    │    │   PostgreSQL    │
│   + pgraft      │    │   + pgraft      │    │   + pgraft      │
│   (Node 1)      │    │   (Node 2)      │    │   (Node 3)      │
└─────────┬───────┘    └─────────┬───────┘    └─────────┬───────┘
          │                      │                      │
          └──────────────────────┼──────────────────────┘
                                 │
                    ┌─────────────┴─────────────┐
                    │        ramd daemon        │
                    │    (Cluster Manager)      │
                    └─────────────┬─────────────┘
                                 │
                    ┌─────────────┴─────────────┐
                    │       ramctrl CLI         │
                    │    (Control Utility)      │
                    └───────────────────────────┘
```

## 📁 Project Structure

```
├── pgraft/                 # PostgreSQL extension for distributed consensus
│   ├── src/               # C source files
│   ├── include/           # Header files
│   ├── sql/               # SQL functions and views
│   └── README.md          # 📖 [Extension Documentation](pgraft/README.md)
├── ramd/                  # Daemon for cluster management
│   ├── src/               # C source files
│   ├── include/           # Header files
│   └── README.md          # 📖 [Daemon Documentation](ramd/README.md)
├── ramctrl/               # Control utility for cluster operations
│   ├── src/               # C source files
│   ├── include/           # Header files
│   └── README.md          # 📖 [CLI Documentation](ramctrl/README.md)
└── README.md              # This overview file
```

## 🧩 Components

### [pgraft](pgraft/README.md) - PostgreSQL Extension
**Pure C PostgreSQL extension providing distributed consensus**

- ✅ Custom Raft consensus algorithm
- ✅ High availability and automatic failover  
- ✅ Log replication across cluster nodes
- ✅ Background worker process
- ✅ Comprehensive monitoring and metrics
- ✅ PostgreSQL integration with custom functions

**[📖 Read pgraft Documentation](pgraft/README.md)**

### [ramd](ramd/README.md) - Cluster Management Daemon
**Daemon process managing PostgreSQL cluster operations**

- ✅ Cluster node management
- ✅ Health monitoring and failover
- ✅ HTTP API for external integration
- ✅ Configuration management
- ✅ Logging and monitoring

**[📖 Read ramd Documentation](ramd/README.md)**

### [ramctrl](ramctrl/README.md) - Control Utility
**Command-line utility for cluster management**

- ✅ Cluster management commands
- ✅ Status monitoring and health checks
- ✅ Replication control
- ✅ Failover operations
- ✅ Maintenance tasks

**[📖 Read ramctrl Documentation](ramctrl/README.md)**

## 🛠️ Development

### Building All Components

```bash
# Build everything
make all

# Build individual components
make build-pgraft
make build-ramd
make build-ramctrl
```

### Testing

```bash
# Run tests
make test

# Run specific component tests
cd pgraft && make test
cd ramd && make test
cd ramctrl && make test
```

## 📚 Documentation

Each component has its own comprehensive documentation:

- **[pgraft/README.md](pgraft/README.md)** - PostgreSQL extension documentation
- **[ramd/README.md](ramd/README.md)** - Cluster daemon documentation  
- **[ramctrl/README.md](ramctrl/README.md)** - Control utility documentation

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `make test`
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🆘 Support

- **Issues**: [GitHub Issues](https://github.com/pgElephant/ram/issues)
- **Discussions**: [GitHub Discussions](https://github.com/pgElephant/ram/discussions)
- **Documentation**: See component-specific READMEs above