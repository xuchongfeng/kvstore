#ifndef __SOCKETSERVER__
#define __SOCKETSERVER__

#include "kvserver.h"
#include "tpcmaster.h"
#include "wq.h"

/* Socket Server defines helper functions for communicating over sockets.
 *
 * connect_to can be used to make a request to a listening host. You will not
 * need to modify this, but you will likely want to utilize it.
 *
 * server_run can be used to start a server (containing a TPCMaster or KVServer)
 * listening on a given port. See the comment above server_run for more information.
 *
 * The server struct stores extra information on top of the stored TPCMaster or
 * KVServer.
 */

void *handle(void *_kvserver);

typedef struct server {
  int master;               /* 1 if this server represents a TPC Master, else 0. */
  int listening;            /* 1 if this server is currently listening, else 0. */
  int sockfd;               /* The socket fd this server is operating on. */
  int max_threads;          /* The maximum number of concurrent jobs that can run. */
  int port;                 /* The port this server will listen on. */
  char *hostname;           /* The hostname this server will listen on. */
  wq_t wq;                  /* The work queue this server will use to process jobs. */
  union {                   /* The kvserver OR tpcmaster this server represents. */
    kvserver_t kvserver;
    tpcmaster_t tpcmaster;
  };
} server_t;

int connect_to(const char *host, int port, int timeout);
int server_run(const char *hostname, int port, server_t *server,
    callback_t callback);
void server_stop(server_t *server);

#endif
