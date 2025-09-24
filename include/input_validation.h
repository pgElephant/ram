/*-------------------------------------------------------------------------
 *
 * input_validation.h
 *		Comprehensive input validation for RAM High Availability System
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef INPUT_VALIDATION_H
#define INPUT_VALIDATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

/* Validation result */
typedef enum
{
    VALIDATION_SUCCESS = 0,
    VALIDATION_ERROR_NULL = -1,
    VALIDATION_ERROR_EMPTY = -2,
    VALIDATION_ERROR_TOO_LONG = -3,
    VALIDATION_ERROR_INVALID_CHAR = -4,
    VALIDATION_ERROR_INVALID_FORMAT = -5,
    VALIDATION_ERROR_OUT_OF_RANGE = -6
} validation_result_t;

/* String validation */
validation_result_t validate_string(const char *str, size_t max_length);
validation_result_t validate_non_empty_string(const char *str, size_t max_length);
validation_result_t validate_alphanumeric_string(const char *str, size_t max_length);
validation_result_t validate_numeric_string(const char *str, size_t max_length);
validation_result_t validate_email(const char *email);
validation_result_t validate_ip_address(const char *ip);
validation_result_t validate_port_number(const char *port);
validation_result_t validate_database_name(const char *dbname);
validation_result_t validate_username(const char *username);
validation_result_t validate_password(const char *password);

/* Numeric validation */
validation_result_t validate_int_range(int value, int min, int max);
validation_result_t validate_uint_range(unsigned int value, unsigned int min, unsigned int max);
validation_result_t validate_long_range(long value, long min, long max);
validation_result_t validate_ulong_range(unsigned long value, unsigned long min, unsigned long max);
validation_result_t validate_float_range(float value, float min, float max);
validation_result_t validate_double_range(double value, double min, double max);

/* Network validation */
validation_result_t validate_hostname(const char *hostname);
validation_result_t validate_port(int port);
validation_result_t validate_url(const char *url);
validation_result_t validate_connection_string(const char *connstr);

/* File validation */
validation_result_t validate_file_path(const char *path);
validation_result_t validate_directory_path(const char *path);
validation_result_t validate_file_exists(const char *path);
validation_result_t validate_directory_exists(const char *path);
validation_result_t validate_file_readable(const char *path);
validation_result_t validate_file_writable(const char *path);

/* Configuration validation */
validation_result_t validate_config_file(const char *config_file);
validation_result_t validate_log_level(const char *log_level);
validation_result_t validate_ssl_mode(const char *ssl_mode);
validation_result_t validate_auth_method(const char *auth_method);

/* Utility functions */
const char *validation_result_to_string(validation_result_t result);
bool is_valid_identifier(const char *str);
bool is_valid_filename(const char *filename);
bool is_valid_path(const char *path);
bool is_safe_string(const char *str);

/* Validation macros */
#define VALIDATE_STRING(str, max_len) \
    do { \
        validation_result_t result = validate_string(str, max_len); \
        if (result != VALIDATION_SUCCESS) { \
            log_error("String validation failed: %s", validation_result_to_string(result)); \
            return -1; \
        } \
    } while(0)

#define VALIDATE_INT_RANGE(value, min, max) \
    do { \
        validation_result_t result = validate_int_range(value, min, max); \
        if (result != VALIDATION_SUCCESS) { \
            log_error("Integer range validation failed: %s", validation_result_to_string(result)); \
            return -1; \
        } \
    } while(0)

#define VALIDATE_PORT(port) \
    do { \
        validation_result_t result = validate_port(port); \
        if (result != VALIDATION_SUCCESS) { \
            log_error("Port validation failed: %s", validation_result_to_string(result)); \
            return -1; \
        } \
    } while(0)

#endif /* INPUT_VALIDATION_H */
