#include <stdlib.h>
#include "wq.h"
#include "kvconstants.h"
#include "utlist.h"

/* Initializes a work queue WQ. Sets up any necessary synchronization constructs. */
void wq_init(wq_t *wq) {
  wq->head = NULL;
  pthread_mutex_init(&(wq->mutex), NULL);
  pthread_cond_init(&(wq->cond), NULL);
}

/* Remove an item from the WQ. Currently, this immediately attempts
 * to remove the item at the head of the list, and will fail if there are
 * no items in the list.
 *
 * It is your task to make it so that this function will wait until the queue
 * contains at least one item, then remove that item from the list and
 * return it. */
void *wq_pop(wq_t *wq) {
  void *job;
  pthread_mutex_lock(&(wq->mutex));
  while(wq->head == NULL){
	  pthread_cond_wait(&(wq->cond), &(wq->mutex));
  job = wq->head->item;
  DL_DELETE(wq->head,wq->head);
  pthread_mutex_unlock(&(wq->mutex));
  return job;
}

/* Add ITEM to WQ. Currently, this just adds ITEM to the list.
 *
 * It is your task to perform any necessary operations to properly
 * perform synchronization. */
void wq_push(wq_t *wq, void *item) {
  wq_item_t *wq_item = calloc(1, sizeof(wq_item_t));
  wq_item->item = item;
  pthread_mutex_lock(&(wq->mutex));
  DL_APPEND(wq->head, wq_item);
  pthread_cond_signal(&(wq->mutex));
  pthread_mutex_unlock(&(wq->mutex));
}

/* Destory WQ.
 *
 * Wait all the jobs are done. */
void wq_destory(wq_t *wq) {
	pthread_mutex_lock(&(wq->lock));
	while(wq->head != NULL) {
		pthread_cond_wait(&(wq->cond), &(wq->mutex));
	}
	pthread_mutex_unlock(&(wq->lock));
	pthread_mutex_destory(&(wq->mutex));
	pthread_mutex_destory(&(wq->cond));
}
