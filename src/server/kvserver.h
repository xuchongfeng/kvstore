#ifndef __KV_SERVER__
#define __KV_SERVER__

#include <stdbool.h>
#include "kvcache.h"
#include "kvstore.h"
#include "kvmessage.h"
#include "tpclog.h"

/* KVServer defines a server which will be used to store <key, value> pairs.
 *
 * Ideally, each KVServer would be running on its own machine with its own file
 * storage.
 *
 * A KVServer accepts incoming messages on a socket using the message format
 * described in the spec, and responds accordingly on the same socket. There is
 * one generic entrypoint, kvserver_handle, which takes in a socket that has
 * already been connected to a master or client and handles all further
 * communication.
 *
 * A KVServer has an associated KVStore and KVCache. The server should attempt
 * to get an entry from cache before accessing its store to eliminate the need
 * to access disk when possible. The cache should write-through; that is, when
 * a new entry is stored, it should be written to both the cache and the store
 * immediately.
 *
 * A KVServer can operate in two modes; TPC or non-TPC. In non-TPC mode, all
 * PUT and DEL requests go immediately to the cache/store. In TPC mode, 2-Phase
 * Commit logic is used, described further in the spec.
 *
 * Because the KVStore stores all data in persistent file storage, a non-TPC
 * KVServer can be reinitialized using a DIRNAME which contains a previous
 * KVServer and all old entries will be available, enabling easy crash
 * recovery.
 *
 * A TPC KVServer maintains state beyond the current KVStore entries, so a
 * TPCLog is used to log incoming requests and can be used to recreate the
 * state of the server upon crash recovery.
 */
struct kvserver;
typedef void (*kvhandle_t)(struct kvserver *, int sockfd, void *extra);

/* A KVServer. Stores the associated KVCache and KVStore, as well as whether or
 * not this is a TPC-enabled server. */
typedef struct kvserver {
  kvcache_t cache;          /* The cache this server will use. */
  kvstore_t store;          /* The store this server will use. */
  tpclog_t log;             /* The log this server will use (checkpoint 2 only). */
  bool use_tpc;             /* 1 if this server should expect TPC operations, else 0. */
  int max_threads;          /* The max threads this server will run on. */
  kvhandle_t handle;        /* The function this server will use to handle requests. */
  int listening;            /* 1 if this server is currently listening for requests, else 0. */
  int sockfd;               /* The socket fd this server is currently listening on (if any). */
  int port;                 /* The port this server should listen on. */
  char *hostname;           /* The host this server should listen on. */
} kvserver_t;

int kvserver_init(kvserver_t *, char *dirname, unsigned int num_sets,
    unsigned int elem_per_set, unsigned int max_threads, const char *hostname,
    int port, bool use_tpc);

int kvserver_register_master(kvserver_t *, int sockfd);

void kvserver_handle(kvserver_t *, int sockfd, void *extra);

void kvserver_handle_tpc(kvserver_t *, kvmessage_t *reqmsg,
    kvmessage_t *respmsg);
void kvserver_handle_no_tpc(kvserver_t *, kvmessage_t *reqmsg,
    kvmessage_t *respmsg);

int kvserver_get(kvserver_t *, char *key, char **value);
int kvserver_put(kvserver_t *, char *key, char *value);
int kvserver_del(kvserver_t *, char *key);

int kvserver_rebuild_state(kvserver_t *);

int kvserver_clean(kvserver_t *);

#endif
