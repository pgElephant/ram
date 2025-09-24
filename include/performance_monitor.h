#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

/* Performance metrics */
typedef struct {
    double cpu_usage;
    double memory_usage;
    double disk_io;
    double network_io;
    long long timestamp;
} performance_metrics_t;

/* Performance monitor */
typedef struct {
    performance_metrics_t* metrics;
    size_t count;
    size_t capacity;
    pthread_mutex_t mutex;
} performance_monitor_t;

/* Performance functions */
performance_monitor_t* performance_monitor_create(size_t capacity);
void performance_monitor_destroy(performance_monitor_t* monitor);
void performance_monitor_add_metrics(performance_monitor_t* monitor, performance_metrics_t* metrics);
performance_metrics_t* performance_monitor_get_latest(performance_monitor_t* monitor);
performance_metrics_t* performance_monitor_get_average(performance_monitor_t* monitor, size_t count);
bool performance_monitor_is_healthy(performance_monitor_t* monitor);

/* System monitoring */
performance_metrics_t* performance_get_system_metrics(void);
double performance_get_cpu_usage(void);
double performance_get_memory_usage(void);
double performance_get_disk_io(void);
double performance_get_network_io(void);

#endif /* PERFORMANCE_MONITOR_H */
