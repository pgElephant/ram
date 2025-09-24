#ifndef SECURITY_HARDENING_H
#define SECURITY_HARDENING_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Security validation functions */
bool security_validate_input(const char* input, size_t max_length);
bool security_validate_port(int port);
bool security_validate_ip(const char* ip);
bool security_validate_path(const char* path);
bool security_validate_username(const char* username);
bool security_validate_password(const char* password);

/* Input sanitization */
char* security_sanitize_string(const char* input, size_t max_length);
char* security_sanitize_path(const char* path);
char* security_sanitize_sql(const char* sql);

/* Encryption utilities */
char* security_encrypt_string(const char* input, const char* key);
char* security_decrypt_string(const char* encrypted, const char* key);
char* security_hash_password(const char* password);
bool security_verify_password(const char* password, const char* hash);

/* Access control */
typedef struct {
    char* username;
    char* password_hash;
    char* permissions;
    bool is_active;
} user_account_t;

typedef struct {
    user_account_t* users;
    size_t count;
    size_t capacity;
} access_control_t;

access_control_t* access_control_create(void);
void access_control_destroy(access_control_t* ac);
bool access_control_add_user(access_control_t* ac, const char* username, const char* password, const char* permissions);
bool access_control_authenticate(access_control_t* ac, const char* username, const char* password);
bool access_control_authorize(access_control_t* ac, const char* username, const char* permission);
void access_control_remove_user(access_control_t* ac, const char* username);

/* Rate limiting */
typedef struct {
    uint64_t requests;
    uint64_t window_start;
    uint64_t window_duration;
    uint64_t max_requests;
} rate_limiter_t;

rate_limiter_t* rate_limiter_create(uint64_t max_requests, uint64_t window_duration);
void rate_limiter_destroy(rate_limiter_t* limiter);
bool rate_limiter_allow_request(rate_limiter_t* limiter);
void rate_limiter_reset(rate_limiter_t* limiter);

#endif /* SECURITY_HARDENING_H */
