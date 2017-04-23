#ifndef _QUEUEH_
#define _QUEUEH_

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
/**
 * Struct representing a queue
 */
typedef struct queue_node_t {

  struct queue_node_t *next;
  void *data;
} queue_node_t;

typedef struct queue_t {
  queue_node_t *head, *tail;
  int size;
  int maxSize;
  pthread_cond_t cv;
  pthread_mutex_t m;
} queue_t;
/**
 *  Given data, place it on the queue.  Can be called by multiple threads.
 *  Blocks if the queue is full.
 */
void queue_push(queue_t *queue, void *data);

/**
 *  Retrieve the data from the front of the queue.  Can be called by multiple
 * threads.
 *  Blocks if the queue is empty.
 */
void *queue_pull(queue_t *queue);

/**
 *  Allocates heap memory for a queue_t and initializes it.
 *  Returns a pointer to this allocated space.
 */
queue_t *queue_create(int maxSize);

/**
 *  Destroys the queue, freeing any remaining nodes in it.
 */
void queue_destroy(queue_t *queue);

#endif
