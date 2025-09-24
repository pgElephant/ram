#ifndef RAM_DEFAULTS_H
#define RAM_DEFAULTS_H

/* Default configuration values */
#define RAM_DEFAULT_POSTGRESQL_HOST "127.0.0.1"
#define RAM_DEFAULT_POSTGRESQL_PORT 5432
#define RAM_DEFAULT_API_PORT 8080
#define RAM_DEFAULT_HTTP_PORT 8080
#define RAM_DEFAULT_CLUSTER_NAME "default_cluster"
#define RAM_DEFAULT_USER "postgres"
#define RAM_DEFAULT_DATABASE "postgres"

/* Configuration keys */
#define RAM_CONFIG_POSTGRESQL_HOST "postgresql_host"
#define RAM_CONFIG_POSTGRESQL_PORT "postgresql_port"
#define RAM_CONFIG_API_PORT "api_port"
#define RAM_CONFIG_HTTP_PORT "http_port"
#define RAM_CONFIG_CLUSTER_NAME "cluster_name"
#define RAM_CONFIG_USER "user"
#define RAM_CONFIG_DATABASE "database"

/* Environment variable names */
#define RAM_ENV_API_URL "RAMCTRL_API_URL"
#define RAM_ENV_POSTGRESQL_HOST "RAM_POSTGRESQL_HOST"
#define RAM_ENV_POSTGRESQL_PORT "RAM_POSTGRESQL_PORT"

#endif /* RAM_DEFAULTS_H */
