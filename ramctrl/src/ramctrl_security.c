/*-------------------------------------------------------------------------
 *
 * ramctrl_security.c
 *		RAM Control Tool - Security Implementation
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
#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "ramctrl_security.h"
#include "ramctrl.h"

/* Callback function for libcurl */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	char *ptr = (char *)userp;
	
	if (ptr)
	{
		strncat(ptr, (char*)contents, realsize);
	}
	
	return realsize;
}

/* Security context */
static ramctrl_security_context_t *g_security_ctx = NULL;

/* Forward declarations */
static bool ramctrl_security_init_ssl(void);
static void ramctrl_security_cleanup_ssl(void);
static bool ramctrl_security_load_certificates(void);
static bool ramctrl_security_validate_certificate(const char *cert_file);
static bool ramctrl_security_generate_token(char *token, size_t token_size);
static void ramctrl_security_cleanup_sensitive_data(char *data, size_t size);

/* Initialize security subsystem */
bool
ramctrl_security_init(ramctrl_security_context_t *ctx)
{
	if (!ctx)
		return false;

	memset(ctx, 0, sizeof(ramctrl_security_context_t));

	/* Initialize SSL/TLS */
	if (!ramctrl_security_init_ssl())
	{
		fprintf(stderr, "ramctrl: Failed to initialize SSL/TLS subsystem\n");
		return false;
	}

	/* Set default security policies */
	ctx->enable_ssl = false;
	ctx->verify_ssl = true;
	ctx->enable_token_auth = false;
	ctx->token[0] = '\0';
	ctx->enable_encryption = false;
	ctx->max_request_size = RAMCTRL_MAX_COMMAND_LENGTH;
	ctx->connection_timeout = 30;
	ctx->enable_audit = true;

	/* Initialize mutex */
	if (pthread_mutex_init(&ctx->mutex, NULL) != 0)
	{
		fprintf(stderr, "ramctrl: Failed to initialize security mutex\n");
		return false;
	}

	g_security_ctx = ctx;
	return true;
}

/* Cleanup security subsystem */
void
ramctrl_security_cleanup(ramctrl_security_context_t *ctx)
{
	if (!ctx)
		return;

	pthread_mutex_lock(&ctx->mutex);

	/* Cleanup SSL/TLS */
	ramctrl_security_cleanup_ssl();

	/* Cleanup sensitive data */
	ramctrl_security_cleanup_sensitive_data(ctx->token, sizeof(ctx->token));
	ramctrl_security_cleanup_sensitive_data(ctx->cert_file, sizeof(ctx->cert_file));
	ramctrl_security_cleanup_sensitive_data(ctx->key_file, sizeof(ctx->key_file));
	ramctrl_security_cleanup_sensitive_data(ctx->ca_file, sizeof(ctx->ca_file));

	/* Cleanup mutex */
	pthread_mutex_unlock(&ctx->mutex);
	pthread_mutex_destroy(&ctx->mutex);

	/* Clear sensitive data */
	memset(ctx, 0, sizeof(ramctrl_security_context_t));
	g_security_ctx = NULL;
}

/* Initialize SSL/TLS */
static bool
ramctrl_security_init_ssl(void)
{
	/* Initialize OpenSSL */
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	return true;
}

/* Cleanup SSL/TLS */
static void
ramctrl_security_cleanup_ssl(void)
{
	EVP_cleanup();
	ERR_free_strings();
}

/* Load certificates */
static bool
ramctrl_security_load_certificates(void)
{
	if (!g_security_ctx)
		return false;

	/* Check if certificate files exist */
	if (strlen(g_security_ctx->cert_file) > 0)
	{
		if (!ramctrl_security_validate_certificate(g_security_ctx->cert_file))
		{
			fprintf(stderr, "ramctrl: Invalid certificate file: %s\n", g_security_ctx->cert_file);
			return false;
		}
	}

	return true;
}

/* Validate certificate */
static bool
ramctrl_security_validate_certificate(const char *cert_file)
{
	if (!cert_file)
		return false;

	FILE *fp = fopen(cert_file, "r");
	if (!fp)
		return false;

	X509 *cert = PEM_read_X509(fp, NULL, NULL, NULL);
	fclose(fp);

	if (!cert)
		return false;

	X509_free(cert);
	return true;
}

/* Generate secure token */
static bool
ramctrl_security_generate_token(char *token, size_t token_size)
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
		sprintf(hex_string + (i * 2), "%02x", random_bytes[i]);
	}
	hex_string[64] = '\0';

	/* Copy to token buffer */
	strncpy(token, hex_string, token_size - 1);
	token[token_size - 1] = '\0';

	return true;
}


/* Cleanup sensitive data */
static void
ramctrl_security_cleanup_sensitive_data(char *data, size_t size)
{
	if (data && size > 0)
	{
		memset(data, 0, size);
	}
}

/* Public API functions */

/* Configure security */
bool
ramctrl_security_configure(const char *config_file)
{
	if (!g_security_ctx)
		return false;

	/* Load configuration from file */
	FILE *fp = fopen(config_file, "r");
	if (!fp)
	{
		fprintf(stderr, "ramctrl: Failed to open security config file: %s\n", config_file);
		return false;
	}

	char line[1024];
	while (fgets(line, sizeof(line), fp))
	{
		/* Remove newline */
		line[strcspn(line, "\n")] = '\0';

		/* Skip comments and empty lines */
		if (line[0] == '#' || line[0] == '\0')
			continue;

		/* Parse configuration */
		char *key = strtok(line, "=");
		char *value = strtok(NULL, "=");

		if (!key || !value)
			continue;

		/* Trim whitespace */
		while (*key == ' ' || *key == '\t') key++;
		while (*value == ' ' || *value == '\t') value++;

		if (strcmp(key, "enable_ssl") == 0)
		{
			g_security_ctx->enable_ssl = (strcmp(value, "true") == 0);
		}
		else if (strcmp(key, "verify_ssl") == 0)
		{
			g_security_ctx->verify_ssl = (strcmp(value, "true") == 0);
		}
		else if (strcmp(key, "enable_token_auth") == 0)
		{
			g_security_ctx->enable_token_auth = (strcmp(value, "true") == 0);
		}
		else if (strcmp(key, "token") == 0)
		{
			strncpy(g_security_ctx->token, value, sizeof(g_security_ctx->token) - 1);
			g_security_ctx->token[sizeof(g_security_ctx->token) - 1] = '\0';
		}
		else if (strcmp(key, "cert_file") == 0)
		{
			strncpy(g_security_ctx->cert_file, value, sizeof(g_security_ctx->cert_file) - 1);
			g_security_ctx->cert_file[sizeof(g_security_ctx->cert_file) - 1] = '\0';
		}
		else if (strcmp(key, "key_file") == 0)
		{
			strncpy(g_security_ctx->key_file, value, sizeof(g_security_ctx->key_file) - 1);
			g_security_ctx->key_file[sizeof(g_security_ctx->key_file) - 1] = '\0';
		}
		else if (strcmp(key, "ca_file") == 0)
		{
			strncpy(g_security_ctx->ca_file, value, sizeof(g_security_ctx->ca_file) - 1);
			g_security_ctx->ca_file[sizeof(g_security_ctx->ca_file) - 1] = '\0';
		}
		else if (strcmp(key, "enable_encryption") == 0)
		{
			g_security_ctx->enable_encryption = (strcmp(value, "true") == 0);
		}
		else if (strcmp(key, "max_request_size") == 0)
		{
			g_security_ctx->max_request_size = (size_t)atoi(value);
		}
		else if (strcmp(key, "connection_timeout") == 0)
		{
			g_security_ctx->connection_timeout = atoi(value);
		}
		else if (strcmp(key, "enable_audit") == 0)
		{
			g_security_ctx->enable_audit = (strcmp(value, "true") == 0);
		}
	}

	fclose(fp);

	/* Load certificates if SSL is enabled */
	if (g_security_ctx->enable_ssl)
	{
		if (!ramctrl_security_load_certificates())
		{
			fprintf(stderr, "ramctrl: Failed to load SSL certificates\n");
			return false;
		}
	}

	/* Generate token if not provided */
	if (g_security_ctx->enable_token_auth && strlen(g_security_ctx->token) == 0)
	{
		if (!ramctrl_security_generate_token(g_security_ctx->token, sizeof(g_security_ctx->token)))
		{
			fprintf(stderr, "ramctrl: Failed to generate security token\n");
			return false;
		}
	}

	return true;
}

/* Secure HTTP request */
int
ramctrl_security_http_request(const char *url, const char *data, char *response, size_t response_size)
{
	if (!g_security_ctx || !url || !response || response_size == 0)
		return -1;

	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl)
		return -1;

	/* Set basic options */
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_security_ctx->connection_timeout);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "ramctrl/1.0");

	/* SSL/TLS configuration */
	if (g_security_ctx->enable_ssl)
	{
		curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
		
		if (!g_security_ctx->verify_ssl)
		{
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		}

		if (strlen(g_security_ctx->cert_file) > 0)
		{
			curl_easy_setopt(curl, CURLOPT_SSLCERT, g_security_ctx->cert_file);
		}

		if (strlen(g_security_ctx->key_file) > 0)
		{
			curl_easy_setopt(curl, CURLOPT_SSLKEY, g_security_ctx->key_file);
		}

		if (strlen(g_security_ctx->ca_file) > 0)
		{
			curl_easy_setopt(curl, CURLOPT_CAINFO, g_security_ctx->ca_file);
		}
	}

	/* Authentication */
	if (g_security_ctx->enable_token_auth && strlen(g_security_ctx->token) > 0)
	{
		struct curl_slist *headers = NULL;
		char auth_header[256];
		snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_security_ctx->token);
		headers = curl_slist_append(headers, auth_header);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	/* POST data if provided */
	if (data && strlen(data) > 0)
	{
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
	}

	/* Perform request */
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	return (res == CURLE_OK) ? 0 : -1;
}

/* Validate input */
bool
ramctrl_security_validate_input(const char *input, size_t max_length)
{
	if (!input || !g_security_ctx)
		return false;

	size_t len = strlen(input);
	if (len > max_length || len > g_security_ctx->max_request_size)
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
bool
ramctrl_security_sanitize_input(char *input, size_t max_length)
{
	if (!input || !g_security_ctx)
		return false;

	size_t len = strlen(input);
	if (len > max_length || len > g_security_ctx->max_request_size)
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

/* Log security event */
void
ramctrl_security_log_event(const char *event, const char *details)
{
	if (!g_security_ctx || !g_security_ctx->enable_audit)
		return;

	time_t now = time(NULL);
	char timestamp[64];
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

	fprintf(stderr, "ramctrl: [%s] SECURITY: %s - %s\n", timestamp, event, details ? details : "");
}

/* Get security status */
bool
ramctrl_security_get_status(ramctrl_security_status_t *status)
{
	if (!status || !g_security_ctx)
		return false;

	status->ssl_enabled = g_security_ctx->enable_ssl;
	status->ssl_verify = g_security_ctx->verify_ssl;
	status->token_auth_enabled = g_security_ctx->enable_token_auth;
	status->encryption_enabled = g_security_ctx->enable_encryption;
	status->audit_enabled = g_security_ctx->enable_audit;
	status->max_request_size = g_security_ctx->max_request_size;
	status->connection_timeout = g_security_ctx->connection_timeout;

	return true;
}
