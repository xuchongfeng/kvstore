#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include "socket_server.h"
#include "kvserver.h"

const char *USAGE = "Usage: kvmaster [port (default=8888)]";

int main(int argc, char** argv) {
  int port = 8888;
  server_t server;

  if (argc > 1) {
    if (argc > 2) {
      printf("%s\n", USAGE);
      return 1;
    }
    port = atoi(argv[1]);
  }
  server.master = 1;
  server.max_threads = 3;
  tpcmaster_init(&server.tpcmaster, 2, 2, 4, 4);
  printf("TPC Master server started listening on port %d...\n", port);
  server_run("localhost", port, &server, NULL);
}
