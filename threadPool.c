#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "osqueue.h"
#include "threadPool.h"

static void *threadLoop(void *arg) {
    ThreadPool *tp = (ThreadPool *)arg;
    while (THREAD_POOL_DESTROYED != tp->state.tps) {
        pthread_mutex_lock(tp->state.lock);
        pthread_cond_wait(tp->state.availableTaskCond, tp->state.lock);
        pthread_mutex_unlock(tp->state.lock);

        // Take a task from the queue -> Dequeue

        // Execute task
    }
}

ThreadPool* tpCreate(int numOfThreads) {
    ThreadPool *tp = malloc(sizeof(ThreadPool));
    if (NULL == tp) {
        return NULL;
    }

    tp->taskQueue = osCreateQueue(); // build a queue for the thread pool
    tp->threads = malloc(numOfThreads * sizeof(pthread_t *)); // threads list
    for (int i = 0; i < numOfThreads; i++) {
        int res = pthread_create(tp->threads[i], NULL, threadLoop, tp);
        if (0 != res) {
            // Error checking
            goto error;
        }
    }

    tp->maxTaskCount = numOfThreads;
    tp->state.currentTaskCount = 0;
    tp->state.tps = THREAD_POOL_ACTIVE;
    pthread_mutex_init(tp->state.lock, NULL);
    return tp;

error:
    free(tp->threads);
    return NULL;
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    if (THREAD_POOL_DESTROYED == threadPool->state.tps) {
        return;
    }

    // Acquire mutex
    threadPool->state.tps = THREAD_POOL_DESTROYED;

    // if shouldWaitForTasks -> wait until queueTask is empty
    // else -> delete all tasks from queueTask
    // pthread_join on all threads
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    if (THREAD_POOL_DESTROYED == threadPool->state.tps) {
        // Error
        return -1;
    }

    // Enqueue task into taskQueue
    osEnqueue(threadPool->taskQueue, (void *)computeFunc);

    // Notify threads to take a task
    pthread_cond_broadcast(threadPool->state.availableTaskCond);
    return 0;
}