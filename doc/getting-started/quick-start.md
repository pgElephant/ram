# Quick Start Guide

Get your RAM PostgreSQL High Availability cluster up and running in under 10 minutes!

## Prerequisites

Before you begin, ensure you have the following installed:

- **PostgreSQL 17**: `brew install postgresql@17`
- **Go 1.21+**: `brew install go`
- **Build Tools**: `brew install make gcc`
- **Git**: `brew install git`

## Step 1: Clone and Build

```bash
# Clone the repository
git clone https://github.com/pgelephant/ram.git
cd ram

# Build all components
make all

# Install components
make install
```

## Step 2: Configure PostgreSQL

Enable the PGRaft extension in PostgreSQL:

```bash
# Edit postgresql.conf
echo "shared_preload_libraries = 'pgraft'" >> /usr/local/var/postgresql@17/postgresql.conf

# Restart PostgreSQL
brew services restart postgresql@17
```

## Step 3: Create Your First Cluster

```bash
# Create a 3-node cluster
./ramctrl cluster create --num-nodes=3

# Verify cluster creation
./ramctrl cluster status
```

Expected output:
```
✓ Cluster 'ram_cluster' created successfully
✓ 3 nodes configured and started
✓ Primary node: localhost:5432
✓ Replica nodes: localhost:5433, localhost:5434
✓ Raft consensus: Active
✓ Health status: Healthy
```

## Step 4: Test High Availability

```bash
# Test failover by stopping the primary
./ramctrl cluster stop --node=primary

# Watch automatic failover
./ramctrl cluster status

# Restart the old primary (it will become a replica)
./ramctrl cluster start --node=primary
```

## Step 5: Monitor Your Cluster

```bash
# Start real-time monitoring
./ramctrl monitor

# View detailed cluster information
./ramctrl cluster info

# Check health metrics
curl http://localhost:8008/api/v1/cluster/health
```

## Step 6: Test Replication

```bash
# Connect to primary and create test data
psql -h localhost -p 5432 -U postgres -d postgres
```

```sql
-- Create test table
CREATE TABLE test_data (id SERIAL PRIMARY KEY, data TEXT);

-- Insert test data
INSERT INTO test_data (data) VALUES ('test1'), ('test2'), ('test3');

-- Check replication lag
SELECT * FROM pg_stat_replication;
```

## Step 7: Backup and Restore

```bash
# Create a backup
./ramctrl backup create --name=initial_backup

# List backups
./ramctrl backup list

# Test restore (optional)
./ramctrl backup restore --name=initial_backup --target-node=replica1
```

## What's Next?

Congratulations! You now have a fully functional PostgreSQL high availability cluster. Here's what you can do next:

### Explore the Documentation
- [Configuration Guide](configuration.md) - Customize your cluster
- [API Reference](api-reference/rest-api.md) - Use the REST API
- [Monitoring Setup](examples/monitoring.md) - Set up Prometheus and Grafana

### Advanced Features
- [Kubernetes Deployment](deployment/kubernetes.md) - Deploy on Kubernetes
- [Security Configuration](configuration/security.md) - Enable SSL and authentication
- [Backup Strategies](examples/backup-restore.md) - Implement backup policies

### Troubleshooting
- [Common Issues](troubleshooting/common-issues.md) - Resolve common problems
- [Debugging Guide](troubleshooting/debugging.md) - Debug cluster issues

## Quick Commands Reference

| Command | Description |
|---------|-------------|
| `./ramctrl cluster create --num-nodes=3` | Create 3-node cluster |
| `./ramctrl cluster status` | Show cluster status |
| `./ramctrl cluster stop --node=primary` | Stop primary node |
| `./ramctrl cluster start --node=primary` | Start primary node |
| `./ramctrl monitor` | Real-time monitoring |
| `./ramctrl backup create` | Create backup |
| `./ramctrl backup list` | List backups |

## Need Help?

- **Documentation**: Browse the full documentation
- **Issues**: Report bugs on GitHub
- **Community**: Join our discussions
- **Support**: Contact our support team

---

**Ready for production?** Check out our [Production Deployment Guide](deployment/production.md)!
