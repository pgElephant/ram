#ifndef RAM_CONFIG_ADVANCED_H
#define RAM_CONFIG_ADVANCED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Advanced configuration management */
typedef struct {
    char* postgresql_host;
    int postgresql_port;
    char* postgresql_user;
    char* postgresql_password;
    char* postgresql_database;
    int api_port;
    int http_port;
    char* cluster_name;
    char* log_file;
    int log_level;
    bool enable_ssl;
    char* ssl_cert;
    char* ssl_key;
    char* auth_token;
} ram_config_advanced_t;

/* Configuration functions */
ram_config_advanced_t* ram_config_advanced_create(void);
void ram_config_advanced_destroy(ram_config_advanced_t* config);
bool ram_config_advanced_load(ram_config_advanced_t* config, const char* filename);
bool ram_config_advanced_save(ram_config_advanced_t* config, const char* filename);
const char* ram_config_advanced_get_string(ram_config_advanced_t* config, const char* key);
int ram_config_advanced_get_int(ram_config_advanced_t* config, const char* key);
bool ram_config_advanced_get_bool(ram_config_advanced_t* config, const char* key);
void ram_config_advanced_set_string(ram_config_advanced_t* config, const char* key, const char* value);
void ram_config_advanced_set_int(ram_config_advanced_t* config, const char* key, int value);
void ram_config_advanced_set_bool(ram_config_advanced_t* config, const char* key, bool value);

/* Environment variable support */
bool ram_config_advanced_load_from_env(ram_config_advanced_t* config);
void ram_config_advanced_print(ram_config_advanced_t* config);

#endif /* RAM_CONFIG_ADVANCED_H */
