/*-------------------------------------------------------------------------
 *
 * ramd_cluster_management.c
 *		PostgreSQL RAM Daemon - Cluster Management API Module
 *
 * This module provides comprehensive cluster management API endpoints
 * including switchover, configuration management, backup operations,
 * parameter validation, and cluster health monitoring.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "ramd_cluster_management.h"
#include "ramd_logging.h"
#include "ramd_postgresql.h"
#include "ramd_backup.h"
#include "ramd_postgresql_params.h"
#include "ramd_cluster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Handle switchover request
 */
bool
ramd_api_handle_switchover(ramd_http_request_t* req, ramd_http_response_t* resp)
{
    ramd_log_operation("Switchover request received");
    
    /* Parse request body */
    char target_node[256] = {0};
    char force[16] = {0};
    
    if (req->body && strlen(req->body) > 0) {
        /* Simple JSON parsing for target_node and force */
        char* target_ptr = strstr(req->body, "\"target_node\"");
        if (target_ptr) {
            char* value_start = strchr(target_ptr, ':');
            if (value_start) {
                value_start = strchr(value_start, '"');
                if (value_start) {
                    value_start++;
                    char* value_end = strchr(value_start, '"');
                    if (value_end) {
                        size_t len = value_end - value_start;
                        if (len < sizeof(target_node)) {
                            strncpy(target_node, value_start, len);
                        }
                    }
                }
            }
        }
        
        char* force_ptr = strstr(req->body, "\"force\"");
        if (force_ptr) {
            char* value_start = strchr(force_ptr, ':');
            if (value_start) {
                value_start = strchr(value_start, '"');
                if (value_start) {
                    value_start++;
                    char* value_end = strchr(value_start, '"');
                    if (value_end) {
                        size_t len = value_end - value_start;
                        if (len < sizeof(force)) {
                            strncpy(force, value_start, len);
                        }
                    }
                }
            }
        }
    }
    
    /* Validate target node */
    if (strlen(target_node) == 0) {
        resp->status_code = 400;
        strcpy(resp->body, "{\"error\":\"target_node is required\"}");
        return false;
    }
    
    /* Check if target node exists and is healthy */
    bool target_found = false;
    bool target_healthy = false;
    
    for (int i = 0; i < g_ramd_daemon->cluster.node_count; i++) {
        if (strcmp(g_ramd_daemon->cluster.nodes[i].hostname, target_node) == 0) {
            target_found = true;
            target_healthy = (g_ramd_daemon->cluster.nodes[i].is_healthy);
            break;
        }
    }
    
    if (!target_found) {
        resp->status_code = 404;
        strcpy(resp->body, "{\"error\":\"Target node not found\"}");
        return false;
    }
    
    if (!target_healthy && strcmp(force, "true") != 0) {
        resp->status_code = 400;
        strcpy(resp->body, "{\"error\":\"Target node is not healthy. Use force=true to override\"}");
        return false;
    }
    
    /* Perform switchover */
    bool switchover_success = false;
    char error_message[512] = {0};
    
    if (strcmp(force, "true") == 0) {
        switchover_success = ramd_cluster_force_switchover(target_node, error_message, sizeof(error_message));
    } else {
        switchover_success = ramd_cluster_switchover(target_node, error_message, sizeof(error_message));
    }
    
    if (switchover_success) {
        resp->status_code = 200;
        snprintf(resp->body, sizeof(resp->body),
            "{\"status\":\"success\",\"message\":\"Switchover completed\","
            "\"target_node\":\"%s\",\"timestamp\":%ld}",
            target_node, time(NULL));
        ramd_log_success("Switchover completed to node: %s", target_node);
    } else {
        resp->status_code = 500;
        snprintf(resp->body, sizeof(resp->body),
            "{\"status\":\"error\",\"message\":\"Switchover failed: %s\"}",
            error_message);
        ramd_log_failure("Switchover failed: %s", error_message);
    }
    
    return true;
}

/*
 * Handle configuration get request
 */
bool
ramd_api_handle_config_get(ramd_http_request_t* req, ramd_http_response_t* resp)
{
    ramd_log_operation("Configuration get request received");
    
    /* Get configuration section */
    char section[64] = {0};
    if (req->query_string) {
        char* section_ptr = strstr(req->query_string, "section=");
        if (section_ptr) {
            section_ptr += 8; /* Skip "section=" */
            char* end_ptr = strchr(section_ptr, '&');
            if (end_ptr) {
                size_t len = end_ptr - section_ptr;
                if (len < sizeof(section)) {
                    strncpy(section, section_ptr, len);
                }
            } else {
                strncpy(section, section_ptr, sizeof(section) - 1);
            }
        }
    }
    
    /* Build configuration JSON */
    char config_json[8192] = {0};
    char* ptr = config_json;
    size_t remaining = sizeof(config_json);
    int written = 0;
    
    written = snprintf(ptr, remaining, "{\"config\":{");
    if (written < 0 || (size_t)written >= remaining) {
        resp->status_code = 500;
        strcpy(resp->body, "{\"error\":\"Failed to build configuration response\"}");
        return false;
    }
    ptr += written;
    remaining -= written;
    
    /* Add cluster configuration */
    if (strlen(section) == 0 || strcmp(section, "cluster") == 0) {
        written = snprintf(ptr, remaining,
            "\"cluster\":{\"name\":\"%s\",\"node_count\":%d,\"local_node_id\":%d},",
            g_ramd_daemon->cluster.cluster_name,
            g_ramd_daemon->cluster.node_count,
            g_ramd_daemon->cluster.local_node_id);
        if (written < 0 || (size_t)written >= remaining) {
            resp->status_code = 500;
            strcpy(resp->body, "{\"error\":\"Failed to build configuration response\"}");
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    /* Add PostgreSQL configuration */
    if (strlen(section) == 0 || strcmp(section, "postgresql") == 0) {
        written = snprintf(ptr, remaining,
            "\"postgresql\":{\"host\":\"%s\",\"port\":%d,\"database\":\"%s\",\"user\":\"%s\"},",
            g_ramd_daemon->config.postgresql_host,
            g_ramd_daemon->config.postgresql_port,
            g_ramd_daemon->config.postgresql_database,
            g_ramd_daemon->config.postgresql_user);
        if (written < 0 || (size_t)written >= remaining) {
            resp->status_code = 500;
            strcpy(resp->body, "{\"error\":\"Failed to build configuration response\"}");
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    /* Add monitoring configuration */
    if (strlen(section) == 0 || strcmp(section, "monitoring") == 0) {
        written = snprintf(ptr, remaining,
            "\"monitoring\":{\"enabled\":%s,\"prometheus_port\":%d,\"metrics_interval\":\"%s\"},",
            g_ramd_daemon->config.monitoring_enabled ? "true" : "false",
            g_ramd_daemon->config.prometheus_port,
            g_ramd_daemon->config.metrics_interval);
        if (written < 0 || (size_t)written >= remaining) {
            resp->status_code = 500;
            strcpy(resp->body, "{\"error\":\"Failed to build configuration response\"}");
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    /* Add security configuration */
    if (strlen(section) == 0 || strcmp(section, "security") == 0) {
        written = snprintf(ptr, remaining,
            "\"security\":{\"enable_ssl\":%s,\"rate_limiting\":%s,\"audit_logging\":%s},",
            g_ramd_daemon->config.security_enable_ssl ? "true" : "false",
            g_ramd_daemon->config.security_rate_limiting ? "true" : "false",
            g_ramd_daemon->config.security_audit_logging ? "true" : "false");
        if (written < 0 || (size_t)written >= remaining) {
            resp->status_code = 500;
            strcpy(resp->body, "{\"error\":\"Failed to build configuration response\"}");
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    /* Remove trailing comma */
    if (ptr > config_json && *(ptr - 1) == ',') {
        ptr--;
        remaining++;
    }
    
    written = snprintf(ptr, remaining, "}}");
    if (written < 0 || (size_t)written >= remaining) {
        resp->status_code = 500;
        strcpy(resp->body, "{\"error\":\"Failed to build configuration response\"}");
        return false;
    }
    
    resp->status_code = 200;
    strcpy(resp->body, config_json);
    return true;
}

/*
 * Handle configuration set request
 */
bool
ramd_api_handle_config_set(ramd_http_request_t* req, ramd_http_response_t* resp)
{
    ramd_log_operation("Configuration set request received");
    
    if (!req->body || strlen(req->body) == 0) {
        resp->status_code = 400;
        strcpy(resp->body, "{\"error\":\"Request body is required\"}");
        return false;
    }
    
    /* Parse configuration from JSON */
    char section[64] = {0};
    char key[64] = {0};
    char value[256] = {0};
    
    /* Simple JSON parsing */
    char* section_ptr = strstr(req->body, "\"section\"");
    if (section_ptr) {
        char* value_start = strchr(section_ptr, ':');
        if (value_start) {
            value_start = strchr(value_start, '"');
            if (value_start) {
                value_start++;
                char* value_end = strchr(value_start, '"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < sizeof(section)) {
                        strncpy(section, value_start, len);
                    }
                }
            }
        }
    }
    
    char* key_ptr = strstr(req->body, "\"key\"");
    if (key_ptr) {
        char* value_start = strchr(key_ptr, ':');
        if (value_start) {
            value_start = strchr(value_start, '"');
            if (value_start) {
                value_start++;
                char* value_end = strchr(value_start, '"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < sizeof(key)) {
                        strncpy(key, value_start, len);
                    }
                }
            }
        }
    }
    
    char* value_ptr = strstr(req->body, "\"value\"");
    if (value_ptr) {
        char* value_start = strchr(value_ptr, ':');
        if (value_start) {
            value_start = strchr(value_start, '"');
            if (value_start) {
                value_start++;
                char* value_end = strchr(value_start, '"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < sizeof(value)) {
                        strncpy(value, value_start, len);
                    }
                }
            }
        }
    }
    
    if (strlen(section) == 0 || strlen(key) == 0 || strlen(value) == 0) {
        resp->status_code = 400;
        strcpy(resp->body, "{\"error\":\"section, key, and value are required\"}");
        return false;
    }
    
    /* Apply configuration based on section */
    bool config_applied = false;
    char error_message[512] = {0};
    
    if (strcmp(section, "postgresql") == 0) {
        config_applied = ramd_postgresql_set_parameter(key, value, error_message, sizeof(error_message));
    } else if (strcmp(section, "monitoring") == 0) {
        if (strcmp(key, "enabled") == 0) {
            g_ramd_daemon->config.monitoring_enabled = (strcmp(value, "true") == 0);
            config_applied = true;
        } else if (strcmp(key, "prometheus_port") == 0) {
            int port = atoi(value);
            if (port > 0 && port < 65536) {
                g_ramd_daemon->config.prometheus_port = port;
                config_applied = true;
            } else {
                strcpy(error_message, "Invalid port number");
            }
        } else if (strcmp(key, "metrics_interval") == 0) {
            strncpy(g_ramd_daemon->config.metrics_interval, value, sizeof(g_ramd_daemon->config.metrics_interval) - 1);
            config_applied = true;
        }
    } else if (strcmp(section, "security") == 0) {
        if (strcmp(key, "enable_ssl") == 0) {
            g_ramd_daemon->config.security_enable_ssl = (strcmp(value, "true") == 0);
            config_applied = true;
        } else if (strcmp(key, "rate_limiting") == 0) {
            g_ramd_daemon->config.security_rate_limiting = (strcmp(value, "true") == 0);
            config_applied = true;
        } else if (strcmp(key, "audit_logging") == 0) {
            g_ramd_daemon->config.security_audit_logging = (strcmp(value, "true") == 0);
            config_applied = true;
        }
    }
    
    if (config_applied) {
        resp->status_code = 200;
        snprintf(resp->body, sizeof(resp->body),
            "{\"status\":\"success\",\"message\":\"Configuration updated\","
            "\"section\":\"%s\",\"key\":\"%s\",\"value\":\"%s\"}",
            section, key, value);
        ramd_log_success("Configuration updated: %s.%s = %s", section, key, value);
    } else {
        resp->status_code = 400;
        snprintf(resp->body, sizeof(resp->body),
            "{\"status\":\"error\",\"message\":\"Failed to update configuration: %s\"}",
            error_message);
        ramd_log_failure("Configuration update failed: %s", error_message);
    }
    
    return true;
}

/*
 * Handle backup start request
 */
bool
ramd_api_handle_backup_start(ramd_http_request_t* req, ramd_http_response_t* resp)
{
    ramd_log_operation("Backup start request received");
    
    /* Parse request body */
    char tool_name[64] = {0};
    char backup_name[128] = {0};
    
    if (req->body && strlen(req->body) > 0) {
        char* tool_ptr = strstr(req->body, "\"tool_name\"");
        if (tool_ptr) {
            char* value_start = strchr(tool_ptr, ':');
            if (value_start) {
                value_start = strchr(value_start, '"');
                if (value_start) {
                    value_start++;
                    char* value_end = strchr(value_start, '"');
                    if (value_end) {
                        size_t len = value_end - value_start;
                        if (len < sizeof(tool_name)) {
                            strncpy(tool_name, value_start, len);
                        }
                    }
                }
            }
        }
        
        char* backup_ptr = strstr(req->body, "\"backup_name\"");
        if (backup_ptr) {
            char* value_start = strchr(backup_ptr, ':');
            if (value_start) {
                value_start = strchr(value_start, '"');
                if (value_start) {
                    value_start++;
                    char* value_end = strchr(value_start, '"');
                    if (value_end) {
                        size_t len = value_end - value_start;
                        if (len < sizeof(backup_name)) {
                            strncpy(backup_name, value_start, len);
                        }
                    }
                }
            }
        }
    }
    
    if (strlen(tool_name) == 0) {
        resp->status_code = 400;
        strcpy(resp->body, "{\"error\":\"tool_name is required\"}");
        return false;
    }
    
    /* Start backup */
    char error_message[512] = {0};
    bool backup_started = ramd_backup_create(tool_name, backup_name, error_message, sizeof(error_message));
    
    if (backup_started) {
        resp->status_code = 200;
        snprintf(resp->body, sizeof(resp->body),
            "{\"status\":\"success\",\"message\":\"Backup started\","
            "\"tool_name\":\"%s\",\"backup_name\":\"%s\",\"timestamp\":%ld}",
            tool_name, backup_name, time(NULL));
        ramd_log_success("Backup started: %s", tool_name);
    } else {
        resp->status_code = 500;
        snprintf(resp->body, sizeof(resp->body),
            "{\"status\":\"error\",\"message\":\"Failed to start backup: %s\"}",
            error_message);
        ramd_log_failure("Backup start failed: %s", error_message);
    }
    
    return true;
}

/*
 * Handle cluster status request
 */
bool
ramd_api_handle_cluster_status(ramd_http_request_t* req, ramd_http_response_t* resp)
{
    ramd_log_operation("Cluster status request received");
    
    /* Build comprehensive cluster status */
    char status_json[4096] = {0};
    char* ptr = status_json;
    size_t remaining = sizeof(status_json);
    int written = 0;
    
    written = snprintf(ptr, remaining,
        "{\"cluster\":{\"name\":\"%s\",\"node_count\":%d,\"local_node_id\":%d,"
        "\"leader\":\"%s\",\"phase\":\"%s\",\"healthy_nodes\":%d},",
        g_ramd_daemon->cluster.cluster_name,
        g_ramd_daemon->cluster.node_count,
        g_ramd_daemon->cluster.local_node_id,
        g_ramd_daemon->cluster.leader,
        g_ramd_daemon->cluster.phase,
        g_ramd_daemon->cluster.healthy_nodes);
    if (written < 0 || (size_t)written >= remaining) {
        resp->status_code = 500;
        strcpy(resp->body, "{\"error\":\"Failed to build status response\"}");
        return false;
    }
    ptr += written;
    remaining -= written;
    
    /* Add nodes array */
    written = snprintf(ptr, remaining, "\"nodes\":[");
    if (written < 0 || (size_t)written >= remaining) {
        resp->status_code = 500;
        strcpy(resp->body, "{\"error\":\"Failed to build status response\"}");
        return false;
    }
    ptr += written;
    remaining -= written;
    
    for (int i = 0; i < g_ramd_daemon->cluster.node_count; i++) {
        if (i > 0) {
            written = snprintf(ptr, remaining, ",");
            if (written < 0 || (size_t)written >= remaining) {
                resp->status_code = 500;
                strcpy(resp->body, "{\"error\":\"Failed to build status response\"}");
                return false;
            }
            ptr += written;
            remaining -= written;
        }
        
        written = snprintf(ptr, remaining,
            "{\"id\":%d,\"hostname\":\"%s\",\"port\":%d,\"role\":\"%s\","
            "\"state\":\"%s\",\"is_healthy\":%s,\"last_seen\":%ld,"
            "\"replication_lag_ms\":%ld}",
            g_ramd_daemon->cluster.nodes[i].id,
            g_ramd_daemon->cluster.nodes[i].hostname,
            g_ramd_daemon->cluster.nodes[i].port,
            g_ramd_daemon->cluster.nodes[i].role == RAMD_ROLE_PRIMARY ? "primary" : "standby",
            g_ramd_daemon->cluster.nodes[i].state == RAMD_NODE_STATE_PRIMARY ? "primary" : "standby",
            g_ramd_daemon->cluster.nodes[i].is_healthy ? "true" : "false",
            g_ramd_daemon->cluster.nodes[i].last_seen,
            g_ramd_daemon->cluster.nodes[i].replication_lag_ms);
        
        if (written < 0 || (size_t)written >= remaining) {
            resp->status_code = 500;
            strcpy(resp->body, "{\"error\":\"Failed to build status response\"}");
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    written = snprintf(ptr, remaining, "],\"timestamp\":%ld}", time(NULL));
    if (written < 0 || (size_t)written >= remaining) {
        resp->status_code = 500;
        strcpy(resp->body, "{\"error\":\"Failed to build status response\"}");
        return false;
    }
    
    resp->status_code = 200;
    strcpy(resp->body, status_json);
    return true;
}

/*
 * Handle parameter validation request
 */
bool
ramd_api_handle_parameter_validate(ramd_http_request_t* req, ramd_http_response_t* resp)
{
    ramd_log_operation("Parameter validation request received");
    
    if (!req->body || strlen(req->body) == 0) {
        resp->status_code = 400;
        strcpy(resp->body, "{\"error\":\"Request body is required\"}");
        return false;
    }
    
    /* Parse parameter name and value */
    char param_name[64] = {0};
    char param_value[256] = {0};
    int pg_version = 17; /* Default to PostgreSQL 17 */
    
    /* Simple JSON parsing */
    char* name_ptr = strstr(req->body, "\"name\"");
    if (name_ptr) {
        char* value_start = strchr(name_ptr, ':');
        if (value_start) {
            value_start = strchr(value_start, '"');
            if (value_start) {
                value_start++;
                char* value_end = strchr(value_start, '"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < sizeof(param_name)) {
                        strncpy(param_name, value_start, len);
                    }
                }
            }
        }
    }
    
    char* value_ptr = strstr(req->body, "\"value\"");
    if (value_ptr) {
        char* value_start = strchr(value_ptr, ':');
        if (value_start) {
            value_start = strchr(value_start, '"');
            if (value_start) {
                value_start++;
                char* value_end = strchr(value_start, '"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < sizeof(param_value)) {
                        strncpy(param_value, value_start, len);
                    }
                }
            }
        }
    }
    
    char* version_ptr = strstr(req->body, "\"version\"");
    if (version_ptr) {
        char* value_start = strchr(version_ptr, ':');
        if (value_start) {
            pg_version = atoi(value_start + 1);
        }
    }
    
    if (strlen(param_name) == 0 || strlen(param_value) == 0) {
        resp->status_code = 400;
        strcpy(resp->body, "{\"error\":\"name and value are required\"}");
        return false;
    }
    
    /* Validate parameter */
    ramd_parameter_validation_result_t result;
    bool valid = ramd_postgresql_validate_parameter(param_name, param_value, pg_version, &result);
    
    if (valid) {
        resp->status_code = 200;
        snprintf(resp->body, sizeof(resp->body),
            "{\"status\":\"success\",\"valid\":true,\"restart_required\":%s,"
            "\"superuser_required\":%s,\"suggested_value\":\"%s\"}",
            result.restart_required ? "true" : "false",
            result.superuser_required ? "true" : "false",
            result.suggested_value);
    } else {
        resp->status_code = 400;
        snprintf(resp->body, sizeof(resp->body),
            "{\"status\":\"error\",\"valid\":false,\"error_message\":\"%s\"}",
            result.error_message);
    }
    
    return true;
}
