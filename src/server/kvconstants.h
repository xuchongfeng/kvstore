#ifndef __KV_CONSTANTS__
#define __KV_CONSTANTS__

/* KVConstants contains general purpose constants for use throughout the
 * project. */

/* Maximum length for keys and values. */
#define MAX_KEYLEN 1024
#define MAX_VALLEN 1024

/* Maximum length for a file name. */
#define MAX_FILENAME 1024

/* Error messages to be used with KVMessage. */
#define MSG_SUCCESS "SUCCESS"
#define ERRMSG_NO_KEY "ERROR: NO KEY"
#define ERRMSG_KEY_LEN "ERROR: IMPROPER KEY LENGTH"
#define ERRMSG_VAL_LEN "ERROR: VALUE TOO LONG"
#define ERRMSG_INVALID_REQUEST "ERROR: INVALID REQUEST"
#define ERRMSG_NOT_IMPLEMENTED "ERROR: NOT IMPLEMENTED"
#define ERRMSG_GENERIC_ERROR "ERROR: UNABLE TO PROCESS REQUEST"

/* Convert an error code to an error message. */
#define GETMSG(error) ((error == ERRKEYLEN) ? ERRMSG_KEY_LEN : \
                      ((error == ERRVALLEN) ? ERRMSG_VAL_LEN : \
                      ((error == ERRNOKEY)  ? ERRMSG_NO_KEY  : \
                                              ERRMSG_GENERIC_ERROR)))

/* Message types for use by KVMessage. */
typedef enum {
  GETREQ,
  PUTREQ,
  DELREQ,
  GETRESP,
  RESP,
  ACK,
  ABORT,
  COMMIT,
  VOTE_COMMIT,
  VOTE_ABORT,
  REGISTER,
  INFO
} msgtype_t;

/* Possible TPC states. */
typedef enum {
  TPC_INIT,
  TPC_WAIT,
  TPC_READY,
  TPC_ABORT,
  TPC_COMMIT
} tpc_state_t;

/* Error types/values */
/* Error for invalid key length. */
#define ERRKEYLEN -11
/* Error for invalid value length. */
#define ERRVALLEN -12
/* Error for invalid (not present) key. */
#define ERRNOKEY -13
/* Error for invalid message type. */
#define ERRINVLDMSG -14
/* Error code used to represent that a filename was too long. */
#define ERRFILLEN -15
/* Error code used to represent that a file could not be created. */
#define ERRFILCRT -16
/* Error returned if error was encountered accessing a file. */
#define ERRFILACCESS -17

#endif
