# Docker Setup for RAM Project

This directory contains Docker configurations for running PostgreSQL 17 with pgraft, ramd, and ramctrl in containerized environments.

## Files

- `Dockerfile` - Main Docker image with PostgreSQL 17, pgraft, ramd, and ramctrl
- `docker-compose.yml` - 3-node cluster setup (1 primary + 2 replicas)
- `docker-compose-single.yml` - Single node setup for development
- `entrypoint.sh` - Startup script that initializes all services
- `ramd.conf` - ramd daemon configuration
- `postgresql.conf` - PostgreSQL 17 configuration optimized for pgraft
- `pg_hba.conf` - PostgreSQL 17 authentication configuration

## Quick Start

### Single Node Setup

```bash
# Build and run single node
docker-compose -f docker-compose-single.yml up --build

# Connect to PostgreSQL
psql -h localhost -p 5432 -U postgres -d postgres

# Check pgraft extension
\dx pgraft

# Check cluster status
docker exec -it postgres-single ramctrl status
```

### 3-Node Cluster Setup

```bash
# Build and run 3-node cluster
docker-compose up --build

# Connect to primary node
psql -h localhost -p 5432 -U postgres -d postgres

# Connect to replica 1
psql -h localhost -p 5433 -U postgres -d postgres

# Connect to replica 2
psql -h localhost -p 5434 -U postgres -d postgres

# Check cluster status from ramctrl container
docker exec -it ramctrl ramctrl status
```

## Configuration

### Environment Variables

- `POSTGRES_DB` - Database name (default: postgres)
- `POSTGRES_USER` - Database user (default: postgres)
- `POSTGRES_PASSWORD` - Database password (default: postgres)
- `PGRAPT_NODE_ID` - Node ID for pgraft (1, 2, 3)
- `PGRAPT_CLUSTER_ADDRESSES` - Comma-separated list of cluster nodes
- `RAMD_NODE_ID` - Node ID for ramd daemon

### Ports

- `5432` - Primary PostgreSQL node
- `5433` - Replica 1 PostgreSQL node
- `5434` - Replica 2 PostgreSQL node
- `8080` - ramd metrics/health endpoint (primary)
- `8081` - ramd metrics/health endpoint (replica 1)
- `8082` - ramd metrics/health endpoint (replica 2)

## Management Commands

### Check Cluster Status

```bash
# From ramctrl container
docker exec -it ramctrl ramctrl status

# Check individual nodes
docker exec -it postgres-primary ramctrl status
docker exec -it postgres-replica1 ramctrl status
docker exec -it postgres-replica2 ramctrl status
```

### Add/Remove Nodes

```bash
# Add node to cluster
docker exec -it ramctrl ramctrl add-node --node-id 4 --address new-node --port 5432

# Remove node from cluster
docker exec -it ramctrl ramctrl remove-node --node-id 3
```

### Health Checks

```bash
# Check cluster health
docker exec -it ramctrl ramctrl health

# Check individual node health
docker exec -it postgres-primary ramctrl health
```

## Development

### Building Custom Image

```bash
# Build image with custom configuration
docker build -t ram-postgres:latest -f docker/Dockerfile .

# Run custom image
docker run -d --name ram-postgres -p 5432:5432 ram-postgres:latest
```

### Debugging

```bash
# View logs
docker logs postgres-primary
docker logs postgres-replica1
docker logs postgres-replica2

# Execute shell in container
docker exec -it postgres-primary bash
docker exec -it ramctrl bash
```

## Production Considerations

1. **Security**: Change default passwords and configure proper authentication
2. **Persistence**: Use named volumes or bind mounts for data persistence
3. **Networking**: Configure proper network security and firewall rules
4. **Monitoring**: Set up proper logging and monitoring for production use
5. **Backup**: Implement regular backup strategies for PostgreSQL data

## Troubleshooting

### Common Issues

1. **Connection Refused**: Check if PostgreSQL is fully started before connecting
2. **Extension Not Found**: Ensure pgraft extension is properly installed and loaded
3. **Cluster Not Forming**: Verify all nodes can communicate and have correct configuration
4. **Permission Denied**: Check file permissions and PostgreSQL user privileges

### Logs

```bash
# View all container logs
docker-compose logs

# View specific service logs
docker-compose logs postgres-primary
docker-compose logs postgres-replica1
docker-compose logs postgres-replica2
```
