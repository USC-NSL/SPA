#ifndef __CONTEXT__H
#define __CONTEXT__H
#include <openssl/sha.h>
#include <sys/types.h>
#if WIN32
#   include "poll.h"
#else
#   include <sys/poll.h>
#   include <netinet/in.h>
#endif
#include "types.h"
#include "segmenter.h"
#include "peer.h"
#include "bitset.h"
#define IDSIZE 20
#define KEYSIZE 4
#if WIN32
#   define MAXCONN 60
#else
#   define MAXCONN 100
#endif
#define SOCKID_MAX 1024
#define TMPLOC SOCKID_MAX
#define TRACKER_MAX 20

#define CTX_STATUS_REVENTS(ctx,ev) ((ctx)->status[ev].revents)

/* TBD */
typedef struct btDownload {
    btObject *md;		/* tracker metadata */
    char *url[TRACKER_MAX];	/* announce url */
    int urlrank[TRACKER_MAX];	/* url priority */
    int tracker;		/* which tracker to check */
    int nurl;			/* number of announce urls */
    char infohash[SHA_DIGEST_LENGTH];
    kBitSet requested;		/* requested blocks */
    kBitSet interested;		/* interested blocks */
    btFileSet fileset;		/* set of files to be written */
    int reregister_interval;	/* how often to reregister with tracker */
    time_t reregister_timer;    /* Unix time of last tracker registration */

    btPeerset peerset;		/* set of peers on this torrent */
    
    int privateflag; /* Set if torrent is tracked by private tracker, disables PEX */

    int complete;		/* download completed */

    int connecting;		/* UDP connecting flag */
    int txid;			/* UDP transaction id */
    _int64 cxid;		/* UDP connection id */

} btDownload;

typedef struct btContext {
    btDownload **downloads;	/* Torrents this context keeps track of */
    int downloadcount;
    struct btPeer* sockpeer[FD_SETSIZE];
    
    char myid[IDSIZE];
    char mykey[KEYSIZE];
    int minport;
    int maxport;
    int listenport;
    /*struct in_addr ip;			/ * my ip address */

    int statmap[SOCKID_MAX+1];	/* socket number to status number map */
    int nstatus;		/* next available status */
    struct pollfd status[MAXCONN]; /* socket status */

    int x_set[SOCKID_MAX]; 	/* timeslicing bits */
    int xsock;			/* number of bits that are set in x_set */
    int ss;			/* server socket */
    int udpsock;		/* UDP server socket */
    float ulfactor;		/* upload multiplier */
} btContext;

btContext *btContext_create( btContext *ctx, float ulfactor, char *rcfile);
    /*
     * Initialize a context from an rcfile 
     */

void btContext_destroy( btContext *ctx);
    /*
     * Free all resources associated with context.
     */

btObject* btannounce(btContext *ctx, btDownload *dl, char *state, int err);
int btresponse( btContext *ctx, int download, btObject *resp);

void ctx_setevents( struct btContext *ctx, int fd, int events);
void ctx_clrevents( struct btContext *ctx, int fd, int events);
    /*
     * Add in or remove events from the set of event flags
     * to be polled.
     */

void ctx_delstatus( struct btContext *ctx, int fd);
int ctx_addstatus( struct btContext *ctx, int fd);
    /*
     * Allocate a pollfd status block for 'fd'.  If 'fd' is set to TMPLOC
     * then the status block will be allocated to a temporary location.
     * Calling the _delstatus() with TMPLOC will free the temporary status
     * block, or caling _fixtmp() will link it to a real 'fd'.
     */

void ctx_fixtmp( struct btContext *ctx, int fd);

int ctx_register( struct btContext *ctx, unsigned download);
    /*
     * Contact the tracker get back the initial peer set.
     */

int ctx_startserver( struct btContext *ctx);
    /*
     * Starts the server socket
     */

int ctx_shutdown( btContext *ctx, unsigned download) ;
    /*
     * Tell the tracker that we are shutting down.
     */

int ctx_complete( btContext *ctx, unsigned download) ;
    /*
     * Tell the tracker that we are done.
     */

int ctx_reregister( btContext *ctx, unsigned download) ;
    /*
     * Update the tracker with our status.
     */

void ctx_exit( int exitcode, void *arg) ;
    /*
     * on_exit() callable function to call shutdown
     */

struct btStream;
int ctx_loadfile( struct btStream *bts, struct btContext *ctx, char *downloaddir, int assumeok, char *ignorepattern) ;
    /*
     * Load a '.torrent' metadata file into the current context.
     * Returns the download ID, which is >=0, or <0 for error.
     */
void ctx_closedownload(btContext *ctx, unsigned download);

int ctx_readfastresume(btDownload *dl, kBitSet *partialData);
int ctx_completedecode (unsigned char *digest, int len, const char *buf);
int ctx_writefastresume(btDownload *dl);
int ctx_deletefastresume(btDownload *dl);

btFileSet ctx_checkhashfordata(btDownload *dl);
/* takes a download and calls hashpartialdata, then updates the 
// requested/interested tables and prints status to stdout.
*/

int ctx_hashpartialdata(btFileSet *templateFileSet, kBitSet *writeCache);
/* Takes a pointer to a fileset to read from, and a temporary allocated bitset
// and writes hash data to it via checkhashforpiece, returns number of good pieces
*/

int ctx_checkhashforpiece(btFileSet *templateFileSet, kBitSet *writeCache, int i);
/* Called from checkhashfordata, returns completion of individual piece
*/

int ctx_writehashtodownload(btDownload *dl, kBitSet *partialData);

/* Utilities */
char *ctx_stringfrommd(btObject *md, char *mdkey);
int ctx_hexdecode(unsigned char *digest, int len, const char *buf);

#endif
