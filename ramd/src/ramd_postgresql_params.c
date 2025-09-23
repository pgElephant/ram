/*
 * PostgreSQL RAM Daemon - Parameter Validation Module
 * 
 * This module provides automatic PostgreSQL parameter validation,
 * version-specific handling, and optimization.
 */

#include "ramd_postgresql_params.h"
#include "ramd_logging.h"
#include "ramd_postgresql.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PostgreSQL parameter definitions */
typedef struct {
    char name[64];
    char type[16];           /* "bool", "int", "float", "string", "enum" */
    int min_version;
    int max_version;
    bool required;
    char* valid_values[];
    char* description;
    char* default_value;
    char* min_value;
    char* max_value;
    bool restart_required;
    bool superuser_required;
} ramd_postgresql_parameter_t;

/* Parameter validation result */
typedef struct {
    bool valid;
    char error_message[512];
    char suggested_value[256];
    bool restart_required;
    bool superuser_required;
} ramd_parameter_validation_result_t;

/* Global parameter definitions */
static ramd_postgresql_parameter_t g_parameters[] = {
    /* Connection parameters */
    {
        .name = "max_connections",
        .type = "int",
        .min_version = 70000,
        .max_version = 0,
        .required = true,
        .description = "Maximum number of concurrent connections",
        .default_value = "100",
        .min_value = "1",
        .max_value = "262143",
        .restart_required = true,
        .superuser_required = true
    },
    {
        .name = "shared_buffers",
        .type = "string",
        .min_version = 70000,
        .max_version = 0,
        .required = true,
        .description = "Amount of memory for shared buffers",
        .default_value = "128MB",
        .min_value = "128kB",
        .max_value = "1TB",
        .restart_required = true,
        .superuser_required = true
    },
    {
        .name = "wal_level",
        .type = "enum",
        .min_version = 70000,
        .max_version = 0,
        .required = true,
        .valid_values = (char*[]){"minimal", "replica", "logical", NULL},
        .description = "Level of WAL logging",
        .default_value = "replica",
        .restart_required = true,
        .superuser_required = true
    },
    {
        .name = "max_wal_senders",
        .type = "int",
        .min_version = 70000,
        .max_version = 0,
        .required = false,
        .description = "Maximum number of WAL sender processes",
        .default_value = "10",
        .min_value = "0",
        .max_value = "262143",
        .restart_required = true,
        .superuser_required = true
    },
    {
        .name = "max_replication_slots",
        .type = "int",
        .min_version = 70000,
        .max_version = 0,
        .required = false,
        .description = "Maximum number of replication slots",
        .default_value = "10",
        .min_value = "0",
        .max_value = "262143",
        .restart_required = true,
        .superuser_required = true
    },
    {
        .name = "hot_standby",
        .type = "bool",
        .min_version = 70000,
        .max_version = 0,
        .required = false,
        .description = "Enable hot standby mode",
        .default_value = "on",
        .restart_required = true,
        .superuser_required = true
    },
    {
        .name = "archive_mode",
        .type = "bool",
        .min_version = 70000,
        .max_version = 0,
        .required = false,
        .description = "Enable archiving of WAL files",
        .default_value = "off",
        .restart_required = true,
        .superuser_required = true
    },
    {
        .name = "archive_command",
        .type = "string",
        .min_version = 70000,
        .max_version = 0,
        .required = false,
        .description = "Command to execute for archiving WAL files",
        .default_value = "",
        .restart_required = false,
        .superuser_required = true
    },
    {
        .name = "restore_command",
        .type = "string",
        .min_version = 70000,
        .max_version = 0,
        .required = false,
        .description = "Command to execute for restoring WAL files",
        .default_value = "",
        .restart_required = false,
        .superuser_required = false
    },
    {
        .name = "synchronous_standby_names",
        .type = "string",
        .min_version = 70000,
        .max_version = 0,
        .required = false,
        .description = "List of synchronous standby names",
        .default_value = "",
        .restart_required = false,
        .superuser_required = true
    },
    {
        .name = "synchronous_commit",
        .type = "enum",
        .min_version = 70000,
        .max_version = 0,
        .required = false,
        .valid_values = (char*[]){"off", "local", "remote_write", "on", "remote_apply", NULL},
        .description = "Synchronous commit level",
        .default_value = "on",
        .restart_required = false,
        .superuser_required = false
    }
};

/* Parameter count */
#define RAMD_PARAMETER_COUNT (sizeof(g_parameters) / sizeof(g_parameters[0]))

/*
 * Validate PostgreSQL parameter
 */
bool
ramd_postgresql_validate_parameter(const char* name, const char* value, 
                                  int pg_version, ramd_parameter_validation_result_t* result)
{
    /* Find parameter definition */
    ramd_postgresql_parameter_t* param = NULL;
    for (int i = 0; i < RAMD_PARAMETER_COUNT; i++) {
        if (strcmp(g_parameters[i].name, name) == 0) {
            param = &g_parameters[i];
            break;
        }
    }
    
    if (!param) {
        snprintf(result->error_message, sizeof(result->error_message), 
                "Unknown parameter: %s", name);
        result->valid = false;
        return false;
    }
    
    /* Check version compatibility */
    if (pg_version < param->min_version) {
        snprintf(result->error_message, sizeof(result->error_message), 
                "Parameter %s requires PostgreSQL %d or later", name, param->min_version);
        result->valid = false;
        return false;
    }
    
    if (param->max_version > 0 && pg_version > param->max_version) {
        snprintf(result->error_message, sizeof(result->error_message), 
                "Parameter %s is not supported in PostgreSQL %d", name, pg_version);
        result->valid = false;
        return false;
    }
    
    /* Validate value based on type */
    if (strcmp(param->type, "bool") == 0) {
        if (strcmp(value, "on") != 0 && strcmp(value, "off") != 0 && 
            strcmp(value, "true") != 0 && strcmp(value, "false") != 0 &&
            strcmp(value, "yes") != 0 && strcmp(value, "no") != 0 &&
            strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Invalid boolean value for %s: %s (expected: on/off, true/false, yes/no, 1/0)", 
                    name, value);
            result->valid = false;
            return false;
        }
    } else if (strcmp(param->type, "int") == 0) {
        char* endptr;
        long int_val = strtol(value, &endptr, 10);
        if (*endptr != '\0') {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Invalid integer value for %s: %s", name, value);
            result->valid = false;
            return false;
        }
        
        if (param->min_value && int_val < strtol(param->min_value, NULL, 10)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Value for %s too small: %s (minimum: %s)", name, value, param->min_value);
            result->valid = false;
            return false;
        }
        
        if (param->max_value && int_val > strtol(param->max_value, NULL, 10)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Value for %s too large: %s (maximum: %s)", name, value, param->max_value);
            result->valid = false;
            return false;
        }
    } else if (strcmp(param->type, "float") == 0) {
        char* endptr;
        double float_val = strtod(value, &endptr);
        if (*endptr != '\0') {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Invalid float value for %s: %s", name, value);
            result->valid = false;
            return false;
        }
        
        if (param->min_value && float_val < strtod(param->min_value, NULL)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Value for %s too small: %s (minimum: %s)", name, value, param->min_value);
            result->valid = false;
            return false;
        }
        
        if (param->max_value && float_val > strtod(param->max_value, NULL)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Value for %s too large: %s (maximum: %s)", name, value, param->max_value);
            result->valid = false;
            return false;
        }
    } else if (strcmp(param->type, "enum") == 0) {
        bool valid_enum = false;
        for (int i = 0; param->valid_values[i] != NULL; i++) {
            if (strcmp(param->valid_values[i], value) == 0) {
                valid_enum = true;
                break;
            }
        }
        
        if (!valid_enum) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Invalid enum value for %s: %s (valid values: ", name, value);
            size_t len = strlen(result->error_message);
            for (int i = 0; param->valid_values[i] != NULL; i++) {
                if (i > 0) {
                    strncat(result->error_message + len, ", ", 
                           sizeof(result->error_message) - len - 1);
                    len += 2;
                }
                strncat(result->error_message + len, param->valid_values[i], 
                       sizeof(result->error_message) - len - 1);
                len += strlen(param->valid_values[i]);
            }
            strncat(result->error_message + len, ")", sizeof(result->error_message) - len - 1);
            result->valid = false;
            return false;
        }
    }
    
    /* Set result */
    result->valid = true;
    result->restart_required = param->restart_required;
    result->superuser_required = param->superuser_required;
    strcpy(result->suggested_value, value);
    
    return true;
}

/*
 * Optimize PostgreSQL parameters
 */
bool
ramd_postgresql_optimize_parameters(ramd_config_t* config, char* optimized_config, size_t config_size)
{
    char* ptr = optimized_config;
    size_t remaining = config_size;
    int written = 0;
    
    /* Start with basic optimization */
    written = snprintf(ptr, remaining, "# Optimized PostgreSQL configuration\n");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    ptr += written;
    remaining -= written;
    
    /* Memory optimization */
    written = snprintf(ptr, remaining, 
        "# Memory settings\n"
        "shared_buffers = '256MB'\n"
        "effective_cache_size = '1GB'\n"
        "work_mem = '4MB'\n"
        "maintenance_work_mem = '64MB'\n"
        "wal_buffers = '16MB'\n");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    ptr += written;
    remaining -= written;
    
    /* Connection optimization */
    written = snprintf(ptr, remaining, 
        "# Connection settings\n"
        "max_connections = 100\n"
        "superuser_reserved_connections = 3\n");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    ptr += written;
    remaining -= written;
    
    /* WAL optimization */
    written = snprintf(ptr, remaining, 
        "# WAL settings\n"
        "wal_level = replica\n"
        "max_wal_senders = 10\n"
        "max_replication_slots = 10\n"
        "wal_keep_segments = 32\n"
        "checkpoint_completion_target = 0.9\n");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    ptr += written;
    remaining -= written;
    
    /* Replication optimization */
    written = snprintf(ptr, remaining, 
        "# Replication settings\n"
        "hot_standby = on\n"
        "synchronous_commit = on\n"
        "synchronous_standby_names = ''\n");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    ptr += written;
    remaining -= written;
    
    /* Logging optimization */
    written = snprintf(ptr, remaining, 
        "# Logging settings\n"
        "log_destination = 'stderr'\n"
        "logging_collector = on\n"
        "log_directory = 'pg_log'\n"
        "log_filename = 'postgresql-%%Y-%%m-%%d_%%H%%M%%S.log'\n"
        "log_rotation_age = 1d\n"
        "log_rotation_size = 100MB\n"
        "log_min_duration_statement = 1000\n"
        "log_line_prefix = '%%t [%%p]: [%%l-1] user=%%u,db=%%d,app=%%a,client=%%h '\n");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    ptr += written;
    remaining -= written;
    
    return true;
}

/*
 * Get parameter information
 */
bool
ramd_postgresql_get_parameter_info(const char* name, char* info, size_t info_size)
{
    /* Find parameter definition */
    ramd_postgresql_parameter_t* param = NULL;
    for (int i = 0; i < RAMD_PARAMETER_COUNT; i++) {
        if (strcmp(g_parameters[i].name, name) == 0) {
            param = &g_parameters[i];
            break;
        }
    }
    
    if (!param) {
        return false;
    }
    
    /* Build parameter information */
    int written = snprintf(info, info_size,
        "Parameter: %s\n"
        "Type: %s\n"
        "Description: %s\n"
        "Default: %s\n"
        "Min Version: %d\n"
        "Max Version: %d\n"
        "Required: %s\n"
        "Restart Required: %s\n"
        "Superuser Required: %s\n",
        param->name,
        param->type,
        param->description,
        param->default_value,
        param->min_version,
        param->max_version,
        param->required ? "Yes" : "No",
        param->restart_required ? "Yes" : "No",
        param->superuser_required ? "Yes" : "No");
    
    if (param->min_value) {
        written += snprintf(info + written, info_size - written, "Min Value: %s\n", param->min_value);
    }
    
    if (param->max_value) {
        written += snprintf(info + written, info_size - written, "Max Value: %s\n", param->max_value);
    }
    
    if (param->valid_values && param->valid_values[0]) {
        written += snprintf(info + written, info_size - written, "Valid Values: ");
        for (int i = 0; param->valid_values[i] != NULL; i++) {
            if (i > 0) {
                written += snprintf(info + written, info_size - written, ", ");
            }
            written += snprintf(info + written, info_size - written, "%s", param->valid_values[i]);
        }
        written += snprintf(info + written, info_size - written, "\n");
    }
    
    return (written > 0 && (size_t)written < info_size);
}

/*
 * List all parameters
 */
bool
ramd_postgresql_list_parameters(char* output, size_t output_size)
{
    char* ptr = output;
    size_t remaining = output_size;
    int written = 0;
    
    written = snprintf(ptr, remaining, "{\"parameters\":[");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    ptr += written;
    remaining -= written;
    
    for (int i = 0; i < RAMD_PARAMETER_COUNT; i++) {
        ramd_postgresql_parameter_t* param = &g_parameters[i];
        
        if (i > 0) {
            written = snprintf(ptr, remaining, ",");
            if (written < 0 || (size_t)written >= remaining) {
                return false;
            }
            ptr += written;
            remaining -= written;
        }
        
        written = snprintf(ptr, remaining,
            "{\"name\":\"%s\",\"type\":\"%s\",\"description\":\"%s\","
            "\"default\":\"%s\",\"min_version\":%d,\"max_version\":%d,"
            "\"required\":%s,\"restart_required\":%s,\"superuser_required\":%s",
            param->name, param->type, param->description,
            param->default_value, param->min_version, param->max_version,
            param->required ? "true" : "false",
            param->restart_required ? "true" : "false",
            param->superuser_required ? "true" : "false");
        
        if (written < 0 || (size_t)written >= remaining) {
            return false;
        }
        ptr += written;
        remaining -= written;
        
        if (param->min_value) {
            written = snprintf(ptr, remaining, ",\"min_value\":\"%s\"", param->min_value);
            if (written < 0 || (size_t)written >= remaining) {
                return false;
            }
            ptr += written;
            remaining -= written;
        }
        
        if (param->max_value) {
            written = snprintf(ptr, remaining, ",\"max_value\":\"%s\"", param->max_value);
            if (written < 0 || (size_t)written >= remaining) {
                return false;
            }
            ptr += written;
            remaining -= written;
        }
        
        if (param->valid_values && param->valid_values[0]) {
            written = snprintf(ptr, remaining, ",\"valid_values\":[");
            if (written < 0 || (size_t)written >= remaining) {
                return false;
            }
            ptr += written;
            remaining -= written;
            
            for (int j = 0; param->valid_values[j] != NULL; j++) {
                if (j > 0) {
                    written = snprintf(ptr, remaining, ",");
                    if (written < 0 || (size_t)written >= remaining) {
                        return false;
                    }
                    ptr += written;
                    remaining -= written;
                }
                
                written = snprintf(ptr, remaining, "\"%s\"", param->valid_values[j]);
                if (written < 0 || (size_t)written >= remaining) {
                    return false;
                }
                ptr += written;
                remaining -= written;
            }
            
            written = snprintf(ptr, remaining, "]");
            if (written < 0 || (size_t)written >= remaining) {
                return false;
            }
            ptr += written;
            remaining -= written;
        }
        
        written = snprintf(ptr, remaining, "}");
        if (written < 0 || (size_t)written >= remaining) {
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    written = snprintf(ptr, remaining, "]}");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    
    return true;
}
