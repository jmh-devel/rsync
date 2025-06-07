#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "threadpool.h"

static void *threadpool_thread(void *threadpool);

threadpool_t *threadpool_create(int thread_count, int queue_size) {
    threadpool_t *pool;
    int i;

    if(thread_count <= 0 || thread_count > 1024 || queue_size <= 0 || queue_size > 65536) {
        return NULL;
    }

    if((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
        goto err;
    }

    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = 0;

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);

    if((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
       (pthread_cond_init(&(pool->notify), NULL) != 0) ||
       (pool->threads == NULL) ||
       (pool->queue == NULL)) {
        goto err;
    }

    for(i = 0; i < thread_count; i++) {
        if(pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool) != 0) {
            threadpool_destroy(pool);
            return NULL;
        }
        pool->thread_count++;
    }

    return pool;

 err:
    if(pool) {
        threadpool_destroy(pool);
    }
    return NULL;
}

int threadpool_add(threadpool_t *pool, void (*function)(void *), void *argument) {
    int err = 0;
    int next;

    if(pool == NULL || function == NULL) {
        return -1;
    }

    if(pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    next = (pool->tail + 1) % pool->queue_size;

    do {
        if(pool->count == pool->queue_size) {
            err = -2; 
            break;
        }

        if(pool->shutdown) {
            err = -3;
            break;
        }

        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;
        pool->tail = next;
        pool->count += 1;

        if(pthread_cond_signal(&(pool->notify)) != 0) {
            err = -4;
            break;
        }
    } while(0);

    if(pthread_mutex_unlock(&pool->lock) != 0) {
        err = -1;
    }

    return err;
}

int threadpool_destroy(threadpool_t *pool) {
    int i;

    if(pool == NULL) {
        return -1;
    }

    if(pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    if(pool->shutdown) {
        if(pthread_mutex_unlock(&pool->lock) != 0) {
            return -1;
        }
        return -2;
    }

    pool->shutdown = 1;

    if((pthread_cond_broadcast(&(pool->notify)) != 0) ||
       (pthread_mutex_unlock(&(pool->lock)) != 0)) {
        return -1;
    }

    for(i = 0; i < pool->thread_count; i++) {
        if(pthread_join(pool->threads[i], NULL) != 0) {
            return -1;
        }
    }

    if(pool->threads) {
        free(pool->threads);
    }
    if(pool->queue) {
        free(pool->queue);
    }
    
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
    
    free(pool);

    return 0;
}

static void *threadpool_thread(void *threadpool) {
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;

    for(;;) {
        pthread_mutex_lock(&(pool->lock));

        while((pool->count == 0) && (!pool->shutdown)) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        if(pool->shutdown) {
            break;
        }

        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count -= 1;

        pthread_mutex_unlock(&(pool->lock));

        (*(task.function))(task.argument);
    }

    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return(NULL);
}