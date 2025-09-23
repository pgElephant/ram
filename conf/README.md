# Configuration Files

This directory contains all configuration files for the pgraft/ramd/ramctrl system, organized by component.

## Directory Structure

```
conf/
├── cluster/           # Cluster configuration files
├── monitoring/        # Monitoring and observability configs
├── pgraft/           # PostgreSQL extension configurations
├── ramctrl/          # RAM Controller configurations
└── ramd/             # RAM Daemon configurations
```

## Configuration Files

### Cluster Configuration (`cluster/`)
- `cluster.json` - Main cluster configuration with node definitions
- `primary.yaml` - Primary node configuration
- `replica1.yaml` - First replica configuration
- `replica2.yaml` - Second replica configuration

### Monitoring Configuration (`monitoring/`)
- `docker-compose.yml` - Docker Compose for basic monitoring
- `docker-compose-monitoring.yml` - Full monitoring stack with Prometheus/Grafana
- `prometheus.yml` - Prometheus server configuration
- `grafana-dashboard.json` - Grafana dashboard definitions

### RAM Daemon Configuration (`ramd/`)
- `ramd.conf` - Main RAM daemon configuration
- `ramd.json` - JSON format configuration
- `ramd.yaml` - YAML format configuration
- `ramd_auth.conf` - Authentication configuration
- `ramd_security.conf` - Security settings
- `ramd_test_password.conf` - Test password configuration

### RAM Controller Configuration (`ramctrl/`)
- `ramctrl.conf` - Main controller configuration
- `ramctrl_security.conf` - Controller security settings

### PostgreSQL Extension Configuration (`pgraft/`)
- (Future: pgraft-specific configuration files)

## Usage

### Starting with Configuration
```bash
# Start RAM daemon with specific config
./ramd -c conf/ramd/ramd.conf

# Start RAM controller with specific config
./ramctrl -c conf/ramctrl/ramctrl.conf

# Start monitoring stack
docker-compose -f conf/monitoring/docker-compose-monitoring.yml up
```

### Configuration Management
- All configuration files support hot-reloading
- Changes are automatically detected and applied
- Backup configurations are maintained in version control
- Environment-specific configs can be created as needed

## Best Practices

1. **Version Control**: All configuration files are tracked in git
2. **Documentation**: Each config file includes inline documentation
3. **Validation**: Configuration files are validated on startup
4. **Backup**: Always backup configurations before making changes
5. **Testing**: Test configurations in development before production

## Security Notes

- Sensitive information (passwords, keys) should be in separate files
- Use appropriate file permissions (600) for sensitive configs
- Consider using environment variables for secrets
- Regularly rotate passwords and keys
