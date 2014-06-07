#ifndef _BTERROR__H
#define _BTERROR__H

/* if adding new error types, keep the string array in synch with
   the error list, and number new errors sequentially.  See die().
*/
enum{
    BTERR_BASE=500,

    BTERR_PROTOCOL_ID = BTERR_BASE,
    BTERR_UNKNOWN_FLAGS,
    BTERR_HASH_MISMATCH,
    BTERR_NETWORK_ERROR,
    BTERR_POLLHUP,
    BTERR_POLLERR,
    BTERR_POLLNVAL,
    BTERR_LARGEPACKET,
    BTERR_NEGATIVE_STRING_LENGTH,
    BTERR_QUEUE_OVERFLOW,

    BTERR_LAST
};

#ifdef BTERROR_BODY
char *bterror_string[] = {
    "Bad protocol ID on peer connection",
    "Unrecognized Flags in peer protocol handshake",
    "Peer hash value doesn't match my hash value",
    "Peer disconnected after repeated errors",
    "Poll detected that the peer hung up",
    "Poll detected a socket error",
    "Poll detected an invalid parameter",
    "Peer disconnected after too large packet",
    "Protocol error: Tried to decode a negative length string",
    "Peer queued too many requests"
};
#else
extern char *bterror_string[];
#endif

#endif 
