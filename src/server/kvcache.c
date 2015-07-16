#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "kvconstants.h"
#include "kvcache.h"
#include "kvstore.h"

/* Initializes KVCache CACHE. The cache will contains NUM_SETS KVCacheSets,
 * each containing up to ELEM_PER_SET entries. Returns 0 if successful, else a
 * negative error code. */
int kvcache_init(kvcache_t *cache, unsigned int num_sets,
    unsigned int elem_per_set) {
  int i;
  if (num_sets == 0 || elem_per_set == 0)
    return -1;
  cache->sets = malloc(num_sets * sizeof(kvcacheset_t));
  if (cache->sets == NULL)
    return ENOMEM;
  cache->num_sets = num_sets;
  cache->elem_per_set = elem_per_set;
  for (i = 0; i < num_sets; ++i) {
    if (kvcacheset_init(&cache->sets[i], elem_per_set) != 0)
      return -1;
  }
  return 0;
}

/* Retrieves the cache set associated with a given KEY. The correct set can be
 * determined based on the hash of the KEY using the hash() function defined
 * within kvstore.h. */
kvcacheset_t *get_cache_set(kvcache_t *cache, char *key) {
  return &cache->sets[0];
}

/* Attempts to retrieve KEY from CACHE. If successful, returns 0 and stores the
 * associated value inside VALUE using malloc()d memory which should be free()d
 * later. Otherwise, returns a negative error code. */
int kvcache_get(kvcache_t *cache, char *key, char **value) {
  if (strlen(key) > MAX_KEYLEN)
    return ERRKEYLEN;
  return kvcacheset_get(get_cache_set(cache, key), key, value);
}

/* Attempts to place the given KEY, VALUE entry into CACHE. Returns 0 if
 * successful, else a negative error code. */
int kvcache_put(kvcache_t *cache, char *key, char *value) {
  if (strlen(key) > MAX_KEYLEN)
    return ERRKEYLEN;
  if (strlen(value) > MAX_VALLEN)
    return ERRVALLEN;
  return kvcacheset_put(get_cache_set(cache, key), key, value);
}

/* Attempts to delete the given KEY from CACHE. Returns 0 if successful, else a
 * negative error code. */
int kvcache_del(kvcache_t *cache, char *key) {
  if (strlen(key) > MAX_KEYLEN)
    return ERRKEYLEN;
  return kvcacheset_del(get_cache_set(cache, key), key);
}

/* Returns the read-write lock associated with a given KEY within CACHE. Each
 * cache set has a separate lock. */
pthread_rwlock_t *kvcache_getlock(kvcache_t *cache, char *key) {
  if (strlen(key) > MAX_KEYLEN)
    return NULL;
  return &get_cache_set(cache, key)->lock;
}

/* Completely clears this cache. For testing purposes. */
void kvcache_clear(kvcache_t *cache) {
  for (int i = 0; i < cache->num_sets; i++)
    kvcacheset_clear(&cache->sets[i]);
}
