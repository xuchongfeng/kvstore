#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "uthash.h"
#include "utlist.h"
#include "kvconstants.h"
#include "kvcacheset.h"

/* enum operations.
 * update.
 * insert.
 * delete.*/
enum operations{
	update,
	insert,
	delete,
};

/* Initializes CACHESET to hold a maximum of ELEM_PER_SET elements.
 * ELEM_PER_SET must be at least 2.
 * Returns 0 if successful, else a negative error code. */
int kvcacheset_init(kvcacheset_t *cacheset, unsigned int elem_per_set) {
  int ret;
  if (elem_per_set < 2) return -1;
  cacheset->elem_per_set = elem_per_set;
  if ((ret = pthread_rwlock_init(&(cacheset->lock), NULL)) < 0)
    return ret;
  if ((ret = pthread_mutex_init(&(cacheset->mutex), NULL)) < 0)
	return ret;
  cacheset->num_entries = 0;
  cacheset->entries = (kvcacheset_entry *)malloc(elem_per_set * sizeof(kvcacheset_entry));
  if(cacheset->entries == NULL) return ENOMEM;
  memset(cacheset->entries, 0 , elem_per_set * sizeof(kvcacheset_entry));
  cacheset->entry_queue = (int *)malloc(elem_per_set * sizeof(int));
  if(cacheset->entry_queue == NULL) return ENOMEM;
  return 0;
}

/* update the last visited data*/
void update_queue(kvcacheset_t *cacheset, int entry_num, int op){
	pthread_mutex_lock(&(cacheset->mutex));
	if(op == update){
		int index;
		for(index=0; index<cacheset->num_entries; index++){
			if(cacheset->entry_queue[index] == entry_num){
				break;
			}
		}
		for(int i=index; i>0; i--){
			cacheset->entry_queue[i] = cacheset->entry_queue[i-1];
		}
		cacheset->entry_queue[0] = entry_num;
	}
	else if(op == insert){
		int entries = cacheset->num_entries;
        for(int i=entries; i>0; i--){
          cacheset->entry_queue[i] = cacheset->entry_queue[i-1];
        }
		cacheset->entry_queue[0] = entry_num;
	}
	else if(op == delete){
		int index;
		for(index=0; index<cacheset->num_entries; index++){
			if(cacheset->entry_queue[index] == entry_num){
				break;
			}
		}
		for(int i=index; i<cacheset->num_entries; i++){
            if(i == (cacheset->elem_per_set-1)){
                break;
            }
			cacheset->entry_queue[i] = cacheset->entry_queue[i+1];
		}
	}
	pthread_mutex_unlock(&(cacheset->mutex));
}

/* get the entry_num when no spare entry space exist */
int get_entry_index(kvcacheset_t *cacheset){
	int index = cacheset->num_entries - 1;
	return cacheset->entry_queue[index];
}

/* Get the entry corresponding to KEY from CACHESET. Returns 0 if successful,
 * else returns a negative error code. If successful, populates VALUE with a
 * malloced string which should later be freed. */
int kvcacheset_get(kvcacheset_t *cacheset, char *key, char **value) {
  int i = 0;
  for(; i < cacheset->elem_per_set; i++){
    if(cacheset->entries[i].refbit && strcmp(key, cacheset->entries[i].key) == 0){
	  pthread_rwlock_rdlock(&(cacheset->lock));
      *value = cacheset->entries[i].value;
      update_queue(cacheset, i, update);
	  pthread_rwlock_unlock(&(cacheset->lock));
      return 0;
    }
  }
  return ERRNOKEY;
}

/* Add the given KEY, VALUE pair to CACHESET. Returns 0 if successful, else
 * returns a negative error code. Should evict elements if necessary to not
 * exceed CACHESET->elem_per_set total entries. */
int kvcacheset_put(kvcacheset_t *cacheset, char *key, char *value) {
  // check key exist or not
  for(int i=0; i<cacheset->num_entries; i++){
	  if(cacheset->entries[i].refbit && strcmp(cacheset->entries[i].key, key) == 0){
		  pthread_rwlock_wrlock(&(cacheset->lock));
		  free(cacheset->entries[i].value);
		  int length = strlen(value) + 1;
		  cacheset->entries[i].value = (char *)malloc(length);
		  if(cacheset->entries[i].value == NULL){
              pthread_rwlock_unlock(&(cacheset->lock));
			  return ENOMEM;
		  }
		  strncpy(cacheset->entries[i].value, value, length);
 		  update_queue(cacheset, i, update); 
		  pthread_rwlock_unlock(&(cacheset->lock));
		  return 0;
	  }
  }

  // key not exist
  // exist ununsed slot or not
  pthread_rwlock_wrlock(&(cacheset->lock));
  int index, operation;
  if(cacheset->num_entries < cacheset->elem_per_set){
	  for(index=0; index<cacheset->elem_per_set; index++){
		  if(!cacheset->entries[index].refbit){
			  operation = insert;
			  break;
		  }
	  }
  }
  else{
	  index = get_entry_index(cacheset);
	  operation = update;
  }
  free(cacheset->entries[index].key);
  free(cacheset->entries[index].value);
  int length_key = strlen(key) + 1;
  int length_value = strlen(value) + 1;
  cacheset->entries[index].key = (char *)malloc(length_key);
  if(cacheset->entries[index].key == NULL){
      pthread_rwlock_unlock(&(cacheset->lock));
      return ENOMEM;
  }
  cacheset->entries[index].value = (char *)malloc(length_value);
  if(cacheset->entries[index].value == NULL){
      pthread_rwlock_unlock(&(cacheset->lock));
      return ENOMEM;
  }
  strncpy(cacheset->entries[index].key, key, length_key);
  strncpy(cacheset->entries[index].value, value, length_value);
  cacheset->entries[index].refbit = true;
  update_queue(cacheset, index, operation);
  if(operation == insert){
	  cacheset->num_entries += 1;
  }
  pthread_rwlock_unlock(&(cacheset->lock));
  return 0;
}

/* Deletes the entry corresponding to KEY from CACHESET. Returns 0 if
 * successful, else returns a negative error code. */
int kvcacheset_del(kvcacheset_t *cacheset, char *key) {
  int index = 0;
  for(; index < cacheset->elem_per_set; index++){
    if(strcmp(key, cacheset->entries[index].key) == 0 && cacheset->entries[index].refbit){
	  pthread_rwlock_wrlock(&(cacheset->lock));
      free(cacheset->entries[index].key);
      free(cacheset->entries[index].value);
      cacheset->entries[index].refbit = false;
      update_queue(cacheset, index, delete);
	  cacheset->num_entries -= 1;
	  pthread_rwlock_unlock(&(cacheset->lock));
	  return 0;
    }
  }
  return ERRNOKEY;
}

/* Completely clears this cache set. For testing purposes. */
void kvcacheset_clear(kvcacheset_t *cacheset) {
  int index = 0;
  int elem_per_set = cacheset->elem_per_set;
  for(;index < elem_per_set; index++){
    if(cacheset->entries[index].refbit){
      free(cacheset->entries[index].key);
      free(cacheset->entries[index].value);
      cacheset->entries[index].refbit = false;
    }
  }
}
