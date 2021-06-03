#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#include "osqueue.h"
#include "threadPool.h"

#define CHECK_PRINT(errnum, msg) do {           \
    if (0 != errnum) {                          \
        printf(msg": %s\n", strerror(errnum));  \
        goto error;                             \
    }                                           \
} while (0);

#define CHECK_PERROR(cond, msg) do {    \
    if (!(cond)) {                      \
        perror(msg);                    \
        goto error;                     \
    }                                   \
} while (0);

static void *threadLoop(void *arg) {
    thread_arg *ta = (thread_arg *)arg;
    ThreadPool *tp = ta->tp;

    while (true) {
        CHECK_PRINT(pthread_mutex_lock(&tp->state.lock), "Lock failed");

        // wait for a task from the queue
        while (osIsQueueEmpty(tp->taskQueue) && THREAD_POOL_DESTROYED != tp->state.tps) {
            CHECK_PRINT(pthread_cond_wait(&tp->state.availableTaskCond, &tp->state.lock), "Wait failed");
        }

        if (THREAD_POOL_DESTROYED == tp->state.tps) {
            break;
        }

        // Take a task from the queue -> Dequeue
        task *t = osDequeue(tp->taskQueue);
        tp->state.currentTaskCount++;
        CHECK_PRINT(pthread_mutex_unlock(&tp->state.lock), "Unlock failed"); // unlock the mutex

        if (NULL != t) {
            // No need to hold the lock when executing a task
            t->func(t->param);
            free(t);
        }

        CHECK_PRINT(pthread_mutex_lock(&tp->state.lock), "Lock failed");
        tp->state.currentTaskCount--;
        if (THREAD_POOL_DESTROYED != tp->state.tps // ThreadPool still alive
            && 0 == tp->state.currentTaskCount // All workers finished executing tasks
            && osIsQueueEmpty(tp->taskQueue)) // No more work in the pool
        {
            // Signal the thread pool that the work is done
            CHECK_PRINT(pthread_cond_signal(&tp->state.workDoneCond), "Signal failed");
        }
        CHECK_PRINT(pthread_mutex_unlock(&tp->state.lock), "Unlock failed");
    }

error:
    tp->state.currentThreadCount--;
    CHECK_PRINT(pthread_cond_signal(&tp->state.workDoneCond), "Signal failed");
    CHECK_PRINT(pthread_mutex_unlock(&tp->state.lock), "Unlock failed");

#ifdef DEBUG
    printf("[%d] Exited\n", ta->id);
#endif

    free(ta);
    pthread_exit(NULL);
}

ThreadPool* tpCreate(int numOfThreads) {
    // allocate memory on the heap for the thread-pool
    ThreadPool *tp = malloc(sizeof(ThreadPool));
    CHECK_PERROR((NULL != tp), "Malloc Failed!");

    // Initialize the max count of threads
    tp->maxTaskCount = numOfThreads;
    tp->state.currentThreadCount = numOfThreads;

    // Initialize the state of the thread pool
    tp->state.tps = THREAD_POOL_ACTIVE;

    int i = 0;
    tp->threads = NULL;

    // Initialize lock
    CHECK_PRINT(0 != pthread_mutex_init(&tp->state.lock, NULL), "Mutex init failed");

    // Initialize conditions
    CHECK_PRINT(0 != pthread_cond_init(&tp->state.availableTaskCond, NULL), "Cond init failed");
    CHECK_PRINT(0 != pthread_cond_init(&tp->state.workDoneCond, NULL), "Cond init failed");

    tp->taskQueue = osCreateQueue(); // build a queue for the thread pool
    tp->threads = malloc(numOfThreads * sizeof(pthread_t *)); // threads list
    CHECK_PERROR(NULL != tp->threads, "Malloc Failed");

    // create number of threads as we requested
    for (i = 0; i < numOfThreads; i++) {
        tp->threads[i] = malloc(sizeof(pthread_t *));
        CHECK_PERROR(NULL != tp->threads[i], "Malloc Failed");

        thread_arg *ta = malloc(sizeof(thread_arg));
        CHECK_PERROR(NULL != ta, "Malloc Failed");
        ta->tp = tp;
        ta->id = i;
        CHECK_PRINT(pthread_create(tp->threads[i], NULL, threadLoop, ta), "Create thread Failed");
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
    if (threadPool == NULL || THREAD_POOL_DESTROYED == threadPool->state.tps) {
        return;
    }

    CHECK_PRINT(pthread_mutex_lock(&threadPool->state.lock), "Lock failed");
    if (0 == shouldWaitForTasks) {
        // Don't wait for all work to be done - destroy work in queue
        while (!osIsQueueEmpty(threadPool->taskQueue)) {
            free(osDequeue(threadPool->taskQueue));
        }
    } else {
        // Wait for all workers to empty the task queue and finish all available work
        while (true) {
            if (!osIsQueueEmpty(threadPool->taskQueue)) { // if there is still work in the queue
                // Wait for a worker to signal that the work is done
                CHECK_PRINT(pthread_cond_wait(&threadPool->state.workDoneCond, &threadPool->state.lock), "Cond wait failed");
            } else {
                break;
            }
        }
    }

    // Set state of thread pool to destroyed
    threadPool->state.tps = THREAD_POOL_DESTROYED;

    // Notify any worker that is waiting on availableTaskCond that the thread pool is destroyed
    CHECK_PRINT(pthread_cond_broadcast(&threadPool->state.availableTaskCond), "Broadcast failed");

    // Wait for threads to finish working
    // When a worker detects that no workers are executing tasks and the task pool is empty, it notifies
    // the pool manager via the workDoneCond condition.
    while (true) {
        if (threadPool->state.currentThreadCount != 0) { // if there is any thread executing a task
            CHECK_PRINT(pthread_cond_wait(&threadPool->state.workDoneCond, &threadPool->state.lock), "Cond wait failed");
        } else {
            break;
        }
    }

    // Now we know that all threads have exited
    CHECK_PRINT(pthread_mutex_unlock(&threadPool->state.lock), "Unlock failed");

    // Now we can release allocated memory
    int k;
    for (k = 0; k < threadPool->maxTaskCount; k++) {
        CHECK_PRINT(pthread_join(*threadPool->threads[k], NULL), "Join Failed");
        free(threadPool->threads[k]);
    }

    if (NULL != threadPool->threads) {
        free(threadPool->threads);
    }

    osDestroyQueue(threadPool->taskQueue);
    free(threadPool);
    return;

    error:;
    exit(-1);
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    if (THREAD_POOL_DESTROYED == threadPool->state.tps) {
        // Cannot insert a task to a destroyed thread pool
        return -1;
    }

    if (NULL == threadPool->taskQueue) {
        return -1;
    }

    task *t = NULL;
    CHECK_PRINT(pthread_mutex_lock(&threadPool->state.lock), "Lock Failed");

    // Enqueue task into taskQueue
    t = malloc(sizeof(task));
    if (NULL == t) {
        perror("Malloc Failed");
        CHECK_PRINT(pthread_mutex_unlock(&threadPool->state.lock), "Unlock Failed");
        goto error;
    }

    t->func = computeFunc;
    t->param = param;
    osEnqueue(threadPool->taskQueue, t); // Insert to the queue
    CHECK_PRINT(pthread_mutex_unlock(&threadPool->state.lock), "Unlock Failed");

    // Notify workers to take a task
    CHECK_PRINT(pthread_cond_broadcast(&threadPool->state.availableTaskCond), "Signal Failed");
    return 0;

    error:
    if (t != NULL) {
        free(t);
    }
    return -1;
}
