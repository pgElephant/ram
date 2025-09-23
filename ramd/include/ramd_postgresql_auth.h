/*
 * ramd_postgresql_auth.h - PostgreSQL Authentication Support for ramd
 * 
 * This header defines comprehensive PostgreSQL authentication methods
 * including password, SSL, Kerberos, LDAP, and other authentication types.
 */

#ifndef RAMD_POSTGRESQL_AUTH_H
#define RAMD_POSTGRESQL_AUTH_H

#include "ramd.h"
#include <libpq-fe.h>
#include <stdbool.h>

/* Authentication method enumeration */
typedef enum
{
    RAMD_AUTH_METHOD_PASSWORD = 0,
    RAMD_AUTH_METHOD_MD5,
    RAMD_AUTH_METHOD_SCRAM_SHA_256,
    RAMD_AUTH_METHOD_CERT,
    RAMD_AUTH_METHOD_KERBEROS,
    RAMD_AUTH_METHOD_LDAP,
    RAMD_AUTH_METHOD_PAM,
    RAMD_AUTH_METHOD_IDENT,
    RAMD_AUTH_METHOD_PEER,
    RAMD_AUTH_METHOD_TRUST,
    RAMD_AUTH_METHOD_REJECT,
    RAMD_AUTH_METHOD_UNKNOWN
} ramd_auth_method_t;

/* Authentication context structure */
typedef struct ramd_auth_context_t
{
    ramd_auth_method_t method;
    char* username;
    char* password;
    char* database;
    char* hostname;
    int port;
    char* ssl_cert;
    char* ssl_key;
    char* ssl_ca;
    char* ssl_mode;
    char* kerberos_service;
    char* ldap_server;
    char* ldap_port;
    char* ldap_basedn;
    char* ldap_binddn;
    char* ldap_bindpasswd;
    char* pam_service;
    bool require_ssl;
    bool verify_ssl;
} ramd_auth_context_t;

/* Function declarations */

/* Core authentication functions */
bool ramd_postgresql_auth_init(const ramd_config_t* config);
PGconn* ramd_postgresql_auth_connect(void);
void ramd_postgresql_auth_cleanup(void);

/* Authentication method configuration */
bool ramd_postgresql_auth_configure_ssl(const char* ssl_cert, const char* ssl_key, 
                                        const char* ssl_ca, const char* ssl_mode);
bool ramd_postgresql_auth_configure_kerberos(const char* service_name);
bool ramd_postgresql_auth_configure_ldap(const char* server, const char* port,
                                         const char* basedn, const char* binddn,
                                         const char* bindpasswd);
bool ramd_postgresql_auth_configure_pam(const char* service_name);
bool ramd_postgresql_auth_set_method(const char* method_str);

/* Utility functions */
const char* ramd_auth_get_method_name(ramd_auth_method_t method);

/* Internal functions are declared in the .c file */

/* SSL/TLS support */
bool ramd_auth_configure_ssl_mode(const char* ssl_mode);
bool ramd_auth_configure_ssl_certificates(const char* cert_file, const char* key_file, const char* ca_file);
bool ramd_auth_verify_ssl_connection(PGconn* conn);

/* Kerberos support */
bool ramd_auth_configure_kerberos_service(const char* service_name);
bool ramd_auth_verify_kerberos_ticket(void);

/* LDAP support */
bool ramd_auth_configure_ldap_server(const char* server, int port, const char* basedn);
bool ramd_auth_configure_ldap_bind(const char* binddn, const char* bindpasswd);
bool ramd_auth_verify_ldap_connection(const char* server, int port);

/* PAM support */
bool ramd_auth_configure_pam_service(const char* service_name);
bool ramd_auth_verify_pam_configuration(const char* service_name);

/* Advanced authentication features */
bool ramd_auth_configure_connection_pooling(int min_connections, int max_connections);
bool ramd_auth_configure_connection_timeout(int timeout_seconds);
bool ramd_auth_configure_retry_policy(int max_retries, int retry_delay);

/* Security features */
bool ramd_auth_configure_password_encryption(const char* encryption_method);
bool ramd_auth_configure_audit_logging(bool enable);
bool ramd_auth_configure_failed_login_protection(int max_attempts, int lockout_duration);

/* Authentication status and monitoring */
bool ramd_auth_get_connection_status(PGconn* conn);
const char* ramd_auth_get_last_error(void);
int ramd_auth_get_failed_attempts(void);
time_t ramd_auth_get_last_successful_login(void);

/* Configuration management */
bool ramd_auth_save_configuration(const char* config_file);
bool ramd_auth_load_configuration(const char* config_file);
bool ramd_auth_validate_configuration(void);

#endif /* RAMD_POSTGRESQL_AUTH_H */
