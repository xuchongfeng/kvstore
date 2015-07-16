#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "kvconstants.h"
#include "tpclog.h"

/* Initialize TPCLog LOG to use the provided DIRNAME to store its associated
 * entries. Sets LOG's NEXTID field based on the entries that currently exist
 * in DIRNAME. */
int tpclog_init(tpclog_t *log, char *dirname) {
  struct stat st;
  unsigned long nextid = 0;
  char filename[MAX_FILENAME];
  if (stat(dirname, &st) == -1) {
    if (mkdir(dirname, 0700) == -1)
      return errno;
  }
  log->dirname = malloc(strlen(dirname) + 1);
  if (log->dirname == NULL)
    return ENOMEM;
  strcpy(log->dirname, dirname);
  pthread_rwlock_init(&log->lock, NULL);

  /* Iterate through entries to determine next available ID, since this log may
   * be recovering from a crash. */
  sprintf(filename, "%s/%lu%s", log->dirname, nextid++, TPCLOG_FILETYPE);
  while (stat(filename, &st) != -1)
    sprintf(filename, "%s/%lu%s", log->dirname, nextid++, TPCLOG_FILETYPE);
  log->nextid = nextid - 1;
  return 0;
}

/* Add a log entry to LOG which will store the message type TYPE and, as
 * applicable, the associated KEY and VALUE (which should be NULL if they are
 * not applicable). See tpclog.h for a complete description of how log entries
 * should be stored in the file system. */
int tpclog_log(tpclog_t *log, msgtype_t type, char *key, char *value) {
  char filename[MAX_FILENAME];
  int fd, keylen, vallen;
  size_t size;
  logentry_t *entry;
  if (type != PUTREQ && type != DELREQ && type != ABORT && type != COMMIT)
    return ERRINVLDMSG;
  pthread_rwlock_wrlock(&log->lock);
  sprintf(filename, "%s/%lu%s", log->dirname, log->nextid, TPCLOG_FILETYPE);
  if ((fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR)) < 0) {
    pthread_rwlock_unlock(&log->lock);
    return ERRFILACCESS;
  }
  log->nextid++;

  keylen = (type == PUTREQ || type == DELREQ) ? (strlen(key) + 1) : 0;
  vallen = (type == PUTREQ) ? (strlen(value) + 1) : 0;
  size = sizeof(logentry_t) + keylen + vallen;
  entry = malloc(size);
  if (entry == NULL) {
    pthread_rwlock_unlock(&log->lock);
    return ENOMEM;
  }
  entry->type = type;
  entry->length = keylen + vallen;
  if (type == PUTREQ || type == DELREQ)
    strcpy(entry->data, key);
  if (type == PUTREQ)
    strcpy(entry->data + keylen, value);
  errno = 0;
  if (write(fd, entry, size) < size) {
    pthread_rwlock_unlock(&log->lock);
    return ERRFILACCESS;
  }
  close(fd);
  pthread_rwlock_unlock(&log->lock);
  free(entry);
  return 0;
}

/* Load the logentry located at FILENAME into ENTRY, which will be set to
 * malloc()d memory which should be later free()d. Returns 0 if successful,
 * else a negative error code (and ENTRY will be NULL). */
int tpclog_load_entry(logentry_t **entry, char *filename) {
  logentry_t tmp;
  int fd;
  size_t size;

  if ((fd = open(filename, O_RDONLY)) < 0)
    return ERRFILACCESS;
  if (read(fd, &tmp, sizeof(logentry_t)) < sizeof(logentry_t))
    return ERRFILACCESS;
  lseek(fd, SEEK_SET, 0);
  size = tmp.length + sizeof(logentry_t);
  *entry = malloc(size);
  if (*entry == NULL)
    return ENOMEM;
  if (read(fd, *entry, size) < size) {
    free(*entry);
    return ERRFILACCESS;
  }
  close(fd);
  return 0;
}

/* Prepare LOG to be iterated over. Once this is called, use the functions
 * tpclog_iterate_has_next and tpclog_iterate_next to iterate through all of
 * the entries in LOG from oldest to most recent. */
void tpclog_iterate_begin(tpclog_t *log) {
  log->iterpos = 0;
}

/* Must be called after tpclog_iterate_begin has been called on LOG. Returns
 * true iff LOG has another entry that is more recent than the most previously
 * iterated over log entry. */
bool tpclog_iterate_has_next(tpclog_t *log) {
  struct stat st;
  char filename[MAX_FILENAME];
  sprintf(filename, "%s/%lu%s", log->dirname, log->iterpos, TPCLOG_FILETYPE);
  if (stat(filename, &st) == -1)
    return false;
  return true;
}

/* Must be called after tpclog_iterate_begin has been called on LOG. Attempts
 * to return the next most recent entry after the entry previously returned
 * during the current iteration, or, immediately after tpclog_iterate_begin,
 * the oldest entry in LOG, using malloc()d memory which should later be
 * free()d. Returns NULL if there is an error or no more recent entry exists
 * (i.e., all entries have been iterated over). */
logentry_t *tpclog_iterate_next(tpclog_t *log) {
  char filename[MAX_FILENAME];
  logentry_t *entry;
  int ret;
  pthread_rwlock_rdlock(&log->lock);
  if (!tpclog_iterate_has_next(log)) {
    pthread_rwlock_unlock(&log->lock);
    return NULL;
  }
  sprintf(filename, "%s/%lu%s", log->dirname, log->iterpos++, TPCLOG_FILETYPE);
  ret = tpclog_load_entry(&entry, filename);
  pthread_rwlock_unlock(&log->lock);
  return (ret < 0) ? NULL : entry;
}

/* Clear the log of all entries. Should be called periodically to keep the
 * number of entries from becoming too large, since a server rebuild will
 * iterate through all existing entries. */
int tpclog_clear_log(tpclog_t *log) {
  struct stat st;
  char filename[MAX_FILENAME];
  unsigned long count = 0;

  pthread_rwlock_wrlock(&log->lock);
  sprintf(filename, "%s/%lu%s", log->dirname, count++, TPCLOG_FILETYPE);
  while (stat(filename, &st) != -1) {
    if (remove(filename) < 0) {
      pthread_rwlock_unlock(&log->lock);
      return errno;
    }
    sprintf(filename, "%s/%lu%s", log->dirname, count++, TPCLOG_FILETYPE);
  }
  pthread_rwlock_unlock(&log->lock);
  log->nextid = 0;
  return 0;
}
