#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <stdlib.h>
#include <string.h>

/* Safe memory allocation with error checking */
static inline void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        /* Log error and exit */
        exit(1);
    }
    return ptr;
}

/* Safe memory reallocation with error checking */
static inline void* safe_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        free(ptr);
        exit(1);
    }
    return new_ptr;
}

/* Safe string duplication */
static inline char* safe_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* copy = safe_malloc(len);
    strcpy(copy, str);
    return copy;
}

/* Memory cleanup macro */
#define CLEANUP_MEMORY(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

#endif /* MEMORY_UTILS_H */
