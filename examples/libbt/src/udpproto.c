#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "udpproto.h"
#include "context.h"
#include "util.h"
#include "peer.h"
#include "random.h"

#define BUFLEN 8192
#define SHIFT_DIGEST(ptr, dig) (memcpy(ptr, dig, SHA_DIGEST_LENGTH),ptr += SHA_DIGEST_LENGTH)

#define EVENT_NONE 0
#define EVENT_COMPLETED 1
#define EVENT_STARTED 2
#define EVENT_STOPPED 3

#define ACTION_CONNECT 0
#define ACTION_ANNOUNCE 1
#define ACTION_SCRAPE 2
#define ACTION_ERROR 3

int udp_init( short port) {
    int udp_socket;
    struct sockaddr_in sa;
    int err;

    udp_socket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket == -1) return -1;

    sa.sin_family = AF_INET;
    sa.sin_port = htons( port);
    sa.sin_addr.s_addr = INADDR_ANY;

    err = bind( udp_socket, (struct sockaddr *)&sa, sizeof(sa));
    if (err == -1) {
	close( udp_socket);
	return -2;
    } else {
	printf("UDP socket %d listening on port %d\n", udp_socket, port);
    }

    return udp_socket;
}

static int parse( char *locator, struct sockaddr_in *psa) {
    int len;
    int port;
    char *host;
    char *hostname;
    char *sep;
    int err = 0;

    const struct addrinfo ai_template={
      .ai_family=AF_INET,
      .ai_socktype=SOCK_DGRAM
    };
    struct addrinfo *ai=NULL;

    /* verify that this is a UDP url */
    if (strncmp( locator, "udp://", 6) != 0) {
	return -1;
    }

    /* find the hostname and port number */
    host = locator + 6;
    sep = strchr( host, ':');
    if (sep == NULL) {
        return -2;
    }
    len = sep - host;
    hostname = btcalloc( 1, len+1);
    strncpy( hostname, host, len);
    port = atoi( sep+1);

    /* lookup the hostname */ 

    err = getaddrinfo(hostname, NULL, &ai_template, &ai);
    if (err==0) {	
	/* Just pick the first one, they are returned in varying order */
	if (ai->ai_addr->sa_family == AF_INET) {
	    /*result of lookup is an IPV4 address */
	    *psa = *(struct sockaddr_in *)ai->ai_addr;
	    psa->sin_port = htons(port);
	} else {
	    printf( "Tracker lookup returned unsupported address type %d. \n", 
		    ai->ai_addr->sa_family);
	    err = -3;
	}
    } else {
	printf("getaddrinfo: error %d\n", err);
    }

    /* clean up */
    if(ai) {
	freeaddrinfo(ai);
    }

    btfree( hostname);
    return err;
}

static int udp_send( int udpsocket, char *msg, int msglen, char *announce) {
    struct sockaddr_in sa;
    int err;

    err = parse( announce, &sa);
    if (err) {
        printf("Error parsing url '%s'\n", announce);
	return err;
    }

    err = sendto( udpsocket, msg, msglen, 0, (struct sockaddr *)&sa, sizeof(sa));
    if (err == -1) {
	perror("send error");
	return -3;
    }
    return 0;
}

static int udp_recv( int udpsocket, char *msgbuf, int bufsize, int *msglen) {
    struct sockaddr_in src;
    int srclen = sizeof(src);

    *msglen = recvfrom( udpsocket, msgbuf, bufsize, 0, (struct sockaddr *)&src, &srclen);
    if (*msglen == -1 || *msglen >= BUFLEN) return -3;

#if 0
    printf("Received UDP message:\n");
    hexdump( msgbuf, *msglen);
#endif
    
    return 0;
}

static int find_dl( btContext *ctx, int txid) {
    int i;
    for (i=0; i<ctx->downloadcount; i++) {
        btDownload *dl = ctx->downloads[ i];
	if ( dl->txid == txid && dl->connecting != 0) {
	    return i;
	}
    }
    return -1;
}

static int udp_request( int udpsocket, 
	_int64 connection_id, 
	char *announce,
        unsigned char digest[SHA_DIGEST_LENGTH],
	unsigned char myid[SHA_DIGEST_LENGTH],
	unsigned short port,
	_int64 downloaded,
	_int64 uploaded,
	_int64 left,
	int event,
	int myipaddr,
	int *txid,
        char key[KEYSIZE]
    ) 
{
#   define ANNOUNCE_LEN 98
    char msg[ANNOUNCE_LEN];
    char *p;
    _int32 action = ACTION_ANNOUNCE;
    _int32 transactionid = rnd(INT_MAX);
    kStringBuffer sb;
    kStringBuffer_create( &sb);
    int err;
    int numwant = -1;
    int i;

    assert(KEYSIZE==4);

    _int64 b64;
    _int32 b32;

    assert( event >= EVENT_NONE && event <= EVENT_STOPPED);

    p = msg;
    SHIFT_INT64( p, b64, connection_id);
    SHIFT_INT32( p, b32, action);
    SHIFT_INT32( p, b32, transactionid);
    SHIFT_DIGEST( p, digest);
    SHIFT_DIGEST( p, myid);
    SHIFT_INT64( p, b64, downloaded);
    SHIFT_INT64( p, b64, left);
    SHIFT_INT64( p, b64, uploaded);
    SHIFT_INT32( p, b32, event);
    SHIFT_INT32( p, b32, myipaddr);
    for (i = 0; i < KEYSIZE; i++) {
	SHIFT_BYTE( p, key[i]);
    }
    SHIFT_INT32( p, b32, numwant);
    SHIFT_INT16( p, b32, port);        // send in NBO

    assert(p-msg == ANNOUNCE_LEN);

#if 0
    printf("UDP_Request msg:\n");
    hexdump(msg, p-msg);
#endif

    err = udp_send( udpsocket, msg, ANNOUNCE_LEN, announce);
    if (err < 0) {
        return err;
    }

    *txid = transactionid;
    return 0;
}

int udp_ready( btContext *ctx) {
    char msg[BUFLEN];
    char *p;
    int msglen;
    int err;
    _int32 b32;
    _int64 b64;
    int leechers;
    int seeders;
    int action;
    int txid;
    struct sockaddr_in target;
    int download;
    btDownload *dl;
    
    err = udp_recv( ctx->udpsock, msg, BUFLEN, &msglen);
    if (err) {
	return -1;
    }

    p = msg;
    UNSHIFT_INT32( p, b32, action);
    UNSHIFT_INT32( p, b32, txid);

    /* find btDownload that is expecting this message */
    download = find_dl( ctx, txid);
    if (download < 0) { 
	printf("Unexpected UDP message ignored: action=%d, txid=%d\n", action, txid);
    	return -1;
    }
    dl = ctx->downloads[download];
    dl->txid = 0;

    /* handle the message */
    switch (action) {
        case ACTION_CONNECT:
	    if (dl->connecting != 1) return -1;
	    int myip=0;

	    UNSHIFT_INT64( p, b64, dl->cxid);
	    if (udp_request( ctx->udpsock, dl->cxid, dl->url[dl->tracker],
		    dl->infohash, ctx->myid,
		    ctx->listenport, dl->fileset.dl,
		    dl->fileset.ul * ctx->ulfactor, dl->fileset.left,
		    EVENT_STARTED, myip,
		    &dl->txid, ctx->mykey)
		) 
	    {
		return -2;
	    }
	    dl->connecting = 2;
	    break;

	case ACTION_ANNOUNCE:
	    if (dl->connecting != 2) return -2;

	    UNSHIFT_INT32( p, b32, dl->reregister_interval);
	    UNSHIFT_INT32( p, b32, leechers);
	    UNSHIFT_INT32( p, b32, seeders);

	    while (p - msg < msglen) {
		/* Collect the address */
		target.sin_family=AF_INET;
		memcpy(&target.sin_addr, p, 4);
		memcpy(&target.sin_port, p+4, 2);	/* already NBO */
		p += 6;

                /* detect duplicates (including self) */
		if (peer_seen( ctx, download, &target)) {
		    continue;
		}

		peer_add( ctx, download, "", &target);
	    }
	    
	    break;
	    
	case ACTION_ERROR:
	    fprintf( stderr, "Error: ");
	    for (;p < msg+msglen;p++) {
	        if (*p == 0) break;
		if (*p <32) continue;
		putc( *p, stderr);
	    }
	    fprintf( stderr, "\n");
	    break;

	case ACTION_SCRAPE:
	    /* fall through */
	default:
	    fprintf( stderr, "Unhandled udp action %d\n", action);
	    break;
    }
    return 0;
}

int udp_connect( btContext *ctx, btDownload *dl) {
#   define CONNECTLEN 16
    _int64 connection_id = 0x41727101980LL;
    _int32 action = ACTION_CONNECT;
    _int32 transactionid = rnd(INT_MAX);
    char msg[CONNECTLEN];
    char *p = msg;
    kStringBuffer sb;
    kStringBuffer_create( &sb);
    int err = 0;
    _int64 b64;
    _int32 b32;

    SHIFT_INT64( p, b64, connection_id);
    SHIFT_INT32( p, b32, action);
    SHIFT_INT32( p, b32, transactionid);
    assert( p - msg == CONNECTLEN);

    err = udp_send( ctx->udpsock, msg, CONNECTLEN, dl->url[dl->tracker]);
    if (err) { 
	printf("udp_send returned error %d\n", err);
	return err;
    }

    dl->txid = transactionid;
    dl->connecting = 1;
    return 0;
}

int udp_announce( btContext *ctx, btDownload *dl, char *state) {
    int event;
    int err;
    if (state == NULL) {
	event = EVENT_NONE;
    } else if (strcmp(state, "started") == 0) {
	event = EVENT_STARTED;
    } else if (strcmp(state, "stopped") == 0) {
	event = EVENT_STOPPED;
    } else if (strcmp(state, "completed") == 0) {
	event = EVENT_COMPLETED;
    } else {
	dl->cxid = 0;
	return -1;
    }

    err = udp_request( ctx->udpsock, 
	    dl->cxid, 
	    dl->url[dl->tracker],
	    dl->infohash,
	    ctx->myid,
	    ctx->listenport,
	    dl->fileset.dl,
	    dl->fileset.ul * ctx->ulfactor,
	    dl->fileset.left,
	    event, 0, 
	    &dl->txid, 
	    ctx->mykey
	);
    return err;
}

