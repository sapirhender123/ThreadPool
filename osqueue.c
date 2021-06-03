#include "osqueue.h"
#include <stdlib.h>

/**
 * create the queue
 * @return a pointer to the queue
 */
OSQueue* osCreateQueue()
{
    OSQueue* q = malloc(sizeof(OSQueue));

    if(q == NULL)
        return NULL;

    q->head = q->tail = NULL;

    return q;
}

/**
 * destroy queue
 * @param q
 */
void osDestroyQueue(OSQueue* q)
{
    if(q == NULL)
        return;

    while(osDequeue(q) != NULL);

    free(q);
}
/**
 * Check if the queue is empty
 * @param q
 * @return
 */
int osIsQueueEmpty(OSQueue* q)
{
    return (q->tail == NULL && q->head == NULL);
}

/**
 * enqueue data to the queue
 * @param q
 * @param data
 */
void osEnqueue(OSQueue* q, void* data)
{
    OSNode* node = malloc(sizeof(OSNode));


    node->data = data;
    node->next = NULL;

    if(q->tail == NULL)
    {
        q->head=q->tail=node;
        return;
    }

    q->tail->next = node;
    q->tail = node;

}
/**
 * dequque data from the queue
 * @param q
 * @return
 */
void* osDequeue(OSQueue* q)
{
    OSNode* previousHead;
    void* data;

    previousHead = q->head;

    if(previousHead == NULL)
        return NULL;

    q->head = q->head->next;

    if (q->head == NULL)
        q->tail = NULL;

    data = previousHead->data;
    free(previousHead);
    return data;
}
