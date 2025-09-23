# RAM PostgreSQL High Availability System Documentation

Welcome to the RAM (Raft-based Auto-failover Manager) PostgreSQL High Availability System documentation. This comprehensive guide will help you understand, deploy, and manage a highly available PostgreSQL cluster using our advanced Raft consensus-based failover system.

## Table of Contents

### Getting Started
- [Quick Start Guide](getting-started/quick-start.md) - Get up and running in minutes
- [Installation Guide](getting-started/installation.md) - Complete installation instructions
- [Configuration Guide](getting-started/configuration.md) - Configure your cluster
- [First Cluster](getting-started/first-cluster.md) - Create your first cluster

### Module Documentation
- [PGRaft Extension](getting-started/pgraft.md) - PostgreSQL Raft consensus extension
- [RAMD Daemon](getting-started/ramd.md) - High availability daemon
- [RAMCTRL CLI](getting-started/ramctrl.md) - Command-line interface

### API Reference
- [REST API](api-reference/rest-api.md) - Complete API documentation
- [Configuration API](api-reference/configuration-api.md) - Configuration management
- [Monitoring API](api-reference/monitoring-api.md) - Health and metrics
- [Backup API](api-reference/backup-api.md) - Backup and restore operations

### Configuration
- [Cluster Configuration](configuration/cluster.md) - Cluster setup and management
- [Node Configuration](configuration/nodes.md) - Individual node settings
- [Security Configuration](configuration/security.md) - Authentication and encryption
- [Monitoring Configuration](configuration/monitoring.md) - Metrics and alerting

### Deployment
- [Production Deployment](deployment/production.md) - Production-ready deployment
- [Docker Deployment](deployment/docker.md) - Containerized deployment
- [Kubernetes Deployment](deployment/kubernetes.md) - Kubernetes operator
- [Cloud Deployment](deployment/cloud.md) - AWS, GCP, Azure deployment

### Development
- [Development Setup](development/setup.md) - Development environment
- [Building from Source](development/building.md) - Compile and build
- [Testing](development/testing.md) - Test suite and validation
- [Contributing](development/contributing.md) - Contributing guidelines

### Troubleshooting
- [Common Issues](troubleshooting/common-issues.md) - Frequently encountered problems
- [Debugging Guide](troubleshooting/debugging.md) - Debug and diagnose issues
- [Performance Tuning](troubleshooting/performance.md) - Optimize performance
- [Recovery Procedures](troubleshooting/recovery.md) - Disaster recovery

### Examples
- [Basic Cluster](examples/basic-cluster.md) - Simple 3-node cluster
- [High Availability Setup](examples/ha-setup.md) - Production HA configuration
- [Backup and Restore](examples/backup-restore.md) - Backup strategies
- [Monitoring Setup](examples/monitoring.md) - Prometheus and Grafana

## Architecture Overview

RAM is a comprehensive PostgreSQL high availability solution consisting of three main components:

### PGRaft Extension
- **Purpose**: PostgreSQL extension providing Raft consensus algorithm
- **Features**: Leader election, consensus, shared memory management
- **Integration**: Deep PostgreSQL integration via shared memory

### RAMD Daemon
- **Purpose**: High availability daemon managing cluster operations
- **Features**: Health monitoring, failover management, API server
- **Integration**: Communicates with PGRaft and external systems

### RAMCTRL CLI
- **Purpose**: Command-line interface for cluster management
- **Features**: Cluster operations, monitoring, configuration
- **Integration**: HTTP-based communication with RAMD

## Key Features

### High Availability
- ✅ Automatic failover with Raft consensus
- ✅ Zero-downtime promotions and demotions
- ✅ Network partition handling
- ✅ Split-brain prevention

### Monitoring & Observability
- ✅ Comprehensive health monitoring
- ✅ Prometheus metrics integration
- ✅ Grafana dashboards
- ✅ Real-time cluster status

### Backup & Recovery
- ✅ pgBackRest integration
- ✅ Barman support
- ✅ Automated backup scheduling
- ✅ Point-in-time recovery

### Security
- ✅ SSL/TLS encryption
- ✅ Authentication and authorization
- ✅ Rate limiting and DoS protection
- ✅ Audit logging

### Kubernetes Integration
- ✅ Native Kubernetes operator
- ✅ Custom Resource Definitions (CRDs)
- ✅ Helm charts
- ✅ Auto-scaling support

### Advanced Features
- ✅ Multiple synchronous standbys
- ✅ ANY N synchronous replication
- ✅ Parameter validation and optimization
- ✅ Maintenance mode operations

## Quick Start

1. **Install Dependencies**
   ```bash
   # Install PostgreSQL 17
   brew install postgresql@17
   
   # Install Go (for PGRaft)
   brew install go
   
   # Install build tools
   brew install make gcc
   ```

2. **Build and Install**
   ```bash
   # Build all components
   make all
   
   # Install PGRaft extension
   make install-pgraft
   
   # Install RAMD daemon
   make install-ramd
   
   # Install RAMCTRL CLI
   make install-ramctrl
   ```

3. **Create Your First Cluster**
   ```bash
   # Create a 3-node cluster
   ./ramctrl cluster create --num-nodes=3
   
   # Check cluster status
   ./ramctrl cluster status
   ```

4. **Monitor Your Cluster**
   ```bash
   # Start monitoring
   ./ramctrl monitor
   
   # View metrics
   curl http://localhost:8008/metrics
   ```

## Support

- **Documentation**: This comprehensive guide
- **Issues**: GitHub Issues for bug reports
- **Discussions**: GitHub Discussions for questions
- **Community**: Join our community forum

## License

Copyright (c) 2024-2025, pgElephant, Inc. All rights reserved.

This software is licensed under the PostgreSQL License. See LICENSE file for details.

## Contributing

We welcome contributions! Please see our [Contributing Guide](development/contributing.md) for details on how to get started.

---

**Ready to get started?** Begin with our [Quick Start Guide](getting-started/quick-start.md)!
