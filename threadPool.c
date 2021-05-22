#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "osqueue.h"
#include "threadPool.h"

static void *threadLoop(void *arg) {
    ThreadPool *tp = (ThreadPool *)arg;
    while (THREAD_POOL_DESTROYED != tp->state.tps) { // while the thread pool not destroyed
        pthread_mutex_lock(tp->state.lock);          // lock
        pthread_cond_wait(tp->state.availableTaskCond, tp->state.lock); // wait for a task from the queue
        pthread_mutex_unlock(tp->state.lock); // unlock the mutex

        // Take a task from the queue -> Dequeue

        // Execute task
    }
}

ThreadPool* tpCreate(int numOfThreads) {
    // allocate memory on the heap for the thread-pool
    ThreadPool *tp = malloc(sizeof(ThreadPool));
    if (NULL == tp) {
        return NULL;
    }
    tp->taskQueue = osCreateQueue(); // build a queue for the thread pool
    tp->threads = malloc(numOfThreads * sizeof(pthread_t *)); // threads list
    // create number of threads as we requested in the function
    for (int i = 0; i < numOfThreads; i++) {
        int res = pthread_create(tp->threads[i], NULL, threadLoop, tp);
        if (0 != res) {
            // Error checking
            goto error;
        }
    }
    // Initialize the max count of threads
    tp->maxTaskCount = numOfThreads;
    // Initialize the number of the current tasks of the thread pool
    tp->state.currentTaskCount = 0;
    // Initialize the state of the thread pool
    tp->state.tps = THREAD_POOL_ACTIVE;
    // Initialize lock
    pthread_mutex_init(tp->state.lock, NULL);
    return tp;

error: // In case of error, free the memory that allocated
    free(tp->threads);
    return NULL;
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    // If trying to destroy thread pool that already destroyed
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