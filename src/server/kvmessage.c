#include <stdlib.h>
#include <unistd.h>
#include <json-c/json.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include "kvmessage.h"

/* Receives and returns a message from socket SOCKFD.
 * Returns NULL if there is an error. */
kvmessage_t *kvmessage_parse(int sockfd) {
  json_object *new_obj;
  kvmessage_t *msg = (kvmessage_t *) calloc(1, sizeof(kvmessage_t));
  int size;

  /* First read the size of the incoming message */
  if (read(sockfd, &size, 4) < 4) {
    return NULL;
  }
  /* Then create the buffer and read in the data */
  size = ntohl(size);
  char buffer[size];
  if (read(sockfd, buffer, size) < size) {
    return NULL;
  }

  struct json_object *value_obj;
  new_obj = json_tokener_parse(buffer);
  if (json_object_object_get_ex(new_obj, "type", &value_obj)) {
    int type = json_object_get_int(value_obj);
    msg->type = type;
  }

  if (json_object_object_get_ex(new_obj, "key", &value_obj)) {
    const char *key = json_object_get_string(value_obj);
    char *key_buf = calloc(1, strlen(key) + 1);
    memcpy(key_buf, key, strlen(key) + 1);
    msg->key = key_buf;
  }
  if (json_object_object_get_ex(new_obj, "value", &value_obj)) {
    const char *value = json_object_get_string(value_obj);
    char *value_buf = calloc(1, strlen(value) + 1);
    memcpy(value_buf, value, strlen(value) + 1);
    msg->value = value_buf;
  }
  if (json_object_object_get_ex(new_obj, "message", &value_obj)) {
    const char *message = json_object_get_string(value_obj);
    char *message_buf = calloc(1, strlen(message) + 1);
    memcpy(message_buf, message, strlen(message) + 1);
    msg->message = message_buf;
  }
  json_object_put(new_obj);
  return msg;
}

/* Sends MESSAGE on socket SOCKFD. Includes whichever fields are
 * non-null in the message. Returns the number of bytes which were sent. */
int kvmessage_send(kvmessage_t *message, int sockfd) {
  int sent = 0;
  json_object *json = json_object_new_object();
  json_object_object_add(json, "type", json_object_new_int(message->type));
  if (message->key) {
    json_object_object_add(json, "key", json_object_new_string(message->key));
  }
  if (message->value) {
    json_object_object_add(json, "value",
        json_object_new_string(message->value));
  }
  if (message->message) {
    json_object_object_add(json, "message",
        json_object_new_string(message->message));
  }
  const char *json_string = json_object_to_json_string(json);
  int size = htonl(strlen(json_string));
  sent += write(sockfd, &size, 4);
  sent += write(sockfd, json_string, strlen(json_string));
  json_object_put(json);
  return sent;
}

/* Frees the memory for MESSAGE. Assumes that the message itself and all
 * fields were allocated using malloc/calloc (which will be the case for a
 * message created using kvmessage_parse). */
void kvmessage_free(kvmessage_t *message) {
  if (message->key)
    free(message->key);
  if (message->value)
    free(message->value);
  if (message->message)
    free(message->message);
  free(message);
}
