#ifndef THREAD_SAFETY_H
#define THREAD_SAFETY_H

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

/* Thread-safe counter */
typedef struct {
    int value;
    pthread_mutex_t mutex;
} thread_safe_counter_t;

/* Thread-safe queue */
typedef struct {
    void** data;
    size_t size;
    size_t capacity;
    size_t head;
    size_t tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} thread_safe_queue_t;

/* Thread-safe hash table */
typedef struct {
    void** keys;
    void** values;
    size_t size;
    size_t capacity;
    pthread_mutex_t mutex;
} thread_safe_hash_t;

/* Counter functions */
thread_safe_counter_t* thread_safe_counter_create(void);
void thread_safe_counter_destroy(thread_safe_counter_t* counter);
int thread_safe_counter_get(thread_safe_counter_t* counter);
void thread_safe_counter_increment(thread_safe_counter_t* counter);
void thread_safe_counter_decrement(thread_safe_counter_t* counter);

/* Queue functions */
thread_safe_queue_t* thread_safe_queue_create(size_t capacity);
void thread_safe_queue_destroy(thread_safe_queue_t* queue);
bool thread_safe_queue_enqueue(thread_safe_queue_t* queue, void* item);
void* thread_safe_queue_dequeue(thread_safe_queue_t* queue);
bool thread_safe_queue_is_empty(thread_safe_queue_t* queue);
bool thread_safe_queue_is_full(thread_safe_queue_t* queue);

/* Hash table functions */
thread_safe_hash_t* thread_safe_hash_create(size_t capacity);
void thread_safe_hash_destroy(thread_safe_hash_t* hash);
void thread_safe_hash_put(thread_safe_hash_t* hash, void* key, void* value);
void* thread_safe_hash_get(thread_safe_hash_t* hash, void* key);
void thread_safe_hash_remove(thread_safe_hash_t* hash, void* key);

#endif /* THREAD_SAFETY_H */
