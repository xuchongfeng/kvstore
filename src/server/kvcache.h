#ifndef __KV_CACHE__
#define __KV_CACHE__

#include <pthread.h>
#include "kvcacheset.h"

/* KVCache defines the in-memory cache which is used by KVServers to quickly
 * access data without going to disk. It is set-associative.
 *
 * A KVCache maintains a list of KVCacheSets, each of which represents a subset
 * of entries in the cache. Each KVCacheSet maintains separate data structures;
 * thus, entries in different cache sets can be accessed/modified concurrently.
 * However, entries in the same cache set must be modified sequentially. This
 * is achieved using a read-write lock maintained by each cache set. The lock 
 * should be acquired/released from whoever will be calling the cache methods 
 * (i.e. KVServer and, later, TPCMaster).
 *
 * The cache uses a second-chance replacement policy implemented within each
 * cache set.  You can think of this as a FIFO queue, where the entry that has
 * been in the cache the longest is evicted, except that it will receive a
 * second chance if it has been accessed while it has been in the cache. Each
 * entry should maintain a reference bit, initially set to false.  When an
 * entry is accessed (GET or PUT), its reference bit is set to true.  When an
 * entry needs to be evicted, iterate through the list of entries starting at
 * the front of the queue. Once an entry with a reference bit of false is
 * reached, evict that entry.  If an entry with a reference bit of true is
 * seen, set its reference bit to false, and move it to the back of the queue.
 */

/* A KVCache. */
typedef struct {
  unsigned int num_sets;        /* The number of sets within this cache. */
  unsigned int elem_per_set;    /* The max number of elements that can be stored within each set. */
  kvcacheset_t *sets;           /* An array of all of the sets used in this cache. */
} kvcache_t;

int kvcache_init(kvcache_t *, unsigned int num_sets, unsigned int elem_per_set);

int kvcache_get(kvcache_t *, char *key, char **value);
int kvcache_put(kvcache_t *, char *key, char *value);
int kvcache_del(kvcache_t *, char *key);

pthread_rwlock_t *kvcache_getlock(kvcache_t *, char *key);

void kvcache_clear(kvcache_t *);

#endif
