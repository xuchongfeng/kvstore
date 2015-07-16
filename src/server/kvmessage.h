#ifndef __KV_MESSAGE__
#define __KV_MESSAGE__

#include "kvconstants.h"

/* KVMessage is used to send messages across sockets.
 *
 * kvmessage_send first sends the size of the entire message in the first four bytes.
 * The following bytes contain the contents of the message in JSON format. Only
 * fields which are non-null in the passed in message are sent.
 *
 * kvmessage_parse reads the first four bytes of the message, uses this to determine
 * the size of the remainder of the message, then parses the remainder of the message
 * as JSON and populates whichever fields of the message are present in the incoming JSON.
 */

typedef struct {
  msgtype_t type;    /* The type of this message. */
  char *key;         /* The key this message stores. May be NULL, depending on type. */
  char *value;       /* The value this message stores. May be NULL, depending on type. */
  char *message;     /* The message this message stores. May be NULL, depending on type. */
} kvmessage_t;

kvmessage_t *kvmessage_parse(int sockfd);

int kvmessage_send(kvmessage_t *, int sockfd);

void kvmessage_free(kvmessage_t *);

#endif
