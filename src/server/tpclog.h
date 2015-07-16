#ifndef __TPC_LOG__
#define __TPC_LOG__

#include <stdbool.h>
#include <pthread.h>
#include "kvconstants.h"

/* TPCLog defines a log which will log the TPC actions for a server such that
 * it can recreate its state after a crash.
 *
 * Entries in the log are stored within DIRNAME with an incrementing id. The
 * first entry has a filename of "0.log", the second has a filename of "1.log",
 * and so on. Files should always be sequential; for example, there should
 * never be a filename of "2.log" without a filename of "1.log"
 *
 * Servers can use the TPCLog to log each incoming action they receive, and
 * later use the tpclog_iterate methods to iterate over all entries in the log,
 * in order of receipt, to recreate their state as necessary. Because the
 * iterator will walk over all entries in the log, servers should call
 * tpclog_clear_log periodically to clear the log. This will erase all entries
 * in the log, so it should only be called when the server is confident that it
 * will not need any existing entry to recreate state.
 */

/* Filetype to use as an extension for the filenames of entries in the TPCLog. */
#define TPCLOG_FILETYPE ".log"

/* A TPCLog. */
typedef struct {
  char *dirname;             /* The name of the directory in which to store log entries. */
  unsigned long nextid;      /* The ID of the next entry to be stored in the log. */
  unsigned long iterpos;     /* The position of the current iteration over the entries. */
  pthread_rwlock_t lock;     /* A read-write lock used to make TPCLog thread-safe. */
} tpclog_t;

/* A single log entry.
 * For messages of type COMMIT and ABORT, data is empty.
 * For messages of type DELREQ, data holds the relevant key.
 * For messages of type PUTREQ, data holds both the key and the value, in the
 * form:
 *   key_string \0 value_string \0
 *   (that is, two concatenated and null terminated strings) */
typedef struct {
  msgtype_t type;          /* The type of message this log entry represents. */
  int length;              /* Stores the total length of DATA, including null terminators. */
  char data[0];            /* Described above. */
} logentry_t;

int tpclog_init(tpclog_t *, char *dirname);

int tpclog_log(tpclog_t *, msgtype_t type, char *key, char *value);

int tpclog_load_entry(logentry_t **entry, char *filename);

void tpclog_iterate_begin(tpclog_t *log);
bool tpclog_iterate_has_next(tpclog_t *log);
logentry_t *tpclog_iterate_next(tpclog_t *log);

int tpclog_clear_log(tpclog_t *);

#endif
