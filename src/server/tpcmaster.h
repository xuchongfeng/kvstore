#ifndef __KV_MASTER__
#define __KV_MASTER__

#include <pthread.h>
#include "kvcache.h"

/* TPCMaster defines a master server which will communicate with multiple
 * slave servers.
 *
 * The TPCMaster behaves like a KVServer from the client's perspective, but
 * actually handles requests quite differently. For every PUT and DEL request
 * it receives, the TPCMaster polls the slaves relevant to the key in question,
 * asking for a VOTE_COMMIT or a VOTE_ABORT. If a consesus to commit is
 * reached, the TPCMaster notifies the slaves to COMMIT, else it commands to
 * ABORT.
 *
 * The TPCMaster will need to listen for registration requests from KVServers
 * acting as its slaves, before it can handle any client request.
 *
 * The TPCMaster has an associated KVCache, which should be updated on PUT
 * and DEL requests, and accessed on GET requests before going to the slaves.
 *
 * For this project, you can assume that the TPCMaster will never fail. Thus,
 * you don't need to maintain a TPCLog for it.
 * 
 * Checkpoint 2 only.
 */

typedef void (*callback_t)(void*);

/* A struct used to represent the slaves which this TPC Master is aware of. */
typedef struct tpcslave {
  int64_t id;                   /* The unique ID for this slave. */
  char *host;                   /* The host where this slave can be reached. */
  unsigned int port;            /* The port where this slave can be reached. */
  struct tpcslave *next;        /* The next slave in the list of slaves. */
  struct tpcslave *prev;        /* The previous slave in the list of slaves. */
} tpcslave_t;

struct tpcmaster;

typedef void (*tpchandle_t)(struct tpcmaster *, int sockfd, callback_t callback);

/* A TPC Master. */
typedef struct tpcmaster {
  unsigned int slave_capacity;  /* The number of slaves this master will use. */
  unsigned int slave_count;     /* The current number of slaves this master is aware of. */
  unsigned int redundancy;      /* The number of slaves a single value will be stored on. */
  tpcslave_t *slaves_head;      /* The head of the list of slaves. */
  pthread_rwlock_t slave_lock;  /* A lock used to protect the list of slaves. */
  kvcache_t cache;              /* The cache this master will use. */
  tpchandle_t handle;           /* The function this master will use to handle requests. */
} tpcmaster_t;

int tpcmaster_init(tpcmaster_t *master, unsigned int slave_capacity,
    unsigned int redundancy, unsigned int num_sets, unsigned int elem_per_set);

void tpcmaster_register(tpcmaster_t *master, kvmessage_t *reqmsg,
    kvmessage_t *respmsg);
tpcslave_t *tpcmaster_get_primary(tpcmaster_t *master, char *key);
tpcslave_t *tpcmaster_get_successor(tpcmaster_t *master,
    tpcslave_t *predecessor);

void tpcmaster_handle(tpcmaster_t *master, int sockfd, callback_t callback);

void tpcmaster_handle_get(tpcmaster_t *master, kvmessage_t *reqmsg,
    kvmessage_t *respmsg);
void tpcmaster_handle_tpc(tpcmaster_t *master, kvmessage_t *reqmsg,
    kvmessage_t *respmsg, callback_t callback);

void tpcmaster_info(tpcmaster_t *master, kvmessage_t *reqmsg,
    kvmessage_t *respmsg);

void tpcmaster_clear_cache(tpcmaster_t *tpcmaster);

#endif
