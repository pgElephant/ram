#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Configuration entry */
typedef struct {
    char* key;
    char* value;
    char* section;
} config_entry_t;

/* Configuration context */
typedef struct {
    config_entry_t* entries;
    size_t count;
    size_t capacity;
} config_context_t;

/* Configuration functions */
config_context_t* config_parse_file(const char* filename);
void config_free(config_context_t* ctx);
const char* config_get_value(config_context_t* ctx, const char* section, const char* key);
int config_get_int(config_context_t* ctx, const char* section, const char* key, int default_value);
bool config_get_bool(config_context_t* ctx, const char* section, const char* key, bool default_value);
void config_set_value(config_context_t* ctx, const char* section, const char* key, const char* value);
bool config_save_file(config_context_t* ctx, const char* filename);

/* Validation functions */
bool config_validate_required(config_context_t* ctx, const char* section, const char* key);
bool config_validate_port(int port);
bool config_validate_ip(const char* ip);

#endif /* CONFIG_PARSER_H */
