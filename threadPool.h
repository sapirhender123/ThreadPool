#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"

typedef enum {
    THREAD_POOL_ACTIVE = 0,
    THREAD_POOL_DESTROYED,
} ThreadPoolState;

typedef struct thread_pool
{
    OSQueue* taskQueue;
    pthread_t **threads;
    size_t maxTaskCount;

    struct {
        // Increase - When adding a task
        // Decrease - When deleting a task
        size_t currentTaskCount;

        pthread_mutex_t *lock;
        pthread_cond_t *availableTaskCond;

        // State - one of ThreadPoolState
        ThreadPoolState tps;
    } state;
} ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif // __THREAD_POOL__
