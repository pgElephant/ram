#ifndef MEMORY_CLEANUP_H
#define MEMORY_CLEANUP_H

#include <stdlib.h>
#include <string.h>

/* Memory cleanup utilities */
#define CLEANUP_STRING(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)
#define CLEANUP_ARRAY(ptr, count) do { \
    if (ptr) { \
        for (size_t i = 0; i < (count); i++) { \
            if ((ptr)[i]) free((ptr)[i]); \
        } \
        free(ptr); \
        ptr = NULL; \
    } \
} while(0)

/* Safe memory allocation with cleanup */
#define SAFE_MALLOC(ptr, size) do { \
    (ptr) = malloc(size); \
    if (!(ptr)) { \
        fprintf(stderr, "Memory allocation failed at %s:%d\n", __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
    memset((ptr), 0, size); \
} while(0)

/* Safe string duplication with cleanup */
#define SAFE_STRDUP(ptr, str) do { \
    if (str) { \
        (ptr) = strdup(str); \
        if (!(ptr)) { \
            fprintf(stderr, "String duplication failed at %s:%d\n", __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } else { \
        (ptr) = NULL; \
    } \
} while(0)

#endif /* MEMORY_CLEANUP_H */
