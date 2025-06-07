#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

// Task structure
typedef struct {
    void (*function)(void *arg);
    void *argument;
} threadpool_task_t;

// Thread pool structure
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    threadpool_task_t *queue;
    int thread_count;
    int queue_size;
    int head;
    int tail;
    int count;
    int shutdown;
} threadpool_t;

threadpool_t *threadpool_create(int thread_count, int queue_size);
int threadpool_add(threadpool_t *pool, void (*function)(void *arg), void *argument);
int threadpool_destroy(threadpool_t *pool);

#endif /* THREADPOOL_H */