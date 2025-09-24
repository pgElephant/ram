/*
 * ramd_postgresql_auth.c - PostgreSQL Authentication Support for ramd
 * 
 * This file implements comprehensive PostgreSQL authentication methods
 * including password, SSL, Kerberos, LDAP, and other authentication types.
 */

#include "ramd_postgresql_auth.h"
#include "ramd_logging.h"
#include "ramd_config.h"
#include <libpq-fe.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* Authentication method and context definitions are in the header file */

/* Global authentication context */
ramd_auth_context_t g_auth_context = {0};

/* Function declarations */
static ramd_auth_method_t ramd_auth_parse_method(const char* method_str);
static char* ramd_auth_build_conninfo(const ramd_auth_context_t* auth_ctx);
static bool ramd_auth_validate_kerberos_config(const ramd_auth_context_t* auth_ctx);
static bool ramd_auth_validate_ldap_config(const ramd_auth_context_t* auth_ctx);
static bool ramd_auth_validate_pam_config(const ramd_auth_context_t* auth_ctx);

/* Public functions */
bool
ramd_postgresql_auth_init(const ramd_config_t* config)
{
    ramd_log_info("Initializing PostgreSQL authentication system");
    
    /* Clear existing context */
    memset(&g_auth_context, 0, sizeof(ramd_auth_context_t));
    
    /* Set basic connection parameters */
    g_auth_context.username = strdup(config->database_user);
    g_auth_context.password = strdup(config->database_password);
    g_auth_context.database = strdup(config->database_name);
    g_auth_context.hostname = strdup(config->hostname);
    g_auth_context.port = config->postgresql_port;
    
    /* Set SSL parameters */
    g_auth_context.ssl_mode = strdup("prefer");
    g_auth_context.require_ssl = false;
    g_auth_context.verify_ssl = true;
    
    /* Set default authentication method */
    g_auth_context.method = RAMD_AUTH_METHOD_PASSWORD;
    
    ramd_log_info("PostgreSQL authentication system initialized");
    return true;
}

bool
ramd_postgresql_auth_configure_ssl(const char* ssl_cert, const char* ssl_key, 
                                   const char* ssl_ca, const char* ssl_mode)
{
    if (!ssl_cert || !ssl_key)
    {
        ramd_log_error("SSL certificate and key are required for SSL authentication");
        return false;
    }
    
    /* Validate SSL certificates exist */
    if (access(ssl_cert, R_OK) != 0)
    {
        ramd_log_error("SSL certificate file not accessible: %s", ssl_cert);
        return false;
    }
    
    if (access(ssl_key, R_OK) != 0)
    {
        ramd_log_error("SSL key file not accessible: %s", ssl_key);
        return false;
    }
    
    /* Set SSL parameters */
    if (g_auth_context.ssl_cert) free(g_auth_context.ssl_cert);
    if (g_auth_context.ssl_key) free(g_auth_context.ssl_key);
    if (g_auth_context.ssl_ca) free(g_auth_context.ssl_ca);
    if (g_auth_context.ssl_mode) free(g_auth_context.ssl_mode);
    
    g_auth_context.ssl_cert = strdup(ssl_cert);
    g_auth_context.ssl_key = strdup(ssl_key);
    g_auth_context.ssl_ca = ssl_ca ? strdup(ssl_ca) : NULL;
    g_auth_context.ssl_mode = ssl_mode ? strdup(ssl_mode) : strdup("require");
    g_auth_context.require_ssl = true;
    
    /* Set authentication method to certificate-based */
    g_auth_context.method = RAMD_AUTH_METHOD_CERT;
    
    ramd_log_info("SSL authentication configured: cert=%s, key=%s, mode=%s", 
                  ssl_cert, ssl_key, g_auth_context.ssl_mode);
    return true;
}

bool
ramd_postgresql_auth_configure_kerberos(const char* service_name)
{
    if (!service_name)
    {
        ramd_log_error("Kerberos service name is required");
        return false;
    }
    
    /* Validate Kerberos configuration */
    if (!ramd_auth_validate_kerberos_config(&g_auth_context))
    {
        return false;
    }
    
    /* Set Kerberos parameters */
    if (g_auth_context.kerberos_service) free(g_auth_context.kerberos_service);
    g_auth_context.kerberos_service = strdup(service_name);
    g_auth_context.method = RAMD_AUTH_METHOD_KERBEROS;
    
    ramd_log_info("Kerberos authentication configured: service=%s", service_name);
    return true;
}

bool
ramd_postgresql_auth_configure_ldap(const char* server, const char* port,
                                    const char* basedn, const char* binddn,
                                    const char* bindpasswd)
{
    if (!server || !basedn)
    {
        ramd_log_error("LDAP server and base DN are required");
        return false;
    }
    
    /* Set LDAP parameters */
    if (g_auth_context.ldap_server) free(g_auth_context.ldap_server);
    if (g_auth_context.ldap_port) free(g_auth_context.ldap_port);
    if (g_auth_context.ldap_basedn) free(g_auth_context.ldap_basedn);
    if (g_auth_context.ldap_binddn) free(g_auth_context.ldap_binddn);
    if (g_auth_context.ldap_bindpasswd) free(g_auth_context.ldap_bindpasswd);
    
    g_auth_context.ldap_server = strdup(server);
    g_auth_context.ldap_port = port ? strdup(port) : strdup("389");
    g_auth_context.ldap_basedn = strdup(basedn);
    g_auth_context.ldap_binddn = binddn ? strdup(binddn) : NULL;
    g_auth_context.ldap_bindpasswd = bindpasswd ? strdup(bindpasswd) : NULL;
    g_auth_context.method = RAMD_AUTH_METHOD_LDAP;
    
    /* Validate LDAP configuration */
    if (!ramd_auth_validate_ldap_config(&g_auth_context))
    {
        return false;
    }
    
    ramd_log_info("LDAP authentication configured: server=%s:%s, basedn=%s", 
                  server, g_auth_context.ldap_port, basedn);
    return true;
}

bool
ramd_postgresql_auth_configure_pam(const char* service_name)
{
    if (!service_name)
    {
        ramd_log_error("PAM service name is required");
        return false;
    }
    
    /* Set PAM parameters */
    if (g_auth_context.pam_service) free(g_auth_context.pam_service);
    g_auth_context.pam_service = strdup(service_name);
    g_auth_context.method = RAMD_AUTH_METHOD_PAM;
    
    /* Validate PAM configuration */
    if (!ramd_auth_validate_pam_config(&g_auth_context))
    {
        return false;
    }
    
    ramd_log_info("PAM authentication configured: service=%s", service_name);
    return true;
}

bool
ramd_postgresql_auth_set_method(const char* method_str)
{
    ramd_auth_method_t method = ramd_auth_parse_method(method_str);
    
    if (method == RAMD_AUTH_METHOD_UNKNOWN)
    {
        ramd_log_error("Unknown authentication method: %s", method_str);
        return false;
    }
    
    g_auth_context.method = method;
    ramd_log_info("Authentication method set to: %s", method_str);
    return true;
}

PGconn*
ramd_postgresql_auth_connect(void)
{
    char* conninfo;
    PGconn* conn;
    char* error_msg;
    
    /* Build connection string based on authentication method */
    conninfo = ramd_auth_build_conninfo(&g_auth_context);
    if (!conninfo)
    {
        ramd_log_error("Failed to build connection string");
        return NULL;
    }
    
    ramd_log_debug("Connecting to PostgreSQL with: %s", conninfo);
    
    /* Attempt connection */
    conn = PQconnectdb(conninfo);
    free(conninfo);
    
    if (!conn)
    {
        ramd_log_error("Failed to create PostgreSQL connection object");
        return NULL;
    }
    
    /* Check connection status */
    if (PQstatus(conn) != CONNECTION_OK)
    {
        error_msg = PQerrorMessage(conn);
        ramd_log_error("PostgreSQL connection failed: %s", error_msg);
        PQfinish(conn);
        return NULL;
    }
    
    /* Log successful connection with authentication method */
    ramd_log_info("PostgreSQL connection established using %s authentication", 
                  ramd_auth_get_method_name(g_auth_context.method));
    
    return conn;
}

const char*
ramd_auth_get_method_name(ramd_auth_method_t method)
{
    switch (method)
    {
        case RAMD_AUTH_METHOD_PASSWORD: return "password";
        case RAMD_AUTH_METHOD_MD5: return "md5";
        case RAMD_AUTH_METHOD_SCRAM_SHA_256: return "scram-sha-256";
        case RAMD_AUTH_METHOD_CERT: return "certificate";
        case RAMD_AUTH_METHOD_KERBEROS: return "kerberos";
        case RAMD_AUTH_METHOD_LDAP: return "ldap";
        case RAMD_AUTH_METHOD_PAM: return "pam";
        case RAMD_AUTH_METHOD_IDENT: return "ident";
        case RAMD_AUTH_METHOD_PEER: return "peer";
        case RAMD_AUTH_METHOD_TRUST: return "trust";
        case RAMD_AUTH_METHOD_REJECT: return "reject";
        default: return "unknown";
    }
}

void
ramd_postgresql_auth_cleanup(void)
{
    /* Free all allocated strings */
    if (g_auth_context.username) { free(g_auth_context.username); g_auth_context.username = NULL; }
    if (g_auth_context.password) { free(g_auth_context.password); g_auth_context.password = NULL; }
    if (g_auth_context.database) { free(g_auth_context.database); g_auth_context.database = NULL; }
    if (g_auth_context.hostname) { free(g_auth_context.hostname); g_auth_context.hostname = NULL; }
    if (g_auth_context.ssl_cert) { free(g_auth_context.ssl_cert); g_auth_context.ssl_cert = NULL; }
    if (g_auth_context.ssl_key) { free(g_auth_context.ssl_key); g_auth_context.ssl_key = NULL; }
    if (g_auth_context.ssl_ca) { free(g_auth_context.ssl_ca); g_auth_context.ssl_ca = NULL; }
    if (g_auth_context.ssl_mode) { free(g_auth_context.ssl_mode); g_auth_context.ssl_mode = NULL; }
    if (g_auth_context.kerberos_service) { free(g_auth_context.kerberos_service); g_auth_context.kerberos_service = NULL; }
    if (g_auth_context.ldap_server) { free(g_auth_context.ldap_server); g_auth_context.ldap_server = NULL; }
    if (g_auth_context.ldap_port) { free(g_auth_context.ldap_port); g_auth_context.ldap_port = NULL; }
    if (g_auth_context.ldap_basedn) { free(g_auth_context.ldap_basedn); g_auth_context.ldap_basedn = NULL; }
    if (g_auth_context.ldap_binddn) { free(g_auth_context.ldap_binddn); g_auth_context.ldap_binddn = NULL; }
    if (g_auth_context.ldap_bindpasswd) { free(g_auth_context.ldap_bindpasswd); g_auth_context.ldap_bindpasswd = NULL; }
    if (g_auth_context.pam_service) { free(g_auth_context.pam_service); g_auth_context.pam_service = NULL; }
    
    ramd_log_info("PostgreSQL authentication system cleaned up");
}

/* Private helper functions */
static ramd_auth_method_t
ramd_auth_parse_method(const char* method_str)
{
    if (!method_str) return RAMD_AUTH_METHOD_UNKNOWN;
    
    if (strcasecmp(method_str, "password") == 0) return RAMD_AUTH_METHOD_PASSWORD;
    if (strcasecmp(method_str, "md5") == 0) return RAMD_AUTH_METHOD_MD5;
    if (strcasecmp(method_str, "scram-sha-256") == 0) return RAMD_AUTH_METHOD_SCRAM_SHA_256;
    if (strcasecmp(method_str, "cert") == 0) return RAMD_AUTH_METHOD_CERT;
    if (strcasecmp(method_str, "kerberos") == 0) return RAMD_AUTH_METHOD_KERBEROS;
    if (strcasecmp(method_str, "ldap") == 0) return RAMD_AUTH_METHOD_LDAP;
    if (strcasecmp(method_str, "pam") == 0) return RAMD_AUTH_METHOD_PAM;
    if (strcasecmp(method_str, "ident") == 0) return RAMD_AUTH_METHOD_IDENT;
    if (strcasecmp(method_str, "peer") == 0) return RAMD_AUTH_METHOD_PEER;
    if (strcasecmp(method_str, "trust") == 0) return RAMD_AUTH_METHOD_TRUST;
    if (strcasecmp(method_str, "reject") == 0) return RAMD_AUTH_METHOD_REJECT;
    
    return RAMD_AUTH_METHOD_UNKNOWN;
}

static char*
ramd_auth_build_conninfo(const ramd_auth_context_t* auth_ctx)
{
    char conninfo[2048];
    size_t offset = 0;
    
    /* Start with basic connection parameters */
    offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                      "host=%s port=%d dbname=%s user=%s",
                      auth_ctx->hostname, auth_ctx->port, auth_ctx->database, auth_ctx->username);
    
    /* Add password for password-based authentication */
    if (auth_ctx->method == RAMD_AUTH_METHOD_PASSWORD || 
        auth_ctx->method == RAMD_AUTH_METHOD_MD5 ||
        auth_ctx->method == RAMD_AUTH_METHOD_SCRAM_SHA_256)
    {
        if (auth_ctx->password)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " password=%s", auth_ctx->password);
        }
    }
    
    /* Add SSL parameters */
    if (auth_ctx->require_ssl || auth_ctx->ssl_mode)
    {
        offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                          " sslmode=%s", auth_ctx->ssl_mode);
        
        if (auth_ctx->ssl_cert)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " sslcert=%s", auth_ctx->ssl_cert);
        }
        
        if (auth_ctx->ssl_key)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " sslkey=%s", auth_ctx->ssl_key);
        }
        
        if (auth_ctx->ssl_ca)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " sslrootcert=%s", auth_ctx->ssl_ca);
        }
    }
    
    /* Add Kerberos parameters */
    if (auth_ctx->method == RAMD_AUTH_METHOD_KERBEROS && auth_ctx->kerberos_service)
    {
        offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                          " krbsrvname=%s", auth_ctx->kerberos_service);
    }
    
    /* Add LDAP parameters */
    if (auth_ctx->method == RAMD_AUTH_METHOD_LDAP)
    {
        if (auth_ctx->ldap_server)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " ldapserver=%s", auth_ctx->ldap_server);
        }
        
        if (auth_ctx->ldap_port)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " ldapport=%s", auth_ctx->ldap_port);
        }
        
        if (auth_ctx->ldap_basedn)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " ldapbasedn=%s", auth_ctx->ldap_basedn);
        }
        
        if (auth_ctx->ldap_binddn)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " ldapbinddn=%s", auth_ctx->ldap_binddn);
        }
        
        if (auth_ctx->ldap_bindpasswd)
        {
            offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                              " ldapbindpasswd=%s", auth_ctx->ldap_bindpasswd);
        }
    }
    
    /* Add PAM parameters */
    if (auth_ctx->method == RAMD_AUTH_METHOD_PAM && auth_ctx->pam_service)
    {
        offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                          " pamservice=%s", auth_ctx->pam_service);
    }
    
    /* Add connection timeout */
    offset += (size_t)snprintf(conninfo + offset, sizeof(conninfo) - offset,
                      " connect_timeout=10");
    
    return strdup(conninfo);
}


static bool
ramd_auth_validate_kerberos_config(const ramd_auth_context_t* auth_ctx)
{
    /* Check if Kerberos libraries are available */
    if (!auth_ctx->kerberos_service)
    {
        ramd_log_error("Kerberos service name is required");
        return false;
    }
    
    /* Check for Kerberos configuration file */
    if (access("{{ETC_DIR}}krb5.conf", R_OK) != 0)
    {
        ramd_log_warning("Kerberos configuration file not found: {{ETC_DIR}}krb5.conf");
    }
    
    ramd_log_info("Kerberos configuration validation passed");
    return true;
}

static bool
ramd_auth_validate_ldap_config(const ramd_auth_context_t* auth_ctx)
{
    if (!auth_ctx->ldap_server || !auth_ctx->ldap_basedn)
    {
        ramd_log_error("LDAP server and base DN are required");
        return false;
    }
    
    /* Validate LDAP server format */
    if (strchr(auth_ctx->ldap_server, ' ') != NULL)
    {
        ramd_log_error("LDAP server name cannot contain spaces: %s", auth_ctx->ldap_server);
        return false;
    }
    
    /* Validate port number */
    if (auth_ctx->ldap_port)
    {
        int port = atoi(auth_ctx->ldap_port);
        if (port <= 0 || port > 65535)
        {
            ramd_log_error("Invalid LDAP port number: %s", auth_ctx->ldap_port);
            return false;
        }
    }
    
    ramd_log_info("LDAP configuration validation passed");
    return true;
}

static bool
ramd_auth_validate_pam_config(const ramd_auth_context_t* auth_ctx)
{
    if (!auth_ctx->pam_service)
    {
        ramd_log_error("PAM service name is required");
        return false;
    }
    
    /* Check for PAM configuration file */
    char pam_config_path[256];
    snprintf(pam_config_path, sizeof(pam_config_path), "{{ETC_DIR}}pam.d/%s", auth_ctx->pam_service);
    
    if (access(pam_config_path, R_OK) != 0)
    {
        ramd_log_warning("PAM configuration file not found: %s", pam_config_path);
    }
    
    ramd_log_info("PAM configuration validation passed");
    return true;
}
