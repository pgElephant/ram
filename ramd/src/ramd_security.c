/*-------------------------------------------------------------------------
 *
 * ramd_security.c
 *		PostgreSQL Auto-Failover Daemon - Security Implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "ramd_security.h"
#include "ramd_logging.h"
#include "ramd_config.h"

/* Security context */
static ramd_security_context_t *g_security_ctx = NULL;
static pthread_mutex_t g_security_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Rate limiting structures */
typedef struct ramd_rate_limit_entry
{
	char client_ip[INET_ADDRSTRLEN];
	time_t first_request;
	int request_count;
	time_t last_request;
	bool blocked;
} ramd_rate_limit_entry_t;

#define RAMD_MAX_RATE_LIMIT_ENTRIES 1000
#define RAMD_RATE_LIMIT_WINDOW_SECONDS 60
#define RAMD_MAX_REQUESTS_PER_WINDOW 100
#define RAMD_BLOCK_DURATION_SECONDS 300

static ramd_rate_limit_entry_t g_rate_limit_entries[RAMD_MAX_RATE_LIMIT_ENTRIES];
static int g_rate_limit_count = 0;

/* Security audit log - using the same structure as in header */

#define RAMD_MAX_AUDIT_ENTRIES 10000
static ramd_audit_entry_t g_audit_log[RAMD_MAX_AUDIT_ENTRIES];
static int g_audit_count = 0;
static int g_audit_index = 0;

/* Forward declarations */
static bool ramd_security_init_ssl(void);
static void ramd_security_cleanup_ssl(void);
static bool ramd_security_generate_token(char *token, size_t token_size);
static bool ramd_security_validate_token(const char *token);
static bool ramd_security_check_rate_limit(const char *client_ip);
static void ramd_security_log_audit(const char *client_ip, const char *user,
									const char *action, const char *resource,
									int result, const char *details);
static bool ramd_security_validate_input(const char *input, size_t max_length);
static bool ramd_security_sanitize_input(char *input, size_t max_length);
static bool ramd_security_check_permissions(const char *user, const char *action);
static void ramd_security_cleanup_rate_limits(void);

/* Initialize security subsystem */
bool
ramd_security_init(ramd_security_context_t *ctx)
{
	if (!ctx)
		return false;

	memset(ctx, 0, sizeof(ramd_security_context_t));

	/* Initialize SSL/TLS */
	if (!ramd_security_init_ssl())
	{
		ramd_log_error("Failed to initialize SSL/TLS subsystem");
		return false;
	}

	/* Initialize rate limiting */
	memset(g_rate_limit_entries, 0, sizeof(g_rate_limit_entries));
	g_rate_limit_count = 0;

	/* Initialize audit log */
	memset(g_audit_log, 0, sizeof(g_audit_log));
	g_audit_count = 0;
	g_audit_index = 0;

	/* Generate default admin token if not provided */
	if (strlen(ctx->admin_token) == 0)
	{
		if (!ramd_security_generate_token(ctx->admin_token, sizeof(ctx->admin_token)))
		{
			ramd_log_error("Failed to generate admin token");
			return false;
		}
		ramd_log_info("Generated admin token: %s", ctx->admin_token);
	}

	/* Initialize user roles */
	ctx->users[0].username[0] = '\0';
	ctx->users[0].password_hash[0] = '\0';
	ctx->users[0].role = RAMD_ROLE_NONE;
	ctx->user_count = 0;

	/* Set default security policies */
	ctx->max_request_size = RAMD_MAX_COMMAND_LENGTH;
	ctx->max_connections = 100;
	ctx->session_timeout = 3600; /* 1 hour */
	ctx->enable_audit = true;
	ctx->enable_rate_limiting = true;
	ctx->enable_ssl = false; /* Will be enabled if certificates are provided */

	/* Initialize mutexes */
	if (pthread_mutex_init(&ctx->mutex, NULL) != 0)
	{
		ramd_log_error("Failed to initialize security mutex");
		return false;
	}

	if (pthread_mutex_init(&g_security_mutex, NULL) != 0)
	{
		ramd_log_error("Failed to initialize global security mutex");
		return false;
	}

	g_security_ctx = ctx;
	ramd_log_info("Security subsystem initialized successfully");
	return true;
}

/* Cleanup security subsystem */
void
ramd_security_cleanup(ramd_security_context_t *ctx)
{
	if (!ctx)
		return;

	pthread_mutex_lock(&g_security_mutex);

	/* Cleanup SSL/TLS */
	ramd_security_cleanup_ssl();

	/* Cleanup rate limits */
	ramd_security_cleanup_rate_limits();

	/* Cleanup audit log */
	memset(g_audit_log, 0, sizeof(g_audit_log));
	g_audit_count = 0;
	g_audit_index = 0;

	/* Cleanup mutexes */
	pthread_mutex_unlock(&g_security_mutex);
	pthread_mutex_destroy(&g_security_mutex);
	pthread_mutex_destroy(&ctx->mutex);

	/* Clear sensitive data */
	memset(ctx, 0, sizeof(ramd_security_context_t));
	g_security_ctx = NULL;

	ramd_log_info("Security subsystem cleaned up");
}

/* Initialize SSL/TLS */
static bool
ramd_security_init_ssl(void)
{
	/* Initialize OpenSSL */
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	return true;
}

/* Cleanup SSL/TLS */
static void
ramd_security_cleanup_ssl(void)
{
	EVP_cleanup();
	ERR_free_strings();
}

/* Generate secure token */
static bool
ramd_security_generate_token(char *token, size_t token_size)
{
	unsigned char random_bytes[32];
	char hex_string[65];

	if (!token || token_size < 65)
		return false;

	/* Generate random bytes */
	if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1)
		return false;

	/* Convert to hex string */
	for (int i = 0; i < 32; i++)
	{
		snprintf(hex_string + (i * 2), 3, "%02x", random_bytes[i]);
	}
	hex_string[64] = '\0';

	/* Copy to token buffer */
	strncpy(token, hex_string, token_size - 1);
	token[token_size - 1] = '\0';

	return true;
}

/* Validate token */
static bool
ramd_security_validate_token(const char *token)
{
	if (!token || !g_security_ctx)
		return false;

	/* Check admin token */
	if (strcmp(token, g_security_ctx->admin_token) == 0)
		return true;

	/* Check user tokens */
	for (int i = 0; i < g_security_ctx->user_count; i++)
	{
		if (strcmp(token, g_security_ctx->users[i].token) == 0)
			return true;
	}

	return false;
}

/* Check rate limiting */
static bool
ramd_security_check_rate_limit(const char *client_ip)
{
	time_t now = time(NULL);
	int entry_index = -1;

	if (!g_security_ctx || !g_security_ctx->enable_rate_limiting)
		return true;

	pthread_mutex_lock(&g_security_mutex);

	/* Find existing entry */
	for (int i = 0; i < g_rate_limit_count; i++)
	{
		if (strcmp(g_rate_limit_entries[i].client_ip, client_ip) == 0)
		{
			entry_index = i;
			break;
		}
	}

	/* Create new entry if not found */
	if (entry_index == -1)
	{
		if (g_rate_limit_count >= RAMD_MAX_RATE_LIMIT_ENTRIES)
		{
			/* Remove oldest entry */
			memmove(&g_rate_limit_entries[0], &g_rate_limit_entries[1],
					sizeof(ramd_rate_limit_entry_t) * (RAMD_MAX_RATE_LIMIT_ENTRIES - 1));
			g_rate_limit_count--;
			entry_index = g_rate_limit_count; /* This is now the last valid index */
		}
		else
		{
			entry_index = g_rate_limit_count++;
		}
		
		/* Bounds check to prevent segfault */
		if (entry_index < 0 || entry_index >= RAMD_MAX_RATE_LIMIT_ENTRIES)
		{
			pthread_mutex_unlock(&g_security_mutex);
			ramd_log_error("Rate limit entry index out of bounds: %d", entry_index);
			return false;
		}

		strncpy(g_rate_limit_entries[entry_index].client_ip, client_ip,
				sizeof(g_rate_limit_entries[entry_index].client_ip) - 1);
		g_rate_limit_entries[entry_index].client_ip[INET_ADDRSTRLEN - 1] = '\0';
		g_rate_limit_entries[entry_index].first_request = now;
		g_rate_limit_entries[entry_index].request_count = 0;
		g_rate_limit_entries[entry_index].last_request = now;
		g_rate_limit_entries[entry_index].blocked = false;
	}

	ramd_rate_limit_entry_t *entry = &g_rate_limit_entries[entry_index];

	/* Check if currently blocked */
	if (entry->blocked)
	{
		if (now - entry->last_request > RAMD_BLOCK_DURATION_SECONDS)
		{
			/* Unblock */
			entry->blocked = false;
			entry->first_request = now;
			entry->request_count = 0;
		}
		else
		{
			pthread_mutex_unlock(&g_security_mutex);
			return false;
		}
	}

	/* Reset window if expired */
	if (now - entry->first_request > RAMD_RATE_LIMIT_WINDOW_SECONDS)
	{
		entry->first_request = now;
		entry->request_count = 0;
	}

	/* Check rate limit */
	entry->request_count++;
	entry->last_request = now;

	if (entry->request_count > RAMD_MAX_REQUESTS_PER_WINDOW)
	{
		entry->blocked = true;
		ramd_security_log_audit(client_ip, "system", "rate_limit_exceeded", "api", 1, "Rate limit exceeded");
		pthread_mutex_unlock(&g_security_mutex);
		return false;
	}

	pthread_mutex_unlock(&g_security_mutex);
	return true;
}

/* Log security audit event */
static void
ramd_security_log_audit(const char *client_ip, const char *user,
						const char *action, const char *resource,
						int result, const char *details)
{
	if (!g_security_ctx || !g_security_ctx->enable_audit)
		return;

	pthread_mutex_lock(&g_security_mutex);

	ramd_audit_entry_t *entry = &g_audit_log[g_audit_index];
	entry->timestamp = time(NULL);

	if (client_ip)
		strncpy(entry->client_ip, client_ip, sizeof(entry->client_ip) - 1);
	else
		entry->client_ip[0] = '\0';
	entry->client_ip[INET_ADDRSTRLEN - 1] = '\0';

	if (user)
		strncpy(entry->user, user, sizeof(entry->user) - 1);
	else
		entry->user[0] = '\0';
	entry->user[RAMD_MAX_USERNAME_LENGTH - 1] = '\0';

	if (action)
		strncpy(entry->action, action, sizeof(entry->action) - 1);
	else
		entry->action[0] = '\0';
	entry->action[RAMD_MAX_COMMAND_LENGTH - 1] = '\0';

	if (resource)
		strncpy(entry->resource, resource, sizeof(entry->resource) - 1);
	else
		entry->resource[0] = '\0';
	entry->resource[RAMD_MAX_PATH_LENGTH - 1] = '\0';

	entry->result = result;

	if (details)
		strncpy(entry->details, details, sizeof(entry->details) - 1);
	else
		entry->details[0] = '\0';
	entry->details[RAMD_MAX_COMMAND_LENGTH - 1] = '\0';

	g_audit_index = (g_audit_index + 1) % RAMD_MAX_AUDIT_ENTRIES;
	if (g_audit_count < RAMD_MAX_AUDIT_ENTRIES)
		g_audit_count++;

	pthread_mutex_unlock(&g_security_mutex);

	/* Log to ramd log system */
	if (result == 0)
	{
		ramd_log_info("AUDIT: %s@%s %s %s - SUCCESS: %s", user ? user : "unknown",
					  client_ip ? client_ip : "unknown", action ? action : "unknown",
					  resource ? resource : "unknown", details ? details : "");
	}
	else
	{
		ramd_log_warning("AUDIT: %s@%s %s %s - FAILED: %s", user ? user : "unknown",
						 client_ip ? client_ip : "unknown", action ? action : "unknown",
						 resource ? resource : "unknown", details ? details : "");
	}
}

/* Validate input */
static bool
ramd_security_validate_input(const char *input, size_t max_length)
{
	if (!input)
		return false;

	size_t len = strlen(input);
	if (len > max_length)
		return false;

	/* Check for null bytes */
	if (memchr(input, '\0', len) != input + len)
		return false;

	/* Check for control characters */
	for (size_t i = 0; i < len; i++)
	{
		if (input[i] < 32 && input[i] != '\t' && input[i] != '\n' && input[i] != '\r')
			return false;
	}

	return true;
}

/* Sanitize input */
static bool
ramd_security_sanitize_input(char *input, size_t max_length)
{
	if (!input)
		return false;

	size_t len = strlen(input);
	if (len > max_length)
		return false;

	/* Remove control characters except tab, newline, carriage return */
	for (size_t i = 0; i < len; i++)
	{
		if (input[i] < 32 && input[i] != '\t' && input[i] != '\n' && input[i] != '\r')
		{
			memmove(input + i, input + i + 1, len - i);
			len--;
			i--;
		}
	}

	input[len] = '\0';
	return true;
}

/* Check permissions */
static bool
ramd_security_check_permissions(const char *user, const char *action)
{
	if (!user || !action || !g_security_ctx)
		return false;

	/* Find user */
	ramd_user_t *user_entry = NULL;
	for (int i = 0; i < g_security_ctx->user_count; i++)
	{
		if (strcmp(g_security_ctx->users[i].username, user) == 0)
		{
			user_entry = &g_security_ctx->users[i];
			break;
		}
	}

	if (!user_entry)
		return false;

	/* Check permissions based on role */
	switch (user_entry->role)
	{
		case RAMD_ROLE_ADMIN:
			return true; /* Admin can do everything */
		case RAMD_ROLE_OPERATOR:
			/* Operators can do most things except user management */
			return strcmp(action, "user_manage") != 0;
		case RAMD_ROLE_VIEWER:
			/* Viewers can only read */
			return strcmp(action, "read") == 0;
		default:
			return false;
	}
}

/* Cleanup rate limits */
static void
ramd_security_cleanup_rate_limits(void)
{
	time_t now = time(NULL);

	for (int i = 0; i < g_rate_limit_count; i++)
	{
		ramd_rate_limit_entry_t *entry = &g_rate_limit_entries[i];
		if (now - entry->last_request > RAMD_BLOCK_DURATION_SECONDS)
		{
			/* Remove expired entries */
			memmove(&g_rate_limit_entries[i], &g_rate_limit_entries[i + 1],
					sizeof(ramd_rate_limit_entry_t) * (size_t)(g_rate_limit_count - i - 1));
			g_rate_limit_count--;
			i--;
		}
	}
}

/* Public API functions */

/* Authenticate HTTP request */
bool
ramd_security_authenticate_http(const char *client_ip, const char *authorization,
								const char *action, const char *resource)
{
	if (!g_security_ctx)
		return false;

	/* Check rate limiting */
	if (!ramd_security_check_rate_limit(client_ip))
	{
		ramd_security_log_audit(client_ip, "anonymous", action, resource, 1, "Rate limit exceeded");
		return false;
	}

	/* Check if authentication is required */
	if (!g_security_ctx->enable_auth)
	{
		ramd_security_log_audit(client_ip, "anonymous", action, resource, 0, "No authentication required");
		return true;
	}

	/* Validate token */
	if (!ramd_security_validate_token(authorization))
	{
		ramd_security_log_audit(client_ip, "anonymous", action, resource, 1, "Invalid token");
		return false;
	}

	/* Check permissions */
	if (!ramd_security_check_permissions("admin", action))
	{
		ramd_security_log_audit(client_ip, "admin", action, resource, 1, "Insufficient permissions");
		return false;
	}

	ramd_security_log_audit(client_ip, "admin", action, resource, 0, "Authentication successful");
	return true;
}

/* Validate and sanitize input */
bool
ramd_security_validate_and_sanitize_input(char *input, size_t max_length)
{
	if (!input)
		return false;

	/* Validate input */
	if (!ramd_security_validate_input(input, max_length))
		return false;

	/* Sanitize input */
	return ramd_security_sanitize_input(input, max_length);
}

/* Add user */
bool
ramd_security_add_user(const char *username, const char *password, ramd_user_role_t role)
{
	if (!g_security_ctx || !username || !password)
		return false;

	if (g_security_ctx->user_count >= RAMD_MAX_USERS)
		return false;

	/* Check if user already exists */
	for (int i = 0; i < g_security_ctx->user_count; i++)
	{
		if (strcmp(g_security_ctx->users[i].username, username) == 0)
			return false;
	}

	/* Add user */
	ramd_user_t *user = &g_security_ctx->users[g_security_ctx->user_count];
	strncpy(user->username, username, sizeof(user->username) - 1);
	user->username[sizeof(user->username) - 1] = '\0';

	/* Hash password */
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256((unsigned char*)password, strlen(password), hash);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		snprintf(user->password_hash + (i * 2), 3, "%02x", hash[i]);
	}
	user->password_hash[SHA256_DIGEST_LENGTH * 2] = '\0';

	user->role = role;

	/* Generate token */
	if (!ramd_security_generate_token(user->token, sizeof(user->token)))
		return false;

	g_security_ctx->user_count++;

	ramd_log_info("Added user: %s with role: %d", username, role);
	return true;
}

/* Get audit log */
bool
ramd_security_get_audit_log(ramd_audit_entry_t *entries, int max_entries, int *actual_count)
{
	if (!entries || !actual_count || !g_security_ctx)
		return false;

	pthread_mutex_lock(&g_security_mutex);

	int count = (max_entries < g_audit_count) ? max_entries : g_audit_count;
	*actual_count = count;

	/* Copy entries in reverse chronological order */
	for (int i = 0; i < count; i++)
	{
		int index = (g_audit_index - 1 - i + RAMD_MAX_AUDIT_ENTRIES) % RAMD_MAX_AUDIT_ENTRIES;
		entries[i] = g_audit_log[index];
	}

	pthread_mutex_unlock(&g_security_mutex);
	return true;
}

/* Get security status */
bool
ramd_security_get_status(ramd_security_status_t *status)
{
	if (!status || !g_security_ctx)
		return false;

	status->auth_enabled = g_security_ctx->enable_auth;
	status->ssl_enabled = g_security_ctx->enable_ssl;
	status->rate_limiting_enabled = g_security_ctx->enable_rate_limiting;
	status->audit_enabled = g_security_ctx->enable_audit;
	status->user_count = g_security_ctx->user_count;
	/* Track active connections */
	status->active_connections = g_security_ctx->max_connections;
	
	/* Track blocked IPs */
	status->blocked_ips = 0; /* No blocked IP tracking implemented yet */

	return true;
}
