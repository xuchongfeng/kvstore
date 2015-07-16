#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include "socket_server.h"
#include "kvserver.h"

const char *USAGE = "Usage: kvslave "
    "[-t] [--tpc] "
    "[slave_port (default=9000)] "
    "[master_port (default=8888)]";

int main(int argc, char **argv) {
  int tpc_mode = 0,
      slave_port = 9000,
      master_port = 8888;
  char *mode = "";
  char *slave_hostname = "localhost", *master_hostname = "localhost";
  int index = 0;
  int opt_ind;
  int c;
  struct option long_options[] = {{"tpc", no_argument, &tpc_mode, 1},
      {0,0,0,0}};
  if ((c = getopt_long (argc, argv, "t", long_options, &opt_ind)) != -1) {
    switch (c) {
      case 0:
        break;
      case 't':
        tpc_mode = 1;
        mode = "(tpc)";
        index += 1;
        break;
      default:
        goto usage;
    }
  }
  if (index < argc) {
    switch (argc - index - 1) {
      case 1:
        index += 1;
        if (argv[index][0] != '-') {
          slave_port = atoi(argv[index]);
          break;
        } else {
          goto usage;
        }
      case 2:
        index += 1;
        if (argv[index][0] != '-') {
          slave_port = atoi(argv[index]);
        } else {
          goto usage;
        }

        if (argv[index + 1][0] != '-') {
          master_port = atoi(argv[index + 1]);
        } else {
          goto usage;
        }
        break;
    }
  }

  if (tpc_mode) {
    printf("Slave server %s started on %d listening for master at "
        "%s:%d... \n", mode, slave_port, master_hostname, master_port);
  } else {
    printf("Single Node server started on port %d...\n", slave_port);
  }

  kvserver_t slave;
  server_t server;
  server.master = 0;
  server.max_threads = 3;

  char slave_name[20];
  sprintf(slave_name, "slave-port%d", slave_port);

  kvserver_init(&slave, slave_name, 4, 4, 2, slave_hostname, slave_port,
      tpc_mode);
  if (tpc_mode) {
    /* Need to send registration to the master.*/
    int ret, sockfd = connect_to(master_hostname, master_port, 0);
    if (sockfd < 0) {
      printf("Error registering slave! "
          "Could not connect to master on host %s at port %d\n",
          master_hostname, master_port);
      return 1;
    }
    ret = kvserver_register_master(&slave, sockfd);
    if (ret < 0) {
      printf("Error registering slave with master! "
          "Received an error message back from master.\n");
      return 1;
    }
    close(sockfd);
  }
  server.kvserver = slave;
  server_run(slave_hostname, slave_port, &server, NULL);
  return 0;

usage:
  printf("%s\n", USAGE);
  return 1;
}
