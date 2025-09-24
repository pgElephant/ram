#ifndef CLIENT_IP_CONFIG_H
#define CLIENT_IP_CONFIG_H

#include <stdbool.h>

/* Client IP configuration */
typedef struct {
    char default_client_ip[INET_ADDRSTRLEN];
    bool trust_proxy_headers;
    bool log_client_ips;
    char trusted_proxies[1024];
} client_ip_config_t;

/* Client IP functions */
void client_ip_config_init(client_ip_config_t* config);
const char* client_ip_get_default(void);
bool client_ip_is_trusted_proxy(const char* ip);
void client_ip_extract_from_headers(const char* headers, char* client_ip, size_t client_ip_size);
bool client_ip_validate(const char* ip);

/* Configuration loading */
bool client_ip_config_load_from_file(client_ip_config_t* config, const char* filename);
bool client_ip_config_load_from_env(client_ip_config_t* config);

#endif /* CLIENT_IP_CONFIG_H */
