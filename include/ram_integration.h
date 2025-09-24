#ifndef RAM_INTEGRATION_H
#define RAM_INTEGRATION_H

#include "ramd_common.h"
#include "pgraft.h"

/* Integration status */
typedef enum {
    RAM_INTEGRATION_OK = 0,
    RAM_INTEGRATION_ERROR = 1,
    RAM_INTEGRATION_NOT_INITIALIZED = 2
} ram_integration_status_t;

/* Integration functions */
ram_integration_status_t ram_integration_init(void);
ram_integration_status_t ram_integration_cleanup(void);
ram_integration_status_t ram_pgraft_to_ramd_communication(void);
ram_integration_status_t ram_ramd_to_ramctrl_communication(void);

/* Configuration-based communication */
char* ram_get_config_value(const char* key, const char* default_value);
int ram_set_config_value(const char* key, const char* value);

#endif /* RAM_INTEGRATION_H */
