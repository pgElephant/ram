/*
 * PostgreSQL RAM Daemon - Parameter Validation Header
 * 
 * This header defines the PostgreSQL parameter validation API.
 */

#ifndef RAMD_POSTGRESQL_PARAMS_H
#define RAMD_POSTGRESQL_PARAMS_H

#include "ramd_common.h"

/* Parameter validation result structure */
typedef struct {
    bool valid;
    char error_message[512];
    char suggested_value[256];
    bool restart_required;
    bool superuser_required;
} ramd_parameter_validation_result_t;

/* Function prototypes */

/*
 * Validate PostgreSQL parameter
 */
bool ramd_postgresql_validate_parameter(const char* name, const char* value, 
                                       int pg_version, ramd_parameter_validation_result_t* result);

/*
 * Optimize PostgreSQL parameters
 */
bool ramd_postgresql_optimize_parameters(ramd_config_t* config, char* optimized_config, size_t config_size);

/*
 * Get parameter information
 */
bool ramd_postgresql_get_parameter_info(const char* name, char* info, size_t info_size);

/*
 * List all parameters
 */
bool ramd_postgresql_list_parameters(char* output, size_t output_size);

/*
 * Validate all parameters in configuration
 */
bool ramd_postgresql_validate_all_parameters(ramd_config_t* config, char* errors, size_t error_size);

/*
 * Get parameter default value
 */
bool ramd_postgresql_get_parameter_default(const char* name, char* default_value, size_t value_size);

/*
 * Check if parameter requires restart
 */
bool ramd_postgresql_parameter_requires_restart(const char* name);

/*
 * Check if parameter requires superuser
 */
bool ramd_postgresql_parameter_requires_superuser(const char* name);

/*
 * Get parameter type
 */
bool ramd_postgresql_get_parameter_type(const char* name, char* type, size_t type_size);

/*
 * Get parameter description
 */
bool ramd_postgresql_get_parameter_description(const char* name, char* description, size_t desc_size);

/*
 * Get parameter valid values (for enum types)
 */
bool ramd_postgresql_get_parameter_valid_values(const char* name, char* values, size_t values_size);

/*
 * Get parameter min/max values
 */
bool ramd_postgresql_get_parameter_limits(const char* name, char* min_value, char* max_value, 
                                         size_t value_size);

/*
 * Check parameter version compatibility
 */
bool ramd_postgresql_check_parameter_version(const char* name, int pg_version);

/*
 * Get parameter category
 */
bool ramd_postgresql_get_parameter_category(const char* name, char* category, size_t category_size);

/*
 * Get parameter impact level
 */
bool ramd_postgresql_get_parameter_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter recommendations
 */
bool ramd_postgresql_get_parameter_recommendations(const char* name, char* recommendations, size_t rec_size);

/*
 * Get parameter examples
 */
bool ramd_postgresql_get_parameter_examples(const char* name, char* examples, size_t examples_size);

/*
 * Get parameter related parameters
 */
bool ramd_postgresql_get_parameter_related(const char* name, char* related, size_t related_size);

/*
 * Get parameter conflicts
 */
bool ramd_postgresql_get_parameter_conflicts(const char* name, char* conflicts, size_t conflicts_size);

/*
 * Get parameter dependencies
 */
bool ramd_postgresql_get_parameter_dependencies(const char* name, char* dependencies, size_t deps_size);

/*
 * Get parameter performance impact
 */
bool ramd_postgresql_get_parameter_performance_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter security impact
 */
bool ramd_postgresql_get_parameter_security_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter memory impact
 */
bool ramd_postgresql_get_parameter_memory_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter disk impact
 */
bool ramd_postgresql_get_parameter_disk_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter network impact
 */
bool ramd_postgresql_get_parameter_network_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter CPU impact
 */
bool ramd_postgresql_get_parameter_cpu_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter I/O impact
 */
bool ramd_postgresql_get_parameter_io_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter concurrency impact
 */
bool ramd_postgresql_get_parameter_concurrency_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter replication impact
 */
bool ramd_postgresql_get_parameter_replication_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter backup impact
 */
bool ramd_postgresql_get_parameter_backup_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter recovery impact
 */
bool ramd_postgresql_get_parameter_recovery_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter monitoring impact
 */
bool ramd_postgresql_get_parameter_monitoring_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter logging impact
 */
bool ramd_postgresql_get_parameter_logging_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter debugging impact
 */
bool ramd_postgresql_get_parameter_debugging_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter testing impact
 */
bool ramd_postgresql_get_parameter_testing_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter development impact
 */
bool ramd_postgresql_get_parameter_development_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter production impact
 */
bool ramd_postgresql_get_parameter_production_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter staging impact
 */
bool ramd_postgresql_get_parameter_staging_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter testing impact
 */
bool ramd_postgresql_get_parameter_testing_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter development impact
 */
bool ramd_postgresql_get_parameter_development_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter production impact
 */
bool ramd_postgresql_get_parameter_production_impact(const char* name, char* impact, size_t impact_size);

/*
 * Get parameter staging impact
 */
bool ramd_postgresql_get_parameter_staging_impact(const char* name, char* impact, size_t impact_size);

#endif /* RAMD_POSTGRESQL_PARAMS_H */
