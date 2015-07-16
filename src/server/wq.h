#ifndef __WQ__
#define __WQ__

#include <pthread.h>

/* WQ defines a work queue which will be used to store jobs which are waiting to be processed.
 *
 * WQ will contain whatever synchronization primitives are necessary to allow any number of
 * threads to be waiting for items to fill the work queue. For each item added to the queue,
 * exactly one thread should receive the item. When the queue is empty, there should be no
 * busy waiting.
 */

typedef struct wq_item {
   void *item;             /* The item which is being stored. */
   struct wq_item *next;   /* The next item in the queue. */
   struct wq_item *prev;   /* The previous item in the queue. */
} wq_item_t;

typedef struct wq {
  wq_item_t *head;         /* The head of the list of items. */
} wq_t;


void wq_init(wq_t *wq);

void wq_push(wq_t *wq, void *item);

void *wq_pop(wq_t *wq);

#endif
