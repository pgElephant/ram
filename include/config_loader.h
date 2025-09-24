#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <stdlib.h>
#include <string.h>

/* Configuration loader functions */
const char* config_get_env(const char* key, const char* default_value);
int config_get_env_int(const char* key, int default_value);
bool config_get_env_bool(const char* key, bool default_value);

/* Macro for getting environment variables with defaults */
#define GET_ENV(key, default) config_get_env(key, default)
#define GET_ENV_INT(key, default) config_get_env_int(key, default)
#define GET_ENV_BOOL(key, default) config_get_env_bool(key, default)

#endif /* CONFIG_LOADER_H */
