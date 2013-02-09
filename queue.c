#include "queue.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

// Initialize our background queue with a maximum size
int fq_init(fqueue *queue, size_t max_len) {
    // Assume success
    int ret = 0;
    
    // NULL sanity check
    if(queue == NULL) {
        return EINVAL;
    }

    // Null everything out
    memset(queue, 0, sizeof(fqueue));

    // Set our maximum length
    queue->max_len = max_len;

    // Empty queue condition
    if((ret = pthread_cond_init(&queue->empty, NULL))) {
        return ret;
    }

    // Full queue condition
    if((ret = pthread_cond_init(&queue->full, NULL))) {
        pthread_cond_destroy(&queue->empty);
        return ret;
    }

    // Exclusive access mutex
    if((ret = pthread_mutex_init(&queue->mutex, NULL))) {
        pthread_cond_destroy(&queue->empty);
        pthread_cond_destroy(&queue->full);
        return ret;
    }

    // Success!
    return 0;
}

int fq_add(fqueue *queue, void *data) {
    struct queue_item *item;

    // Attain exclusive access
    pthread_mutex_lock(&queue->mutex);

    // Block if our queue is full
    while(queue->len == queue->max_len) {
        pthread_cond_wait(&queue->full, &queue->mutex);
    }

    // Allocate our item, set members
    item = malloc(sizeof(struct queue_item));

    // Set members
    item->data = data;
    item->next = NULL;
    
    if(queue->last == NULL) {
        queue->last = item;
        queue->first = item;
    } else {
        queue->last->next = item;
        queue->last = item;
    }

    // If our list was empty, let anyone waiting for an item continue
    if(queue->len == 0) {
        pthread_cond_broadcast(&queue->empty);
    }

    // Increase our queue length
    queue->len++;

    // Release Mutex
    pthread_mutex_unlock(&queue->mutex);

    // Success
    return 0;
}

int fq_get(fqueue *queue, void **data) {
    // Argument sanity check
    if(queue == NULL) {
        return EINVAL;
    }

    // Gain exclusive access
    pthread_mutex_lock(&queue->mutex);

    // Block if our queue is empty
    while(queue->first == NULL) {
        pthread_cond_wait(&queue->empty, &queue->mutex);
    }

    // If our data is not NULL it's an actual element, otherwise
    // it's a fake one that lets our consumers finish.
    if(queue->first->data) {
    	// Extract data
    	*data = queue->first->data;

    	// Grab a copy of this element
    	struct queue_item *tmp = queue->first;

    	// Move our first pointer
    	queue->first = queue->first->next;
    	queue->len--;

    	// Free our removed element
    	free(tmp);

    	// If we are out of elements, update last as well
    	if(queue->first == NULL) {
    		queue->last = NULL;
    		queue->len = 0;
    	}
    } else {
        *data = NULL;
    }

    // Unlock anyone waiting to add to the thread
    if(queue->len == queue->max_len - 1) {
    	pthread_cond_broadcast(&queue->full);
    }

    // Release exclusive access
    pthread_mutex_unlock(&queue->mutex);

    // Return zero if we're sending non null data
    return *data == NULL;
}

// Flag our queue as being done so consumers can exit
int fq_fin(fqueue *queue) {
    return fq_add(queue, NULL);
}

// Free our list
int fq_free(fqueue *queue) {
    // Sanity check
    if(queue == NULL) {
        return EINVAL;
    }

    // Gain exclusive access
    pthread_mutex_lock(&queue->mutex);

    struct queue_item *cur = queue->first, *next;

    // Free all of our items
    while(cur) {
        next = cur->next;
        free(cur);
        cur = next;
    }

    // Relese exclusive access, destroy mutex
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);

    // Destroy conditions
    pthread_cond_destroy(&queue->empty);
    pthread_cond_destroy(&queue->full);

    // Success!
    return 0;
}
