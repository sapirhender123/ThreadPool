#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "osqueue.h"
#include "threadPool.h"

#define CHECK_PRINT(errnum, msg) do {           \
    if (0 != errnum) {                          \
        printf(msg": %s\n", strerror(errnum));  \
        goto error;                             \
    }                                           \
while (0);

#define CHECK_PERROR(cond, msg) do {    \
    if (!cond) {                        \
        perror(msg);                    \
        goto error;                     \
    }                                   \
while (0);

static void *threadLoop(void *arg) {
    ThreadPool *tp = (ThreadPool *)arg;

    // while the thread pool not destroyed
    while (THREAD_POOL_DESTROYED != tp->state.tps) {
        CHECK_PRINT(pthread_mutex_lock(&tp->state.lock), "Lock failed");
        CHECK_PRINT(pthread_cond_wait(&tp->state.availableTaskCond, &tp->state.lock), "Wait failed");

        // wait for a task from the queue
        if (THREAD_POOL_DESTROYED == tp->state.tps) { // if now the thread pool destroyed
            goto unlock;
        }

        // Take a task from the queue -> Dequeue
        task *t = osDequeue(tp->taskQueue);
        if (NULL == t) {
            goto unlock;
        }

        CHECK_PRINT(pthread_mutex_unlock(&tp->state.lock), "Unlock failed"); // unlock the mutex

        // No need to hold the lock when executing a task
        t->func(t->param);
        free(t);

        error:;
    }

unlock:
    if (0 != pthread_mutex_unlock(&tp->state.lock)) {
      printf("Unlock failed");
    }
    return NULL;
}

ThreadPool* tpCreate(int numOfThreads) {
    // allocate memory on the heap for the thread-pool
    ThreadPool *tp = malloc(sizeof(ThreadPool));
    CHECK_PERROR((NULL != tp), "Malloc Failed!");

    // Initialize the max count of threads
    tp->maxTaskCount = numOfThreads;
    // Initialize the state of the thread pool
    tp->state.tps = THREAD_POOL_ACTIVE;

    int i = 0;
    tp->threads = NULL;

    // Initialize lock
    CHECK_PRINT(0 != pthread_mutex_init(&tp->state.lock, "Init failed"));
    CHECK_PRINT(0 != pthread_mutex_init(&tp->action_lock, "Init failed"));

    tp->taskQueue = osCreateQueue(); // build a queue for the thread pool
    tp->threads = malloc(numOfThreads * sizeof(pthread_t *)); // threads list
    CHECK_PERROR(NULL != tp->threads, "Malloc Failed"));

    // create number of threads as we requested
    for (; i < numOfThreads; i++) {
        tp->threads[i] = malloc(sizeof(pthread_t *));
        CHECK_PERROR(NULL != tp->threads[i], "Malloc Failed"));
        CHECK_PRINT(pthread_create(tp->threads[i], NULL, threadLoop, tp), "Create thread Failed"));
    }

    return tp;

error: // In case of error, free the memory that allocated
    if (NULL != tp->threads) {
        while (i--) {
            free(tp->threads[i]);
        }

        free(tp->threads);
    }
    return NULL;
}



void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    // If trying to destroy thread pool that already destroyed
    if (THREAD_POOL_DESTROYED == threadPool->state.tps) {
        return;
    }

    // Acquire mutex
    CHECK_PRINT(pthread_mutex_lock(&threadPool->action_lock), "Lock failed");

    // if shouldWaitForTasks is a non-zero and positive number, wait until queueTask is empty
    if (shouldWaitForTasks > 0) { // wait until all of the tasks will finish
        while (!osIsQueueEmpty(threadPool->taskQueue)) {
            pthread_cond_signal(&threadPool->state.availableTaskCond);
        }
    }

    threadPool->state.tps = THREAD_POOL_DESTROYED;
    pthread_cond_broadcast(&threadPool->state.availableTaskCond);

    // wait to all threads to be finished
    for (int i = 0; i < threadPool->maxTaskCount; i++) {
        CHECK_PRINT(pthread_join(*threadPool->threads[i], NULL), "Join Failed");
    }

    if (NULL != threadPool->threads) {
        free(threadPool->threads);
        threadPool->threads = NULL;
    }

    while (!osIsQueueEmpty(threadPool->taskQueue)) {
        task *t = osDequeue(threadPool->taskQueue);
        free(t);
    }

    if (NULL != threadPool->taskQueue) {
        free(threadPool->taskQueue);
        threadPool->taskQueue = NULL;
    }

    CHECK_PRINT(pthread_mutex_unlock(&threadPool->action_lock), "Unlock Failed");

    error:;
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    if (THREAD_POOL_DESTROYED == threadPool->state.tps) {
        // Error
        return -1;
    }
    // in order to prevent errors
    CHECK_PRINT(pthread_mutex_lock(&threadPool->action_lock), "Lock Failed");
    if (NULL == threadPool->taskQueue) {
        return -1;
    }

    // Enqueue task into taskQueue
    task *t = malloc(sizeof(task));
    CHECK_PERROR((NULL != t),"Malloc Failed");

    t->func = computeFunc;
    t->param = param;
    osEnqueue(threadPool->taskQueue, t); // Insert to the queue

    // Notify threads to take a task
    CHECK_PRINT(pthread_cond_signal(&threadPool->state.availableTaskCond), "Signal Failed");
    CHECK_PRINT(pthread_mutex_unlock(&threadPool->action_lock), "Unlock Failed");
    return 0;

    error:;
}