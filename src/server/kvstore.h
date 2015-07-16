#ifndef __KV_STORE__
#define __KV_STORE__

#include <stdbool.h>
#include <pthread.h>
#include "kvconstants.h"

/* KVStore defines the persistent storage used by a server to store <key, value> entries.
 *
 * Each entry is stored as an individual file, all collected within the
 * directory name which is passed in upon initialization. If you are running
 * multiple KVStores, they MUST have unique directory names, else behavior is
 * undefined.
 *
 * The files which store entries are simple binary dumps of a kventry_t
 * struct.  Note that this means entry files are NOT portable, and results will
 * vary if an entry created on one machine is accessed on another machine, or
 * even by a program compiled by a different compiler. The LENGTH field of kventry_t
 * is used to determine how large an entry and its associated file are.
 *
 * The name of the file that stores an entry is determined by the djb2 string
 * hash of the entry's key, which can be found using the hash() function. To
 * resolve collisions, hash chaining is used, thus the file names of entries
 * within the store directory should have the format:
 *    hash(key)-chainpos.entry
 *        OR, more explicitly:
 *    sprintf(filename, "%lu-%u.entry", hash(key), chainpos);
 * chainpos represents the entry's position within its hash chain, which should
 * start from 0.  If a collision is found when storing an entry, the new entry
 * will have a chainpos of 1, and so on.  Chains should always be complete;
 * that is, you may never have a chain which has entries with a chainpos of 0
 * and 2 but not 1.
 *
 * All state is stored in persistent file storage, so it is valid to initialize
 * a KVStore using a directory name which was previously used for a KVStore,
 * and the new store will be an exact clone of the old store.
 */

/* The filetype to append to the filenames of entries within the log. */
#define KVSTORE_FILETYPE ".entry"

/* A KVStore. */
typedef struct {
  char dirname[MAX_FILENAME];  /* The name of the directory used to store its entries. */
  pthread_rwlock_t lock;       /* The lock used to make KVStore's functions thread-safe. */
} kvstore_t;

/* A single kvstore entry.
 * data stores both the key and the value, in the form:
 *   key_string \0 value_string \0
 * (that is, two concatenated and null terminated strings) */
typedef struct {
  int length;                   /* Stores the total length of data, including null terminators. */
  char data[0];                 /* Described above. */
} kventry_t;

unsigned long hash(char *str);

int kvstore_init(kvstore_t *, char *dirname);

int kvstore_get(kvstore_t *, char *key, char **value);

int kvstore_put(kvstore_t *, char *key, char *value);
int kvstore_put_check(kvstore_t *, char *key, char *value);

int kvstore_del(kvstore_t *, char *key);
int kvstore_del_check(kvstore_t *, char *key);

bool kvstore_haskey(kvstore_t *, char *key);

int kvstore_clean(kvstore_t *);

#endif
