/*-------------------------------------------------------------------------
 *
 * ramctrl_security.h
 *		RAM Control Tool - Security Interface
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMCTRL_SECURITY_H
#define RAMCTRL_SECURITY_H

#include "ramctrl.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <curl/curl.h>

/* Security constants */
#define RAMCTRL_MAX_TOKEN_LENGTH 64
#define RAMCTRL_MAX_CERT_PATH_LENGTH 512
#define RAMCTRL_MAX_KEY_PATH_LENGTH 512
#define RAMCTRL_MAX_CA_PATH_LENGTH 512

/* Security context */
typedef struct ramctrl_security_context_t
{
	/* SSL/TLS */
	bool enable_ssl;
	bool verify_ssl;
	char cert_file[RAMCTRL_MAX_CERT_PATH_LENGTH];
	char key_file[RAMCTRL_MAX_KEY_PATH_LENGTH];
	char ca_file[RAMCTRL_MAX_CA_PATH_LENGTH];

	/* Authentication */
	bool enable_token_auth;
	char token[RAMCTRL_MAX_TOKEN_LENGTH];

	/* Encryption */
	bool enable_encryption;

	/* Input validation */
	size_t max_request_size;

	/* Connection settings */
	int connection_timeout;

	/* Audit logging */
	bool enable_audit;

	/* Thread safety */
	pthread_mutex_t mutex;
} ramctrl_security_context_t;

/* Security status */
typedef struct ramctrl_security_status_t
{
	bool ssl_enabled;
	bool ssl_verify;
	bool token_auth_enabled;
	bool encryption_enabled;
	bool audit_enabled;
	size_t max_request_size;
	int connection_timeout;
} ramctrl_security_status_t;

/* Security API functions */

/* Initialize security subsystem */
bool ramctrl_security_init(ramctrl_security_context_t *ctx);

/* Cleanup security subsystem */
void ramctrl_security_cleanup(ramctrl_security_context_t *ctx);

/* Configure security from file */
bool ramctrl_security_configure(const char *config_file);

/* Secure HTTP request */
int ramctrl_security_http_request(const char *url, const char *data, char *response, size_t response_size);

/* Validate input */
bool ramctrl_security_validate_input(const char *input, size_t max_length);

/* Sanitize input */
bool ramctrl_security_sanitize_input(char *input, size_t max_length);

/* Log security event */
void ramctrl_security_log_event(const char *event, const char *details);

/* Get security status */
bool ramctrl_security_get_status(ramctrl_security_status_t *status);

/* Utility functions */
bool ramctrl_security_is_valid_url(const char *url);
bool ramctrl_security_is_valid_token(const char *token);
bool ramctrl_security_is_valid_certificate(const char *cert_file);

#endif /* RAMCTRL_SECURITY_H */
