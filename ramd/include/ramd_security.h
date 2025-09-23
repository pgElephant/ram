/*-------------------------------------------------------------------------
 *
 * ramd_security.h
 *		PostgreSQL Auto-Failover Daemon - Security Interface
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_SECURITY_H
#define RAMD_SECURITY_H

#include "ramd.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

/* Security constants */
#define RAMD_MAX_USERS 100
#define RAMD_MAX_TOKEN_LENGTH 64
#define RAMD_MAX_PASSWORD_LENGTH 256
#define RAMD_MAX_CERT_PATH_LENGTH 512
#define RAMD_MAX_KEY_PATH_LENGTH 512
#define RAMD_MAX_CA_PATH_LENGTH 512

/* User roles */
typedef enum
{
	RAMD_ROLE_NONE = 0,
	RAMD_ROLE_VIEWER = 1,
	RAMD_ROLE_OPERATOR = 2,
	RAMD_ROLE_ADMIN = 3
} ramd_user_role_t;

/* User structure */
typedef struct ramd_user_t
{
	char username[RAMD_MAX_USERNAME_LENGTH];
	char password_hash[SHA256_DIGEST_LENGTH * 2 + 1];
	char token[RAMD_MAX_TOKEN_LENGTH];
	ramd_user_role_t role;
	time_t created_at;
	time_t last_login;
	bool active;
} ramd_user_t;

/* Security context */
typedef struct ramd_security_context_t
{
	/* Authentication */
	bool enable_auth;
	char admin_token[RAMD_MAX_TOKEN_LENGTH];
	ramd_user_t users[RAMD_MAX_USERS];
	int user_count;

	/* SSL/TLS */
	bool enable_ssl;
	char cert_file[RAMD_MAX_CERT_PATH_LENGTH];
	char key_file[RAMD_MAX_KEY_PATH_LENGTH];
	char ca_file[RAMD_MAX_CA_PATH_LENGTH];
	SSL_CTX *ssl_ctx;

	/* Rate limiting */
	bool enable_rate_limiting;
	int max_requests_per_minute;
	int max_connections;

	/* Input validation */
	size_t max_request_size;
	bool enable_input_validation;

	/* Session management */
	int session_timeout;
	bool enable_session_management;

	/* Audit logging */
	bool enable_audit;
	char audit_log_file[RAMD_MAX_PATH_LENGTH];

	/* Thread safety */
	pthread_mutex_t mutex;
} ramd_security_context_t;

/* Security status */
typedef struct ramd_security_status_t
{
	bool auth_enabled;
	bool ssl_enabled;
	bool rate_limiting_enabled;
	bool audit_enabled;
	int user_count;
	int active_connections;
	int blocked_ips;
} ramd_security_status_t;

/* Audit entry */
typedef struct ramd_audit_entry_t
{
	time_t timestamp;
	char client_ip[INET_ADDRSTRLEN];
	char user[RAMD_MAX_USERNAME_LENGTH];
	char action[RAMD_MAX_COMMAND_LENGTH];
	char resource[RAMD_MAX_PATH_LENGTH];
	int result; /* 0 = success, 1 = failure */
	char details[RAMD_MAX_COMMAND_LENGTH];
} ramd_audit_entry_t;

/* Security API functions */

/* Initialize security subsystem */
bool ramd_security_init(ramd_security_context_t *ctx);

/* Cleanup security subsystem */
void ramd_security_cleanup(ramd_security_context_t *ctx);

/* Authenticate HTTP request */
bool ramd_security_authenticate_http(const char *client_ip, const char *authorization,
									const char *action, const char *resource);

/* Validate and sanitize input */
bool ramd_security_validate_and_sanitize_input(char *input, size_t max_length);

/* Add user */
bool ramd_security_add_user(const char *username, const char *password, ramd_user_role_t role);

/* Get audit log */
bool ramd_security_get_audit_log(ramd_audit_entry_t *entries, int max_entries, int *actual_count);

/* Get security status */
bool ramd_security_get_status(ramd_security_status_t *status);

/* Utility functions */
const char *ramd_security_role_to_string(ramd_user_role_t role);
ramd_user_role_t ramd_security_string_to_role(const char *role_str);
bool ramd_security_is_valid_username(const char *username);
bool ramd_security_is_valid_password(const char *password);

#endif /* RAMD_SECURITY_H */
