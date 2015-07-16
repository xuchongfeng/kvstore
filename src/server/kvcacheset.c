#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include "uthash.h"
#include "utlist.h"
#include "kvconstants.h"
#include "kvcacheset.h"

/* Initializes CACHESET to hold a maximum of ELEM_PER_SET elements.
 * ELEM_PER_SET must be at least 2.
 * Returns 0 if successful, else a negative error code. */
int kvcacheset_init(kvcacheset_t *cacheset, unsigned int elem_per_set) {
  int ret;
  if (elem_per_set < 2) return -1;
  cacheset->elem_per_set = elem_per_set;
  if ((ret = pthread_rwlock_init(&cacheset->lock, NULL)) < 0)
    return ret;
  cacheset->num_entries = 0;
  cacheset->entries = (kvcacheset_entry *)malloc(elem_per_set * sizeof(kvcacheset_entry));
  memset(cacheset->entries, 0 , elem_per_set * sizeof(kvcacheset_entry));
  if(cacheset->entries == NULL) return -1;
  cacheset->entry_queue = (int *)malloc(elem_per_set * sizeof(int));
  if(cacheset->entry_queue == NULL) return -1;
  return 0;
}

/* update the last visited data*/
void update_queue(kvcacheset_t *cacheset, int entry_num, bool del){
  int index = 0;
  int elem_per_set = cacheset->elem_per_set;
  while(cacheset->entry_queue[index++] != entry_num && index < elem_per_set) ;
  if(del){ 
    while(index++ < elem_per_set){
      cacheset->entry_queue[index] = cacheset->entry_queue[index+1];
    }
    return;
  }
  while(index-- > 0){
    cacheset->entry_queue[index] = cacheset->entry_queue[index-1];
  }
  cacheset->entry_queue[0] = entry_num;
}

/* get the entry_num when no spare entry space exist */
int get_entry_index(kvcacheset_t *cacheset){
  int index = cacheset->elem_per_set - 1;
  int used_entry = cacheset->entry_queue[index];
  while(index-- > 0){
    cacheset->entry_queue[index] = cacheset->entry_queue[index-1];
  }
  cacheset->entry_queue[0] = used_entry;
  return used_entry;
}

/* Get the entry corresponding to KEY from CACHESET. Returns 0 if successful,
 * else returns a negative error code. If successful, populates VALUE with a
 * malloced string which should later be freed. */
int kvcacheset_get(kvcacheset_t *cacheset, char *key, char **value) {
  int i = 0;
  for(; i < cacheset->num_entries; i++){
    if(strcmp(key, cacheset->entries[i].key) == 0 && cacheset->entries[i].refbit){
      value = &(cacheset->entries[i].value);
      update_queue(cacheset, i, false);
      return 0;
    }
  }
  return -1;
}

/* Add the given KEY, VALUE pair to CACHESET. Returns 0 if successful, else
 * returns a negative error code. Should evict elements if necessary to not
 * exceed CACHESET->elem_per_set total entries. */
int kvcacheset_put(kvcacheset_t *cacheset, char *key, char *value) {
  // exist unused slot
  int index = 0;
  if(cacheset->num_entries < cacheset->elem_per_set){
    int i = 0;
    for(; i < cacheset->elem_per_set; i++){
      if(!cacheset->entries[i].refbit || strcmp(key, cacheset->entries[i].key) == 0){
        index = i;
        break;
      }          
    }
  }
  else{
    index = get_entry_index(cacheset);
  }
  
  int length_key = strlen(key);
  int length_value = strlen(value);
  free(cacheset->entries[index].key);
  free(cacheset->entries[index].value);
  cacheset->entries[index].key = (char *)malloc(length_key + 1);
  cacheset->entries[index].value = (char *)malloc(length_value + 1);
  if(!cacheset->entries[index].key) return -1;
  if(!cacheset->entries[index].value) return -1;
  strncpy(cacheset->entries[index].key, key, (length_key + 1));
  strncpy(cacheset->entries[index].value, value, (length_value + 1));
  cacheset->entries[index].refbit = true;
  cacheset->num_entries++;
  update_queue(cacheset, index, true);
  return 0;
}

/* Deletes the entry corresponding to KEY from CACHESET. Returns 0 if
 * successful, else returns a negative error code. */
int kvcacheset_del(kvcacheset_t *cacheset, char *key) {
  int index = 0;
  for(; index < cacheset->elem_per_set; index++){
    if(strcmp(key, cacheset->entries[index].key) == 0){
      free(cacheset->entries[index].key);
      free(cacheset->entries[index].value);
      cacheset->entries[index].refbit = false;
      update_queue(cacheset, index, true);
    }
  }
  return -1;
}

/* Completely clears this cache set. For testing purposes. */
void kvcacheset_clear(kvcacheset_t *cacheset) {
  int index = 0;
  int elem_per_set = cacheset->elem_per_set;
  for(;index < elem_per_set; index++){
    if(cacheset->entries[index].refbit){
      free(cacheset->entries[index].key);
      free(cacheset->entries[index].value);
    }
  }
  free(cacheset->entries);
}
