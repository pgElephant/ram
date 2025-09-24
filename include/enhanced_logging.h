#ifndef ENHANCED_LOGGING_H
#define ENHANCED_LOGGING_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

/* Log levels */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4
} log_level_t;

/* Log context */
typedef struct {
    log_level_t level;
    const char* component;
    const char* file;
    int line;
    const char* function;
} log_context_t;

/* Enhanced logging functions */
void enhanced_log(log_level_t level, const char* component, const char* file, int line, const char* function, const char* format, ...);
void enhanced_log_init(void);
void enhanced_log_set_level(log_level_t level);
void enhanced_log_set_output(FILE* output);

/* Convenience macros */
#define LOG_DEBUG(component, format, ...) \
    enhanced_log(LOG_LEVEL_DEBUG, (component), __FILE__, __LINE__, __FUNCTION__, (format), ##__VA_ARGS__)

#define LOG_INFO(component, format, ...) \
    enhanced_log(LOG_LEVEL_INFO, (component), __FILE__, __LINE__, __FUNCTION__, (format), ##__VA_ARGS__)

#define LOG_WARN(component, format, ...) \
    enhanced_log(LOG_LEVEL_WARN, (component), __FILE__, __LINE__, __FUNCTION__, (format), ##__VA_ARGS__)

#define LOG_ERROR(component, format, ...) \
    enhanced_log(LOG_LEVEL_ERROR, (component), __FILE__, __LINE__, __FUNCTION__, (format), ##__VA_ARGS__)

#define LOG_FATAL(component, format, ...) \
    enhanced_log(LOG_LEVEL_FATAL, (component), __FILE__, __LINE__, __FUNCTION__, (format), ##__VA_ARGS__)

#endif /* ENHANCED_LOGGING_H */
