#include "queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Struct representing a node in a queue_t
 */
typedef struct queue_node_t {

  struct queue_node_t *next;
  void *data;
} queue_node_t;

/**
 * Struct representing a queue
 */
struct queue_t {

  queue_node_t *head, *tail;
  int size;
  int maxSize;
  pthread_cond_t cv;
  pthread_mutex_t m;
};

/**
 *  Given data, place it on the queue.  Can be called by multiple threads.
 *  Blocks if the queue is full.
 */
void queue_push(queue_t *queue, void *data) {
  queue_node_t *nd = (queue_node_t*) malloc(sizeof(queue_node_t));
  nd->data = data;
  nd->next = NULL;
  pthread_mutex_lock(&(queue->m));
  while (queue->maxSize > 0 && queue->size == queue->maxSize) {
    pthread_cond_wait(&(queue->cv), &(queue->m));
  }
  queue->size++;
  if (queue->tail) {
    queue->tail->next = nd;
  }
  else {
    queue->head = nd;
  }
  queue->tail = nd;
  if (queue->size == 1) pthread_cond_broadcast(&(queue->cv));
  pthread_mutex_unlock(&(queue->m));
}

/**
 *  Retrieve the data from the front of the queue.  Can be called by multiple
 * threads.
 *  Blocks if the queue is empty.
 */
void *queue_pull(queue_t *queue) {
  pthread_mutex_lock(&(queue->m));
  while (queue->size == 0) pthread_cond_wait(&(queue->cv), &(queue->m));
  queue_node_t *tofree = queue->head;
  void *data = queue->head->data;
  queue->size--;
  if (queue->size == 0) {
    queue->tail = NULL;
  }
  queue->head = queue->head->next;
  free(tofree);
  if (queue->maxSize > 0 && queue->size == queue->maxSize - 1) 
	pthread_cond_broadcast(&(queue->cv));
  pthread_mutex_unlock(&(queue->m));
  return data;
}

/**
 *  Allocates heap memory for a queue_t and initializes it.
 *  Returns a pointer to this allocated space.
 */
queue_t *queue_create(int maxSize) {
  queue_t *q = (queue_t*) malloc(sizeof(queue_t));
  q->size = 0;
  q->maxSize = maxSize;
  pthread_cond_init(&(q->cv), NULL);
  pthread_mutex_init(&(q->m), NULL);
  q->head = NULL;
  q->tail = NULL;
  return q;
}

/**
 *  Destroys the queue, freeing any remaining nodes in it.
 */
void queue_destroy(queue_t *queue) {
  queue_node_t *curr = queue->head;
  queue_node_t *tofree = NULL;
  while (curr) {
    tofree = curr;
    curr = curr->next;
    free(tofree);
  }
  pthread_mutex_destroy(&(queue->m));
  pthread_cond_destroy(&(queue->cv));
  free(queue);
	fprintf(stderr, "queue is destroyed\n");
  queue = NULL;
}

