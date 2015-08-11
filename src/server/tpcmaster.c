#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include "kvconstants.h"
#include "kvmessage.h"
#include "socket_server.h"
#include "time.h"
#include "tpcmaster.h"

#define TIME_OUT 1

/* Initializes a tpcmaster. Will return 0 if successful, or a negative error
 * code if not. SLAVE_CAPACITY indicates the maximum number of slaves that
 * the master will support. REDUNDANCY is the number of replicas (slaves) that
 * each key will be stored in. The master's cache will have NUM_SETS cache sets,
 * each with ELEM_PER_SET elements. */
int tpcmaster_init(tpcmaster_t *master, unsigned int slave_capacity,
    unsigned int redundancy, unsigned int num_sets, unsigned int elem_per_set) {
  int ret;
  ret = kvcache_init(&master->cache, num_sets, elem_per_set);
  if (ret < 0) return ret;
  ret = pthread_rwlock_init(&master->slave_lock, NULL);
  if (ret < 0) return ret;
  master->slave_count = 0;
  master->slave_capacity = slave_capacity;
  if (redundancy > slave_capacity) {
    master->redundancy = slave_capacity;
  } else {
    master->redundancy = redundancy;
  }
  master->slaves_head = NULL;
  master->handle = tpcmaster_handle;
  return 0;
}

/* Converts Strings to 64-bit longs. Borrowed from http://goo.gl/le1o0W,
 * adapted from the Java builtin String.hashcode().
 * DO NOT CHANGE THIS FUNCTION. */
int64_t hash_64_bit(char *s) {
  int64_t h = 1125899906842597LL;
  int i;
  for (i = 0; s[i] != 0; i++) {
    h = (31 * h) + s[i];
  }
  return h;
}

/* Init slave 
 * with hostname and port from reqmsg
 */
tpcslave_t* init_slave(char* hostname, char* port)
{
	tpcslave_t* slave = (tpcslave_t *)malloc(sizeof(tpcslave_t));
	if(slave == NULL) return NULL;
	slave->host = (char *)malloc((strlen(hostname) + 1));
	strcpy(slave->host, hostname);
	if(slave->host == NULL) return NULL;
	slave->port = atoi(port);
	char* port_host = (char *)malloc(strlen(hostname) + strlen(port) + 2);
	if(port_host == NULL) return NULL;
	sprintf(port_host, "%s:%s", port, hostname);
	slave->is = hash_64_bit(port_host);
	return slave;
}

/* Cmp function
 * sort slaves according to the UID
 */
int cmp(tpcslave_t *slave1, tpcslave_t *slave2)
{
	if(slave1->id < slave2->id) return 1;
	else if(slave1->id == slave2->id) return 0;
	else return -1;
}

/* Handles an incoming kvmessage REQMSG, and populates the appropriate fields
 * of RESPMSG as a response. RESPMSG and REQMSG both must point to valid
 * kvmessage_t structs. Assigns an ID to the slave by hashing a string in the
 * format PORT:HOSTNAME, then tries to add its info to the MASTER's list of
 * slaves. If the slave is already in the list, do nothing (success).
 * There can never be more slaves than the MASTER's slave_capacity. RESPMSG
 * will have MSG_SUCCESS if registration succeeds, or an error otherwise.
 *
 * Checkpoint 2 only. */
void tpcmaster_register(tpcmaster_t *master, kvmessage_t *reqmsg,
    kvmessage_t *respmsg) {
  tpcslave_t* slave = init_slave(reqmsg->key, reqmsg->value);
  if(slave == NULL){
	  respmsg->message = ERRMSG_GENERIC_ERROR;
	  return;
  }
  respmsg->message = MSG_SUCCESS;
  // check slave exist or not, if exist return
  tpcslave_t* tmp = master->slaves_head;
  while(tmp){
	  if(tmp->id == slave->id) return;
	  tmp = tmp->next;
  }
  DL_APPEND(master->slaves_head, slave);
  DL_SORT(master->slaves_head, cmp);
}

/* Hashes KEY and finds the first slave that should contain it.
 * It should return the first slave whose ID is greater than the
 * KEY's hash, and the one with lowest ID if none matches the
 * requirement.
 *
 * Checkpoint 2 only. */
tpcslave_t *tpcmaster_get_primary(tpcmaster_t *master, char *key) {
  int64_t key_hash = hash_64_bit(key);
  tpcslave_t *slave = master->head;
  while(slave && slave->id < key_hash){
	  slave = slave->next;
  }
  return slave == NULL ? master->head : slave;
}

/* Returns the slave whose ID comes after PREDECESSOR's, sorted
 * in increasing order.
 *
 * Checkpoint 2 only. */
tpcslave_t *tpcmaster_get_successor(tpcmaster_t *master,
    tpcslave_t *predecessor) {
  return predecessor->next == NULL ? master->slaves_head : predecessor->next;
}

/* Handles an incoming GET request REQMSG, and populates the appropriate fields
 * of RESPMSG as a response. RESPMSG and REQMSG both must point to valid
 * kvmessage_t structs.
 *
 * Checkpoint 2 only. */
void tpcmaster_handle_get(tpcmaster_t *master, kvmessage_t *reqmsg,
    kvmessage_t *respmsg) {
  char *key = reqmsg->key;
  char *value;
  // get from master's cache
  int ret = kvcache_get(master->cache, reqmsg->key, &value); 
  respmsg->message = MSG_SUCCESS;
  if(ret < 0){
	  // get from primary slave
	  tpcslave_t* primary = tpcmaster_get_primary(master, key);
	  int sock_fd = connect_to(primary->hostname, primary->port, TIME_OUT);
	  kvmessage_send(reqmsg, sock_fd);
	  respmsg = kvmessage_parse(sock_fd);

	  if( strcmp(respmsg->message, MSG_SUCCESS) != 0 ) {
		  // get from successor slave
		  tpcslave_t *successor = tpcmaster_get_successor(master, primary);
		  sock_fd = connect_to(primary->hostname, primary->port, TIME_OUT);
		  kvmessage_send(reqmsg, sock_fd);
		  respmsg = kvmessage_parse(sock_fd);
	  }
  }
}

/* Handles an incoming TPC request REQMSG, and populates the appropriate fields
 * of RESPMSG as a response. RESPMSG and REQMSG both must point to valid
 * kvmessage_t structs. Implements the TPC algorithm, polling all the slaves
 * for a vote first and sending a COMMIT or ABORT message in the second phase.
 * Must wait for an ACK from every slave after sending the second phase messages. 
 * 
 * The CALLBACK field is used for testing purposes. You MUST include the following
 * calls to the CALLBACK function whenever CALLBACK is not null, or you will fail
 * some of the tests:
 * - During both phases of contacting slaves, whenever a slave cannot be reached (i.e. you
 *   attempt to connect and receive a socket fd of -1), call CALLBACK(slave), where
 *   slave is a pointer to the tpcslave you are attempting to contact.
 * - Between the two phases, call CALLBACK(NULL) to indicate that you are transitioning
 *   between the two phases.  
 * 
 * Checkpoint 2 only. */
void tpcmaster_handle_tpc(tpcmaster_t *master, kvmessage_t *reqmsg,
    kvmessage_t *respmsg, callback_t callback) {
  char *key = reqmsg->key;
  // pharse 1, ask associated slave commit or abort
  tpcslave_t *primary = tpcmaster_get_primary(master, key);
  int sock_primary = connect_to(primary->hostname, primary->port, TIME_OUT);
  kvmessage_send(reqmsg);
  respmsg = kvmessage_parse(sock_primary);
  if(respmsg->type != VOTE_COMMIT){
	  // pharse 2, abort
	  reqmsg->type = ABORT;
	  kvmessage_send(reqmsg, sock_primary);
	  respmsg->message = ERRMSG_GENERIC_MESSAGE;
	  return;
  }
  tpcslave_t *successor = tpcmaster_get_successor(master, primary);
  int sock_successor = connect_to(successor->hostname, successor->port, TIME_OUT);
  kvmessage_send(reqmsg, sock_successor);
  respmsg = kvmessage_parge(sock_primary);
  if(respmsg->type != VOTE_COMMIT){
	  // pharse 2, abort
	  reqmsg->type = ABORT;
	  kvmessage_send(reqmsg, sock_primary);
	  kvmessage_send(reqmsg, sock_successor);
	  respmsg->message = ERRMSG_GENERIC_MESSAGE;
	  return;
  }
  // pharse 2, commit
  reqmsg->type = COMMIT;
  kvmessage_send(reqmsg, sock_primary);
  kvmessage_send(reqmsg, sock_successor);
  respmsg->message = MSG_SUCCESS;
}

/* Handles an incoming kvmessage REQMSG, and populates the appropriate fields
 * of RESPMSG as a response. RESPMSG and REQMSG both must point to valid
 * kvmessage_t structs. Provides information about the slaves that are
 * currently alive.
 *
 * Checkpoint 2 only. */
void tpcmaster_info(tpcmaster_t *master, kvmessage_t *reqmsg,
    kvmessage_t *respmsg) {
  respmsg->message = ERRMSG_NOT_IMPLEMENTED;
}

/* Generic entrypoint for this MASTER. Takes in a socket on SOCKFD, which
 * should already be connected to an incoming request. Processes the request
 * and sends back a response message.  This should call out to the appropriate
 * internal handler. */
void tpcmaster_handle(tpcmaster_t *master, int sockfd, callback_t callback) {
  kvmessage_t *reqmsg, respmsg;
  reqmsg = kvmessage_parse(sockfd);
  memset(&respmsg, 0, sizeof(kvmessage_t));
  respmsg.type = RESP;
  if (reqmsg->key != NULL) {
    respmsg.key = calloc(1, strlen(reqmsg->key));
    strcpy(respmsg.key, reqmsg->key);
  }
  if (reqmsg->type == INFO) {
    tpcmaster_info(master, reqmsg, &respmsg);
  } else if (reqmsg == NULL || reqmsg->key == NULL) {
    respmsg.message = ERRMSG_INVALID_REQUEST;
  } else if (reqmsg->type == REGISTER) {
    tpcmaster_register(master, reqmsg, &respmsg);
  } else if (reqmsg->type == GETREQ) {
    tpcmaster_handle_get(master, reqmsg, &respmsg);
  } else {
    tpcmaster_handle_tpc(master, reqmsg, &respmsg, callback);
  }
  kvmessage_send(&respmsg, sockfd);
  kvmessage_free(reqmsg);
  if (respmsg.key != NULL)
    free(respmsg.key);
}

/* Completely clears this TPCMaster's cache. For testing purposes. */
void tpcmaster_clear_cache(tpcmaster_t *tpcmaster) {
  kvcache_clear(&tpcmaster->cache);
}
