#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* Error handling macros */
#define CHECK_NULL(ptr, msg) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "ERROR: %s at %s:%d\n", (msg), __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

#define CHECK_RETURN(ret, msg) do { \
    if ((ret) < 0) { \
        fprintf(stderr, "ERROR: %s: %s at %s:%d\n", (msg), strerror(errno), __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

#define CHECK_PTR(ptr, msg) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "ERROR: %s at %s:%d\n", (msg), __FILE__, __LINE__); \
        return NULL; \
    } \
} while(0)

/* Safe string operations */
#define SAFE_STRCPY(dest, src, size) do { \
    if (strlen(src) >= (size)) { \
        fprintf(stderr, "ERROR: String too long for buffer at %s:%d\n", __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
    strcpy((dest), (src)); \
} while(0)

#define SAFE_SNPRINTF(dest, size, format, ...) do { \
    int ret = snprintf((dest), (size), (format), ##__VA_ARGS__); \
    if (ret < 0 || ret >= (size)) { \
        fprintf(stderr, "ERROR: String formatting failed at %s:%d\n", __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

#endif /* ERROR_HANDLING_H */
