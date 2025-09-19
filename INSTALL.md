# Installation Guide

This guide provides detailed instructions for installing pgElephant RAM on various platforms.

## Prerequisites

### System Requirements

- **Operating System**: Linux (Ubuntu 20.04+, CentOS 8+, RHEL 8+), macOS 10.15+, or Windows 10+
- **PostgreSQL**: Version 12 or higher
- **Memory**: Minimum 2GB RAM, 4GB recommended
- **Disk Space**: 1GB for installation, additional space for data and logs
- **Network**: TCP/IP connectivity between cluster nodes

### Dependencies

- **Build Tools**: GCC 7.0+, Make, CMake 3.10+
- **PostgreSQL Development**: postgresql-server-dev, libpq-dev
- **Libraries**: libssl-dev, libcurl4-openssl-dev, libjansson-dev
- **Python**: Python 3.6+ (for testing and utilities)

## Installation Methods

### 1. Docker Installation (Recommended)

#### Using Docker Compose

1. **Clone the repository:**
```bash
git clone https://github.com/pgelephant/ram.git
cd ram
```

2. **Start the cluster:**
```bash
docker-compose up -d
```

3. **Verify installation:**
```bash
docker-compose ps
docker-compose logs postgres-primary
```

#### Using Docker Images

1. **Pull the images:**
```bash
docker pull pgelephant/ram-postgres:latest
docker pull pgelephant/ram-daemon:latest
```

2. **Run PostgreSQL with pgraft:**
```bash
docker run -d \
  --name postgres-primary \
  -e POSTGRES_DB=testdb \
  -e POSTGRES_USER=postgres \
  -e POSTGRES_PASSWORD=postgres \
  -e PGRaft_NODE_ID=1 \
  -e PGRaft_NODE_NAME=primary \
  -e PGRaft_CLUSTER_NAME=my-cluster \
  -p 5432:5432 \
  pgelephant/ram-postgres:latest
```

3. **Run ramd daemon:**
```bash
docker run -d \
  --name ramd-primary \
  --link postgres-primary:postgres \
  -p 8080:8080 \
  pgelephant/ram-daemon:latest
```

### 2. Package Installation

#### Ubuntu/Debian

1. **Add the repository:**
```bash
curl -fsSL https://packages.pgelephant.com/ram/gpg | sudo apt-key add -
echo "deb https://packages.pgelephant.com/ram/ubuntu/ focal main" | sudo tee /etc/apt/sources.list.d/pgelephant-ram.list
```

2. **Update package list:**
```bash
sudo apt update
```

3. **Install packages:**
```bash
sudo apt install postgresql-15-pgraft ramd ramctrl
```

#### CentOS/RHEL

1. **Add the repository:**
```bash
sudo yum install https://packages.pgelephant.com/ram/centos/8/x86_64/pgelephant-ram-release-1.0-1.el8.noarch.rpm
```

2. **Install packages:**
```bash
sudo yum install postgresql15-pgraft ramd ramctrl
```

#### macOS

1. **Install using Homebrew:**
```bash
brew tap pgelephant/ram
brew install pgraft ramd ramctrl
```

### 3. Source Installation

#### Prerequisites

1. **Install PostgreSQL development packages:**
```bash
# Ubuntu/Debian
sudo apt install postgresql-server-dev-15 libpq-dev libssl-dev libcurl4-openssl-dev libjansson-dev

# CentOS/RHEL
sudo yum install postgresql15-devel libpq-devel openssl-devel libcurl-devel jansson-devel

# macOS
brew install postgresql libpq openssl curl jansson
```

2. **Install build tools:**
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake git

# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install cmake git

# macOS
xcode-select --install
brew install cmake git
```

#### Build and Install

1. **Clone the repository:**
```bash
git clone https://github.com/pgelephant/ram.git
cd ram
```

2. **Build the project:**
```bash
make clean
make
sudo make install
```

3. **Verify installation:**
```bash
# Check pgraft extension
psql -U postgres -c "SELECT * FROM pg_available_extensions WHERE name = 'pgraft';"

# Check ramd daemon
ramd --version

# Check ramctrl utility
ramctrl --version
```

## Configuration

### PostgreSQL Configuration

1. **Edit postgresql.conf:**
```postgresql
# Add to postgresql.conf
shared_preload_libraries = 'pgraft'

# pgraft configuration
pgraft.node_id = 1
pgraft.node_name = 'primary'
pgraft.cluster_name = 'my-cluster'
pgraft.raft_port = 5433
pgraft.log_level = 1

# Replication settings
wal_level = replica
max_wal_senders = 10
max_replication_slots = 10
hot_standby = on
```

2. **Restart PostgreSQL:**
```bash
sudo systemctl restart postgresql
```

3. **Create the extension:**
```sql
psql -U postgres -c "CREATE EXTENSION pgraft;"
```

### ramd Daemon Configuration

1. **Create configuration directory:**
```bash
sudo mkdir -p /etc/ramd
sudo mkdir -p /var/log/ramd
sudo mkdir -p /var/lib/ramd
```

2. **Create configuration file:**
```bash
sudo tee /etc/ramd/ramd.conf > /dev/null <<EOF
[global]
daemon = true
pid_file = /var/run/ramd.pid
log_file = /var/log/ramd/ramd.log
log_level = INFO

[http]
enabled = true
port = 8080
host = 0.0.0.0
timeout = 30

[database]
host = localhost
port = 5432
name = postgres
user = postgres
password = postgres

[cluster]
name = my-cluster
node_id = 1
node_name = primary
raft_port = 5433

[monitoring]
health_check_interval = 30
metrics_enabled = true
metrics_port = 9090
EOF
```

3. **Create systemd service:**
```bash
sudo tee /etc/systemd/system/ramd.service > /dev/null <<EOF
[Unit]
Description=pgElephant RAM Daemon
After=postgresql.service
Requires=postgresql.service

[Service]
Type=forking
User=postgres
Group=postgres
ExecStart=/usr/local/bin/ramd --config /etc/ramd/ramd.conf --daemon
ExecStop=/bin/kill -TERM \$MAINPID
PIDFile=/var/run/ramd.pid
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
```

4. **Start the service:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable ramd
sudo systemctl start ramd
```

## Verification

### Check Installation

1. **Verify pgraft extension:**
```sql
psql -U postgres -c "SELECT * FROM pg_available_extensions WHERE name = 'pgraft';"
psql -U postgres -c "SELECT pgraft_version();"
```

2. **Verify ramd daemon:**
```bash
sudo systemctl status ramd
curl http://localhost:8080/health
```

3. **Verify ramctrl utility:**
```bash
ramctrl --version
ramctrl list-clusters
```

### Test Cluster Formation

1. **Initialize cluster:**
```sql
psql -U postgres -c "SELECT pgraft_init_cluster('test-cluster', 1, 'primary');"
```

2. **Check cluster status:**
```sql
psql -U postgres -c "SELECT * FROM pgraft.cluster_overview;"
```

3. **Test HTTP API:**
```bash
curl http://localhost:8080/cluster/info
```

## Troubleshooting

### Common Issues

1. **Extension not found:**
   - Check PostgreSQL version compatibility
   - Verify shared_preload_libraries setting
   - Restart PostgreSQL after configuration changes

2. **ramd daemon fails to start:**
   - Check configuration file syntax
   - Verify database connectivity
   - Check log files for errors

3. **Cluster formation fails:**
   - Check network connectivity
   - Verify firewall settings
   - Ensure unique node IDs

### Log Files

- **PostgreSQL logs**: `/var/log/postgresql/postgresql-15-main.log`
- **ramd logs**: `/var/log/ramd/ramd.log`
- **System logs**: `journalctl -u ramd`

### Debug Mode

Enable debug logging for troubleshooting:

```postgresql
-- In postgresql.conf
pgraft.log_level = 3
```

```ini
# In ramd.conf
log_level = DEBUG
```

## Uninstallation

### Package Uninstallation

```bash
# Ubuntu/Debian
sudo apt remove postgresql-15-pgraft ramd ramctrl

# CentOS/RHEL
sudo yum remove postgresql15-pgraft ramd ramctrl

# macOS
brew uninstall pgraft ramd ramctrl
```

### Source Uninstallation

```bash
cd ram
sudo make uninstall
```

### Cleanup

1. **Remove configuration files:**
```bash
sudo rm -rf /etc/ramd
sudo rm -rf /var/log/ramd
sudo rm -rf /var/lib/ramd
```

2. **Remove systemd service:**
```bash
sudo systemctl stop ramd
sudo systemctl disable ramd
sudo rm /etc/systemd/system/ramd.service
sudo systemctl daemon-reload
```

3. **Remove PostgreSQL extension:**
```sql
psql -U postgres -c "DROP EXTENSION IF EXISTS pgraft;"
```

## Support

For installation issues or questions:

- **Documentation**: [https://docs.pgelephant.com/ram](https://docs.pgelephant.com/ram)
- **Issues**: [GitHub Issues](https://github.com/pgelephant/ram/issues)
- **Email**: support@pglephant.com
