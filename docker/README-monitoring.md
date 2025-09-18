# RAM PostgreSQL Monitoring with Prometheus & Grafana

This directory contains the complete monitoring stack for RAM PostgreSQL clusters using Prometheus for metrics collection and Grafana for visualization.

## 🚀 Quick Start

### 1. Start the Complete Monitoring Stack

```bash
# Start PostgreSQL cluster with Prometheus and Grafana
docker-compose -f docker-compose-monitoring.yml up -d

# Check all services are running
docker-compose -f docker-compose-monitoring.yml ps
```

### 2. Access the Services

| Service | URL | Credentials |
|---------|-----|-------------|
| **Grafana** | http://localhost:3000 | admin/admin |
| **Prometheus** | http://localhost:9090 | - |
| **PostgreSQL Node 1** | localhost:5432 | postgres/postgres |
| **PostgreSQL Node 2** | localhost:5433 | postgres/postgres |
| **PostgreSQL Node 3** | localhost:5434 | postgres/postgres |
| **ramd API Node 1** | http://localhost:8008 | - |
| **ramd API Node 2** | http://localhost:8009 | - |
| **ramd API Node 3** | http://localhost:8010 | - |

## 📊 Available Metrics

### Cluster Metrics
- `ramd_cluster_nodes_total` - Total number of nodes in cluster
- `ramd_cluster_nodes_healthy` - Number of healthy nodes
- `ramd_cluster_has_quorum` - Whether cluster has quorum (1=yes, 0=no)
- `ramd_cluster_is_leader` - Whether this node is the leader (1=yes, 0=no)
- `ramd_cluster_primary_node_id` - ID of the primary node

### Node Metrics
- `ramd_node_healthy` - Whether this node is healthy (1=yes, 0=no)
- `ramd_node_health_score` - Health score of this node (0.0-1.0)
- `ramd_node_replication_lag_ms` - Replication lag in milliseconds
- `ramd_node_wal_lsn` - Current WAL LSN position

### Performance Metrics
- `ramd_health_checks_total` - Total number of health checks performed
- `ramd_health_checks_failed` - Total number of failed health checks
- `ramd_failovers_total` - Total number of failovers performed
- `ramd_promotions_total` - Total number of node promotions
- `ramd_demotions_total` - Total number of node demotions

### HTTP API Metrics
- `ramd_http_requests_total` - Total number of HTTP requests
- `ramd_http_requests_2xx` - Total number of 2xx HTTP responses
- `ramd_http_requests_4xx` - Total number of 4xx HTTP responses
- `ramd_http_requests_5xx` - Total number of 5xx HTTP responses

### Replication Metrics
- `ramd_replication_lag_max_ms` - Maximum replication lag in milliseconds
- `ramd_replication_lag_avg_ms` - Average replication lag in milliseconds
- `ramd_replication_connections_active` - Number of active replication connections
- `ramd_replication_connections_total` - Total number of replication connections

### Resource Metrics
- `ramd_memory_usage_bytes` - Memory usage in bytes
- `ramd_daemon_uptime_seconds` - Daemon uptime in seconds

## 🎯 Grafana Dashboards

### 1. Cluster Overview Dashboard
- **File**: `grafana/dashboards/ramd-cluster-overview.json`
- **Features**:
  - Cluster health status
  - Node count and health
  - Replication lag monitoring
  - Health check rates
  - Failover events timeline
  - HTTP API performance
  - Resource usage

### 2. Detailed Metrics Dashboard
- **File**: `grafana/dashboards/ramd-detailed-metrics.json`
- **Features**:
  - Node state table
  - WAL LSN position tracking
  - Replication connection status
  - Health check success rates
  - HTTP response times
  - Cluster events timeline

## 🔧 Configuration

### Prometheus Configuration
- **File**: `prometheus/prometheus.yml`
- **Scrape Interval**: 5 seconds for ramd metrics
- **Retention**: 200 hours
- **Targets**: All ramd nodes on ports 8008, 8009, 8010

### Grafana Configuration
- **Datasource**: Prometheus (auto-configured)
- **Dashboards**: Auto-provisioned from JSON files
- **Refresh Rate**: 5-10 seconds

## 🚨 Alerting Rules (Optional)

You can add Prometheus alerting rules to monitor critical conditions:

```yaml
# prometheus/alerts.yml
groups:
  - name: ramd-cluster
    rules:
      - alert: ClusterNoQuorum
        expr: ramd_cluster_has_quorum == 0
        for: 30s
        labels:
          severity: critical
        annotations:
          summary: "RAM cluster has lost quorum"
          
      - alert: NodeUnhealthy
        expr: ramd_node_healthy == 0
        for: 1m
        labels:
          severity: warning
        annotations:
          summary: "Node {{ $labels.hostname }} is unhealthy"
          
      - alert: HighReplicationLag
        expr: ramd_replication_lag_max_ms > 10000
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "High replication lag detected"
```

## 🛠️ Development & Testing

### Test Metrics Endpoint
```bash
# Test metrics endpoint directly
curl http://localhost:8008/metrics

# Test with Prometheus format
curl -H "Accept: text/plain" http://localhost:8008/metrics
```

### View Raw Metrics
```bash
# Check Prometheus targets
curl http://localhost:9090/api/v1/targets

# Query specific metrics
curl "http://localhost:9090/api/v1/query?query=ramd_cluster_has_quorum"
```

### Custom Queries
```promql
# Cluster health score
ramd_cluster_nodes_healthy / ramd_cluster_nodes_total

# Replication lag trend
rate(ramd_replication_lag_max_ms[5m])

# Health check success rate
rate(ramd_health_checks_total[5m]) - rate(ramd_health_checks_failed[5m])
```

## 📈 Monitoring Best Practices

### 1. Key Metrics to Watch
- **Cluster Quorum**: Always ensure `ramd_cluster_has_quorum == 1`
- **Node Health**: Monitor `ramd_node_healthy` for all nodes
- **Replication Lag**: Keep `ramd_replication_lag_max_ms < 1000`
- **Health Check Success Rate**: Should be > 95%

### 2. Dashboard Customization
- Clone dashboards for environment-specific views
- Add custom panels for business-specific metrics
- Set up alerting channels (Slack, email, PagerDuty)

### 3. Performance Tuning
- Adjust scrape intervals based on cluster size
- Use recording rules for expensive queries
- Set appropriate retention periods

## 🔍 Troubleshooting

### Common Issues

1. **Metrics not appearing in Grafana**
   - Check Prometheus targets: http://localhost:9090/targets
   - Verify ramd is running and accessible
   - Check firewall/network connectivity

2. **High memory usage**
   - Reduce scrape frequency
   - Lower retention period
   - Optimize query patterns

3. **Dashboard not loading**
   - Check Grafana logs: `docker logs grafana`
   - Verify datasource configuration
   - Ensure Prometheus is accessible

### Debug Commands
```bash
# Check container logs
docker logs ramd-node1
docker logs prometheus
docker logs grafana

# Test connectivity
curl -f http://localhost:8008/metrics
curl -f http://localhost:9090/api/v1/targets

# Restart services
docker-compose -f docker-compose-monitoring.yml restart
```

## 📚 Additional Resources

- [Prometheus Documentation](https://prometheus.io/docs/)
- [Grafana Documentation](https://grafana.com/docs/)
- [PostgreSQL Monitoring Best Practices](https://www.postgresql.org/docs/current/monitoring.html)
- [RAM Documentation](../README.md)

## 🤝 Contributing

To add new metrics or improve dashboards:

1. Add new metrics to `ramd_metrics.c`
2. Update Prometheus configuration if needed
3. Create/update Grafana dashboard JSON files
4. Test with the monitoring stack
5. Submit pull request with documentation

---

**Happy Monitoring!** 🎉
