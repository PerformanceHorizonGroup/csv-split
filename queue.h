/*
 * fqueue.h
 *
 *  Created on: Jan 27, 2013
 *      Author: mike
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include <pthread.h>
#include <semaphore.h>

/**
 * A very simple linked list based item for our queue.  The data can 
 * be whatever the user wants, but it must be freed by the caller
 */
struct queue_item {
    /**
     * The data in our item
     */
    void *data;
     
    /**
     * Next item in our queue
     */
    struct queue_item *next; 
};

/**
 * Thread-safe blocking queue with a maximum number of items.  The
 * queue will block when empty or when the maximum number of items
 * is reached.  When you're done with the queue just call fq_fin()
 * to add a NULL element which will unblock the queue such that
 * consumers can finish and we can join worker threads
 */
typedef struct _fqueue {
	/**
     * Our items
     */
    struct queue_item *items;

    /**
      * Fist, last items
     */
    struct queue_item *first, *last;

    /**
     * Maximum length of queue
    */
    unsigned int max_len;

    /** 
     * Length of our queue
     */
	unsigned int len;

    /**
     * Mutex, for exclusive access to our queue
    */
    pthread_mutex_t mutex;

    /**
     * Condition variable to block when we're full
     */
    pthread_cond_t full;

    /**
     * Condition variable to block when we're empty
     */
    pthread_cond_t empty;
} fqueue;

/**
 * Initialize a queue with a maximum length
 */
int fq_init(fqueue *queue, size_t max_len);

/**
 * Add an item to our queue
 */
int fq_add(fqueue *queue, void *data);

/**
 * Get something from the queeue
 */
int fq_get(fqueue *queue, void **data);

/**
 * Add a null element which will flag our queue as done
 */
int fq_fin(fqueue *queue);

/**
 * Free a queue
 */
int fq_free(fqueue *queue);

#endif /* QUEUE_H_ */
