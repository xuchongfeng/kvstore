#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "kvserver.h"
#include "kvconstants.h"
#include "socket_server.h"
#include "wq.h"

#define TIMEOUT 100

/* Handles requests under the assumption that SERVER is a TPC Master. */
void handle_master(server_t *server) {
  int sockfd;
  tpcmaster_t *tpcmaster = &server->tpcmaster;
  sockfd = (intptr_t) wq_pop(&server->wq);
  tpcmaster->handle(tpcmaster, sockfd, NULL);
}

/* Handles requests under the assumption that SERVER is a kvserver slave. */
void handle_slave(server_t *server) {
  int sockfd;
  kvserver_t *kvserver = &server->kvserver;
  sockfd = (intptr_t) wq_pop(&server->wq);
  kvserver->handle(kvserver, sockfd, NULL);
}

/* Handles requests for _SERVER. */
void *handle(void *_server) {
  server_t *server = (server_t *) _server;
  if (server->master) {
    handle_master(server);
  } else {
    handle_slave(server);
  }
  return NULL;
}

/* Connects to the host given at HOST:PORT using a TIMEOUT second timeout.
 * Returns a socket fd which should be closed, else -1 if unsuccessful. */
int connect_to(const char *host, int port, int timeout) {
  struct sockaddr_in addr;
  struct hostent *ent;
  int sockfd;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  ent = gethostbyname(host);
  if (ent == NULL) {
    return -1;
  }
  bzero((char *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  bcopy((char *)ent->h_addr, (char *)&addr.sin_addr.s_addr, ent->h_length);
  addr.sin_port = htons(port);
  if (timeout > 0) {
    struct timeval t;
    t.tv_sec = timeout;
    t.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &t, sizeof(t));
  }
  if (connect(sockfd,(struct sockaddr *) &addr, sizeof(addr)) < 0) {
    return -1;
  }
  return sockfd;
}

/* Runs SERVER such that it indefinitely (until server_stop is called) listens
 * for incoming requests at HOSTNAME:PORT. If CALLBACK is not NULL, makes a
 * call to CALLBACK with NULL as its parameter once SERVER is actively
 * listening for requests (this is for testing purposes).
 *
 * As given, this function will synchronously handle only a single request
 * at a time. It is your task to modify it such that it can handle up to
 * SERVER->max_threads jobs at a time asynchronously. */
int server_run(const char *hostname, int port, server_t *server,
    callback_t callback) {
  int sock_fd, client_sock, socket_option;
  struct sockaddr_in client_address;
  size_t client_address_length = sizeof(client_address);
  wq_init(&server->wq);
  server->listening = 1;
  server->port = port;
  server->hostname = (char *) malloc(strlen(hostname) + 1);
  strcpy(server->hostname, hostname);

  sock_fd = socket(PF_INET, SOCK_STREAM, 0);
  server->sockfd = sock_fd;
  if (sock_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno,
        strerror(errno));
    exit(errno);
  }
  socket_option = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
      sizeof(socket_option)) == -1) {
    fprintf(stderr, "Failed to set socket options: error %d: %s\n", errno,
        strerror(errno));
    exit(errno);
  }
  memset(&client_address, 0, sizeof(client_address));
  client_address.sin_family = AF_INET;
  client_address.sin_addr.s_addr = INADDR_ANY;
  client_address.sin_port = htons(port);

  if (bind(sock_fd, (struct sockaddr *) &client_address,
      sizeof(client_address)) == -1) {
    fprintf(stderr, "Failed to bind on socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  if (listen(sock_fd, 1024) == -1) {
    fprintf(stderr, "Failed to listen on socket: error %d: %s\n", errno,
        strerror(errno));
    exit(errno);
  }

  if (callback != NULL){
    callback(NULL);
  }


  while (server->listening) {
    client_sock = accept(sock_fd, (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_sock > 0) {
      wq_push(&server->wq, (void *) (intptr_t) client_sock);
      handle(server);
    }
  }
  shutdown(sock_fd, SHUT_RDWR);
  close(sock_fd);
  return 0;
}

/* Stops SERVER from continuing to listen for incoming requests. */
void server_stop(server_t *server) {
  server->listening = 0;
  shutdown(server->sockfd, SHUT_RDWR);
  close(server->sockfd);
}
