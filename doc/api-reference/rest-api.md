# RAM REST API Reference

Complete reference for the RAM system REST API endpoints.

## Base URL

```
http://localhost:8008/api/v1
```

## Authentication

### Token Authentication

```bash
curl -H "Authorization: Bearer your-token" \
  http://localhost:8008/api/v1/cluster/status
```

### Basic Authentication

```bash
curl -u username:password \
  http://localhost:8008/api/v1/cluster/status
```

## Response Format

All API responses follow this format:

```json
{
  "status": "success|error",
  "code": 200,
  "message": "Operation completed successfully",
  "data": { ... },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## Error Codes

| Code | Description |
|------|-------------|
| 200 | Success |
| 201 | Created |
| 400 | Bad Request |
| 401 | Unauthorized |
| 403 | Forbidden |
| 404 | Not Found |
| 409 | Conflict |
| 429 | Too Many Requests |
| 500 | Internal Server Error |
| 503 | Service Unavailable |

## Endpoints

### Health Endpoints

#### GET /health
Check daemon health.

**Response:**
```json
{
  "status": "success",
  "data": {
    "daemon_status": "running",
    "uptime_seconds": 3600,
    "version": "1.0.0"
  }
}
```

#### GET /cluster/health
Check cluster health.

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster_status": "healthy",
    "healthy_nodes": 3,
    "total_nodes": 3,
    "primary_node_id": 1,
    "replication_lag_ms": 0
  }
}
```

### Cluster Management

#### GET /cluster/status
Get cluster status.

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster_name": "ram_cluster",
    "node_count": 3,
    "primary_node_id": 1,
    "leader_node_id": 1,
    "has_quorum": true,
    "auto_failover_enabled": true,
    "synchronous_replication": true,
    "last_topology_change": 1704067200
  }
}
```

#### POST /cluster/create
Create new cluster.

**Request:**
```json
{
  "num_nodes": 3,
  "primary_port": 5432,
  "replica_ports": [5433, 5434],
  "data_dir": "/var/lib/postgresql",
  "log_dir": "/var/log/postgresql"
}
```

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster_id": "ram_cluster",
    "nodes_created": 3,
    "primary_node": "localhost:5432",
    "replica_nodes": ["localhost:5433", "localhost:5434"]
  }
}
```

#### POST /cluster/destroy
Destroy cluster.

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster_destroyed": true,
    "nodes_stopped": 3
  }
}
```

#### POST /cluster/start
Start cluster.

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster_started": true,
    "nodes_started": 3
  }
}
```

#### POST /cluster/stop
Stop cluster.

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster_stopped": true,
    "nodes_stopped": 3
  }
}
```

#### POST /cluster/restart
Restart cluster.

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster_restarted": true,
    "nodes_restarted": 3
  }
}
```

### Failover Operations

#### POST /cluster/failover
Trigger cluster failover.

**Response:**
```json
{
  "status": "success",
  "data": {
    "failover_triggered": true,
    "old_primary": 1,
    "new_primary": 2,
    "failover_time_ms": 1500
  }
}
```

#### POST /cluster/promote
Promote node to primary.

**Request:**
```json
{
  "node_id": 2
}
```

**Response:**
```json
{
  "status": "success",
  "data": {
    "node_promoted": true,
    "new_primary": 2,
    "promotion_time_ms": 1200
  }
}
```

#### POST /cluster/demote
Demote primary to replica.

**Request:**
```json
{
  "node_id": 1
}
```

**Response:**
```json
{
  "status": "success",
  "data": {
    "node_demoted": true,
    "old_primary": 1,
    "new_primary": 2
  }
}
```

### Node Management

#### GET /nodes
List all nodes.

**Response:**
```json
{
  "status": "success",
  "data": {
    "nodes": [
      {
        "id": 1,
        "name": "primary",
        "hostname": "localhost",
        "port": 5432,
        "role": "primary",
        "state": "running",
        "priority": 100,
        "replication_lag_ms": 0,
        "last_seen": 1704067200
      }
    ]
  }
}
```

#### GET /nodes/{node_id}
Get node information.

**Response:**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "name": "primary",
    "hostname": "localhost",
    "port": 5432,
    "role": "primary",
    "state": "running",
    "priority": 100,
    "replication_lag_ms": 0,
    "last_seen": 1704067200,
    "postgresql_version": "17.0",
    "uptime_seconds": 3600
  }
}
```

#### POST /nodes/{node_id}/start
Start node.

**Response:**
```json
{
  "status": "success",
  "data": {
    "node_started": true,
    "node_id": 1,
    "start_time": 1704067200
  }
}
```

#### POST /nodes/{node_id}/stop
Stop node.

**Response:**
```json
{
  "status": "success",
  "data": {
    "node_stopped": true,
    "node_id": 1,
    "stop_time": 1704067200
  }
}
```

#### POST /nodes/{node_id}/restart
Restart node.

**Response:**
```json
{
  "status": "success",
  "data": {
    "node_restarted": true,
    "node_id": 1,
    "restart_time": 1704067200
  }
}
```

### Backup Management

#### POST /backup/start
Start backup.

**Request:**
```json
{
  "name": "backup_001",
  "type": "full",
  "compression": true,
  "retention_days": 7
}
```

**Response:**
```json
{
  "status": "success",
  "data": {
    "backup_id": "backup_001",
    "backup_type": "full",
    "started_at": 1704067200,
    "estimated_duration_seconds": 300
  }
}
```

#### GET /backup/list
List backups.

**Response:**
```json
{
  "status": "success",
  "data": {
    "backups": [
      {
        "id": "backup_001",
        "type": "full",
        "size_bytes": 1073741824,
        "created_at": 1704067200,
        "status": "completed"
      }
    ]
  }
}
```

#### GET /backup/{backup_id}
Get backup information.

**Response:**
```json
{
  "status": "success",
  "data": {
    "id": "backup_001",
    "type": "full",
    "size_bytes": 1073741824,
    "created_at": 1704067200,
    "status": "completed",
    "duration_seconds": 300,
    "compression_ratio": 0.5
  }
}
```

#### POST /backup/restore
Restore backup.

**Request:**
```json
{
  "backup_id": "backup_001",
  "target_node": "replica1",
  "restore_time": "2024-01-01T12:00:00Z"
}
```

**Response:**
```json
{
  "status": "success",
  "data": {
    "restore_started": true,
    "backup_id": "backup_001",
    "target_node": "replica1",
    "estimated_duration_seconds": 600
  }
}
```

### Configuration Management

#### GET /config
Get configuration.

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster": {
      "name": "ram_cluster",
      "size": 3
    },
    "monitoring": {
      "interval_ms": 5000,
      "auto_failover_enabled": true
    }
  }
}
```

#### POST /config
Update configuration.

**Request:**
```json
{
  "monitoring": {
    "interval_ms": 3000,
    "auto_failover_enabled": true
  }
}
```

**Response:**
```json
{
  "status": "success",
  "data": {
    "config_updated": true,
    "updated_sections": ["monitoring"]
  }
}
```

### Metrics and Monitoring

#### GET /metrics
Get Prometheus metrics.

**Response:**
```
# HELP ramd_cluster_nodes_total Total number of nodes
# TYPE ramd_cluster_nodes_total gauge
ramd_cluster_nodes_total 3

# HELP ramd_cluster_healthy_nodes Number of healthy nodes
# TYPE ramd_cluster_healthy_nodes gauge
ramd_cluster_healthy_nodes 3
```

#### GET /cluster/metrics
Get cluster metrics.

**Response:**
```json
{
  "status": "success",
  "data": {
    "cluster_metrics": {
      "total_nodes": 3,
      "healthy_nodes": 3,
      "primary_node_id": 1,
      "replication_lag_ms": 0,
      "failover_count": 0,
      "uptime_seconds": 3600
    }
  }
}
```

### Synchronous Replication

#### GET /sync-standbys/status
Get synchronous standby status.

**Response:**
```json
{
  "status": "success",
  "data": {
    "synchronous_standbys": ["replica1", "replica2"],
    "num_sync_standbys": 2,
    "any_n_enabled": false,
    "sync_replication_timeout_ms": 10000
  }
}
```

#### POST /sync-standbys/add
Add synchronous standby.

**Request:**
```json
{
  "node_name": "replica3"
}
```

**Response:**
```json
{
  "status": "success",
  "data": {
    "standby_added": true,
    "node_name": "replica3",
    "total_sync_standbys": 3
  }
}
```

#### POST /sync-standbys/remove
Remove synchronous standby.

**Request:**
```json
{
  "node_name": "replica3"
}
```

**Response:**
```json
{
  "status": "success",
  "data": {
    "standby_removed": true,
    "node_name": "replica3",
    "total_sync_standbys": 2
  }
}
```

## Rate Limiting

API requests are rate limited to prevent abuse:

- **Default**: 100 requests per minute per IP
- **Configurable**: Via `rate_limit_requests_per_minute` setting
- **Headers**: Rate limit information in response headers

```bash
# Rate limit headers
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1704067260
```

## Error Handling

### Error Response Format

```json
{
  "status": "error",
  "code": 400,
  "message": "Invalid request parameters",
  "details": {
    "field": "num_nodes",
    "error": "Must be between 1 and 9"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

### Common Errors

#### 400 Bad Request
```json
{
  "status": "error",
  "code": 400,
  "message": "Invalid request parameters",
  "details": {
    "field": "num_nodes",
    "error": "Must be between 1 and 9"
  }
}
```

#### 401 Unauthorized
```json
{
  "status": "error",
  "code": 401,
  "message": "Authentication required",
  "details": {
    "auth_method": "token",
    "error": "Invalid or missing token"
  }
}
```

#### 404 Not Found
```json
{
  "status": "error",
  "code": 404,
  "message": "Resource not found",
  "details": {
    "resource": "node",
    "id": "999",
    "error": "Node with ID 999 not found"
  }
}
```

#### 429 Too Many Requests
```json
{
  "status": "error",
  "code": 429,
  "message": "Rate limit exceeded",
  "details": {
    "limit": 100,
    "reset_time": 1704067260,
    "error": "Too many requests"
  }
}
```

## SDK Examples

### Python

```python
import requests

class RAMClient:
    def __init__(self, host='localhost', port=8008, token=None):
        self.base_url = f"http://{host}:{port}/api/v1"
        self.headers = {}
        if token:
            self.headers['Authorization'] = f"Bearer {token}"
    
    def get_cluster_status(self):
        response = requests.get(f"{self.base_url}/cluster/status", headers=self.headers)
        return response.json()
    
    def create_cluster(self, num_nodes=3):
        data = {"num_nodes": num_nodes}
        response = requests.post(f"{self.base_url}/cluster/create", 
                               json=data, headers=self.headers)
        return response.json()

# Usage
client = RAMClient(token="your-token")
status = client.get_cluster_status()
print(status)
```

### JavaScript

```javascript
class RAMClient {
    constructor(host = 'localhost', port = 8008, token = null) {
        this.baseUrl = `http://${host}:${port}/api/v1`;
        this.headers = {};
        if (token) {
            this.headers['Authorization'] = `Bearer ${token}`;
        }
    }
    
    async getClusterStatus() {
        const response = await fetch(`${this.baseUrl}/cluster/status`, {
            headers: this.headers
        });
        return await response.json();
    }
    
    async createCluster(numNodes = 3) {
        const response = await fetch(`${this.baseUrl}/cluster/create`, {
            method: 'POST',
            headers: {
                ...this.headers,
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({num_nodes: numNodes})
        });
        return await response.json();
    }
}

// Usage
const client = new RAMClient('localhost', 8008, 'your-token');
client.getClusterStatus().then(status => console.log(status));
```

### Go

```go
package main

import (
    "bytes"
    "encoding/json"
    "fmt"
    "net/http"
)

type RAMClient struct {
    BaseURL string
    Token   string
}

func NewRAMClient(host, port, token string) *RAMClient {
    return &RAMClient{
        BaseURL: fmt.Sprintf("http://%s:%s/api/v1", host, port),
        Token:   token,
    }
}

func (c *RAMClient) GetClusterStatus() (map[string]interface{}, error) {
    req, err := http.NewRequest("GET", c.BaseURL+"/cluster/status", nil)
    if err != nil {
        return nil, err
    }
    
    if c.Token != "" {
        req.Header.Set("Authorization", "Bearer "+c.Token)
    }
    
    client := &http.Client{}
    resp, err := client.Do(req)
    if err != nil {
        return nil, err
    }
    defer resp.Body.Close()
    
    var result map[string]interface{}
    err = json.NewDecoder(resp.Body).Decode(&result)
    return result, err
}

// Usage
client := NewRAMClient("localhost", "8008", "your-token")
status, err := client.GetClusterStatus()
if err != nil {
    panic(err)
}
fmt.Println(status)
```

## Testing

### Using curl

```bash
# Test health endpoint
curl http://localhost:8008/api/v1/health

# Test cluster status
curl http://localhost:8008/api/v1/cluster/status

# Test with authentication
curl -H "Authorization: Bearer your-token" \
  http://localhost:8008/api/v1/cluster/status
```

### Using Postman

1. Import the API collection
2. Set base URL: `http://localhost:8008/api/v1`
3. Add authentication header if needed
4. Test endpoints

---

**Next Steps**: Learn about [Configuration Management](configuration/cluster.md)!
