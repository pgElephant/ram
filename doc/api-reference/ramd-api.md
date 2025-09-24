# RAMD API Reference

## Overview
The RAMD (RAM Daemon) provides a comprehensive REST API for managing PostgreSQL clusters with Raft consensus.

## Base URL
```
http://localhost:8080/api/v1
```

## Authentication
All API endpoints require authentication via API key or token.

### Headers
```
Authorization: Bearer <token>
Content-Type: application/json
```

## Endpoints

### Cluster Management

#### GET /cluster/status
Get current cluster status.

**Response:**
```json
{
  "status": "healthy",
  "nodes": [
    {
      "id": "node1",
      "role": "leader",
      "status": "online",
      "address": "127.0.0.1:5432"
    }
  ],
  "leader": "node1",
  "term": 1
}
```

#### POST /cluster/add-node
Add a new node to the cluster.

**Request:**
```json
{
  "node_id": "node2",
  "hostname": "node2.example.com",
  "address": "127.0.0.1:5433",
  "port": 5433
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Node added successfully",
  "node_id": "node2"
}
```

#### POST /cluster/remove-node
Remove a node from the cluster.

**Request:**
```json
{
  "node_address": "127.0.0.1:5433"
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Node removed successfully"
}
```

### Failover Management

#### POST /cluster/failover/{target_node_id}
Trigger failover to target node.

**Response:**
```json
{
  "status": "success",
  "message": "Failover initiated",
  "new_leader": "node2"
}
```

#### POST /cluster/promote/{node_id}
Promote node to leader.

**Response:**
```json
{
  "status": "success",
  "message": "Node promoted successfully"
}
```

#### POST /cluster/demote/{node_id}
Demote node from leader.

**Response:**
```json
{
  "status": "success",
  "message": "Node demoted successfully"
}
```

### Configuration Management

#### GET /config
Get current configuration.

**Response:**
```json
{
  "postgresql": {
    "host": "127.0.0.1",
    "port": 5432,
    "user": "postgres"
  },
  "cluster": {
    "name": "default_cluster",
    "max_nodes": 16
  }
}
```

#### POST /config
Update configuration.

**Request:**
```json
{
  "postgresql": {
    "port": 5433
  }
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Configuration updated"
}
```

### Backup Management

#### POST /backup/start
Start backup process.

**Request:**
```json
{
  "backup_name": "backup_2024_01_01",
  "compression": true
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Backup started",
  "backup_id": "backup_123"
}
```

#### GET /backup/list
List available backups.

**Response:**
```json
{
  "backups": [
    {
      "id": "backup_123",
      "name": "backup_2024_01_01",
      "created": "2024-01-01T00:00:00Z",
      "size": "1.2GB"
    }
  ]
}
```

### Monitoring

#### GET /metrics
Get Prometheus metrics.

**Response:**
```
# HELP ramd_cluster_nodes_total Total number of cluster nodes
# TYPE ramd_cluster_nodes_total gauge
ramd_cluster_nodes_total 3

# HELP ramd_cluster_leader_term Current leader term
# TYPE ramd_cluster_leader_term gauge
ramd_cluster_leader_term 1
```

#### GET /health
Health check endpoint.

**Response:**
```json
{
  "status": "healthy",
  "timestamp": "2024-01-01T00:00:00Z",
  "uptime": 3600
}
```

## Error Responses

All error responses follow this format:

```json
{
  "error": "error_code",
  "message": "Human readable error message",
  "details": "Additional error details"
}
```

### Common Error Codes

- `400` - Bad Request
- `401` - Unauthorized
- `403` - Forbidden
- `404` - Not Found
- `500` - Internal Server Error
- `503` - Service Unavailable

## Rate Limiting

API requests are rate limited to prevent abuse. Default limits:
- 100 requests per minute per IP
- 1000 requests per hour per API key

Rate limit headers are included in responses:
```
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1640995200
```
