/* 
 * Copyright 2003,2004,2005 Kevin Smathers, All Rights Reserved
 *
 * This file may be used or modified without the need for a license.
 *
 * Redistribution of this file in either its original form, or in an
 * updated form may be done under the terms of the GNU LIBRARY GENERAL
 * PUBLIC LICENSE.  If this license is unacceptable to you then you
 * may not redistribute this work.
 * 
 * See the file COPYING.LGPL for details.
 */

# include "config.h"

#if !WIN32
#   include <sys/types.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <netdb.h>
#   include <sys/socket.h>
#   include <limits.h>
#   if HAVE_UNISTD_H
#      include <unistd.h>
#   endif
#   if HAVE_FCNTL_H
#      include <fcntl.h>
#   endif
#endif
#include <sys/types.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#   include <strings.h>
#endif
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <assert.h>

#include "bterror.h"
#include "btmessage.h"
#include "peer.h"
#include "stream.h"
#include "bitset.h"
#include "context.h"
#include "segmenter.h"
#include "bts.h"
#include "benc.h"
#include "peerexchange.h"


#if WIN32
#   define EINPROGRESS     WSAEINPROGRESS   /* Operation now in progress */
#   define close(s) closesocket(s)
#   define int32_t signed int
#endif
#define REQMAX 5	    /* Maximum requests to send.  Must be less than queuesize or local queue will overflow */
#define DOWNLOADS 4
#define REQUEST_SIZE 16384  /* Default request size */
char g_filebuffer[MAXREQUEST];	/* This should be moved to context or allocated in process_queue */

#ifdef TRACE
#   define bttrace(msg) printf("%s+%d:%s\n", __FILE__, __LINE__, msg)
#else
#   define bttrace(msg)
#endif

btPeerset* btPeerset_create( btPeerset *pset) {
    if (!pset) {
	pset = (btPeerset *)btmalloc(sizeof(btPeerset));
    }
    memset(pset, 0, sizeof(btPeerset));
    return pset;
}

/* prototypes */
int
send_have( btPeer *peer, int piece);
int 
send_cancel( btPeer *peer, int piece, int offs, int len);
int
send_choke( btPeer *peer, int choke) ;

int peer_connect_complete( btContext* ctx, struct btPeer *p) {
    int error;
    int	errlen;
    bttrace("peer_connect_complete");


    errlen = sizeof(error);
    if (getsockopt( p->ios.fd, SOL_SOCKET, SO_ERROR, &error, &errlen)) {
	p->ios.error = errno;
	bts_perror(p->ios.error, "getsockopt");
       bttrace("peer_connect_complete error");
	return p->ios.error;
    }
    if (error != 0) {
	errno = error;
	p->ios.error = error;
	bts_perror(p->ios.error, "connect_complete");
	p->state = PEER_ERROR;
       bttrace("peer_connect_complete error");
	return p->ios.error;
    }
    p->state = PEER_OUTGOING;
    printf("%d: completed connection %s\n", p->ios.fd, inet_ntoa(p->sa.sin_addr));
   bttrace("peer_connect_complete exit");
    return 0;    
}

int peer_connect_request( btContext* ctx, struct btPeer *p) {
    int sock;
    long flags;
    int err;

    bttrace("peer_connect_request");

    if (p == NULL) {
	bttrace("peer_connect_request error");
    	return -1;
    }
    if (!memcmp(ctx->myid, p->id, IDSIZE)) {
	bttrace("peer_connect_request error");
    	return -1;
    }

    /* create the socket */
    sock = socket( PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
	bts_perror(errno, "socket");
	bttrace("peer_connect_request error");
	return -1;
    }

    /* change socket to non-blocking */
#if WIN32
    flags = ioctlsocket( sock, FIONBIO, (unsigned long *) 1);
    if (flags != 0) {
        bts_perror(errno, "ioctlsocket");
	close(sock);
	bttrace("peer_connect_request error");
        return -1;
    }
#else
    flags = fcntl( sock, F_GETFL);
    if (flags < 0) {
        bts_perror(errno, "fcntl F_GETFL");
	close(sock);
	bttrace("peer_connect_request error");
        return -1;
    }
    
    flags |= O_NONBLOCK;
    if ( fcntl( sock, F_SETFL, flags)) {
        bts_perror(errno, "fcntl F_SETFL");
	close(sock);
	bttrace("peer_connect_request error");
        return -1;
    }
#endif


    /* connect the socket */
    err = connect( sock, (void *)&p->sa, sizeof(struct sockaddr_in));
    if (err) {
#if WIN32
        errno=WSAGetLastError();
#endif
	if (errno != EINPROGRESS) {
	    bts_perror(errno, "connect");
	    close(sock);
	    bttrace("peer_connect_request error");
	    return -1;
	}
    }

    /* create a kStream for the socket */
    kStream_create( &p->ios, sock);
    /* index the peer by socket */
    ctx->sockpeer[sock]=p;

    if (err) {
	/* EINPROGRESS */
	bttrace("peer_connect_request error (EINPROGRESS)");
	return 1;
    }
    bttrace("peer_connect_request exit");
    return 0;
}

int peer_seen( btContext *ctx, unsigned download, struct sockaddr_in *target) {
    int j;

    btDownload *dl = ctx->downloads[download];
    /* Detect collisions */
    for (j = 0; j < dl->peerset.len; j++) {
	btPeer *p = dl->peerset.peer[j];
	if ( p->sa.sin_port == target->sin_port &&
	     memcmp( &p->sa.sin_addr, &target->sin_addr, sizeof(target->sin_addr) == 0)
	   ) 
	{ 
	    fprintf( stderr, 
		   "Skipping old peer: %s:%d\n", 
		   inet_ntoa(target->sin_addr), ntohs(target->sin_port));
	    return 1;
	}
    }
    return 0;
}

btPeer *peer_add( btContext *ctx, unsigned download, char *id, struct sockaddr_in *sa) {
    btDownload *dl=ctx->downloads[download];
    btPeerset *pset = &dl->peerset;
    int idx, err;
    struct btPeer *p;

    bttrace("peer_add");

    DIE_UNLESS(download<ctx->downloadcount);

    /* initialize status info */
    if (ctx_addstatus( ctx, TMPLOC)) {
	/* over the connection limit */
	bttrace("peer_add error");
	return NULL;
    }


    /* initialize the peer structure */
    p = btcalloc(1, sizeof(struct btPeer));
    p->download=download;
    kBitSet_create(&p->blocks, dl->fileset.npieces);
    memcpy(p->id, id, IDSIZE);
    memcpy(p->extendedid, id, IDSIZE);
    p->sa = *sa;
    p->currentPiece = NULL;
    p->remote.choked = 1;
    p->local.choked = 1;
    p->last_exchange = btPeerCache_Create(NULL);
    
    /* connect to the peer */
    err = peer_connect_request( ctx, p);
    if (err < 0) {
	/* error connecting, clean up and return */
	ctx_delstatus( ctx, TMPLOC);
	bts_perror(errno, "peer_connect_request failed"); 
        btfree(p);
	bttrace("peer_add error");
        return NULL;
    }

    /* index the peer by peerid */
    idx = pset->len++;
    pset->peer = btrealloc( pset->peer, sizeof(struct btPeer*) * pset->len);
    pset->peer[ idx] = p;

    /* fix the statmap link from fd to status */
    ctx_fixtmp( ctx, p->ios.fd);

    /* check connect results */
    if (err) {
	if (err == 1) {
	    /* incomplete */
	    printf("%d: %s:%d : incomplete\n", p->ios.fd, inet_ntoa(p->sa.sin_addr), ntohs(p->sa.sin_port));
	    p->state=PEER_INIT;
	    ctx_setevents( ctx, p->ios.fd, POLLOUT);
	} else {
	    /* error connecting */
	    bts_perror(errno, "peer_connect failed");
	    p->state=PEER_ERROR;
	    p->local.unreachable=1;
	}
    } else {
	ctx_setevents( ctx, p->ios.fd, POLLIN);
	p->state=PEER_OUTGOING;
	if (peer_send_handshake( ctx, p)<0) {
	    p->state=PEER_ERROR;
	}
	if (peer_send_bitfield( ctx, p)<0) {
	    p->state=PEER_ERROR;
	}
    }

    bttrace("peer_add exit");
    return p;
}

int peer_send_handshake( btContext *ctx, btPeer *peer) {
    /*
    *    Handshake:
    *    \x13BitTorrent protocol\0\0\0\0\0\0\0\0<sha1 info hash><20byte peerid>
    */
    char shake[0x14] = "\x13" "BitTorrent protocol";
    char flags[8] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0 };
    // char flags[8] = { 0 };
    int err;

    bttrace("peer_send_handshake");
    assert(peer->download >= 0);
    err = kStream_fwrite( &peer->ios, shake, 0x14);
    if (err < 0) {
	bttrace("peer_send_handshake return -1");
    	return -1;
    }
    err = kStream_fwrite( &peer->ios, flags, 8);
    if (err < 0) {
	bttrace("peer_send_handshake return -2");
    	return -2;
    }
    err = kStream_fwrite( &peer->ios, ctx->downloads[peer->download]->infohash, SHA_DIGEST_LENGTH);
    if (err < 0) {
	bttrace("peer_send_handshake return -3");
    	return -3;
    }
    err = kStream_fwrite( &peer->ios, ctx->myid, IDSIZE);
    if (err < 0) {
	bttrace("peer_send_handshake return -4");
    	return -4;
    }
    bttrace("peer_send_handshake exit");
    return 0;
}

/*
 * check_handshake()
 *
 * Returns:
 *     0 - ok or incomplete
 *    -1 - error
 * Sets ios.error to:
 *    BTERR_PROTOCOL_ID if there is an error in the protocol identification
 *    BTERR_UNKNOWN_FLAGS if there the flags are set incorrectly
 *    BTERR_HASH_MISMATCH if the infohash doesn't match ours
 *
 * Sets peer->id to:
 *    remote id
 */
#define PROTO_LENGTH 0x14
#define FLAGS_LENGTH 0x08
static int check_handshake( btContext *ctx, btPeer *peer, char *shake, int len) 
{
    char *flags = shake + PROTO_LENGTH;
    char *infohash = flags + FLAGS_LENGTH;
    char *id = infohash + SHA_DIGEST_LENGTH;

    bttrace( "check_handshake");
    if (shake[0] != '\x13') {
	peer->ios.error = BTERR_PROTOCOL_ID;
	bttrace( "check_handshake error");
	return -1;
    }
    if (len >= PROTO_LENGTH 
	    && memcmp( shake, "\x13" "BitTorrent protocol", PROTO_LENGTH) != 0) 
    {
	/* bad protocol */
	peer->ios.error = BTERR_PROTOCOL_ID;
	bttrace( "check_handshake error");
	return -1;
    }
    if (len >= PROTO_LENGTH + FLAGS_LENGTH 
	    && memcmp( flags, "\0\0\0\0\0\0\0\0", FLAGS_LENGTH)!= 0) 
    {
		// check for PEX flag
		if (flags[5] == 0x10)
		{
			// This client supports extended messages
#if PEX_DEBUG
			fprintf(stderr, "Enabling extended messaging for client %s\n", inet_ntoa(peer->sa.sin_addr));
#endif
			peer->extended_protocol = 1;
		}
		else
		{/* bad flags */
			int i;
			fprintf(stderr, "Unknown flags in handshake: ");
			for (i=0; i<8; i++) 
			{
				fprintf(stderr, "%02x", flags[i]);
			}
			fprintf(stderr, "\n");
		}
	}
	
    if (len >= PROTO_LENGTH + FLAGS_LENGTH + SHA_DIGEST_LENGTH)
    {
        int i=INT_MAX;

	if(peer->state==PEER_INCOMING && peer->download==INT_MAX)
	{	/* Find the correct torrent */
	    for(i=0; i<ctx->downloadcount; i++)
	    {
		if (memcmp( infohash, ctx->downloads[i]->infohash, SHA_DIGEST_LENGTH)==0) 
		{
		    int idx;
		    btPeerset *pset = &ctx->downloads[i]->peerset;

		    /* index the peer by peerid */
		    idx = pset->len++;
		    pset->peer = btrealloc( pset->peer, sizeof(struct btPeer*) * pset->len);
		    pset->peer[ idx] = peer;

		    peer->download=i;

		    kBitSet_create(&peer->blocks, ctx->downloads[i]->fileset.npieces);
		    if (peer_send_handshake(ctx, peer) < 0) 
			{
				peer->state=PEER_ERROR;
				bttrace( "check_handshake error");
				return -1;
				}
			if (peer->extended_protocol)
			{
				sendExtendedHandshake(peer, ctx->listenport, ctx->downloads[i]->privateflag);
			}
			if (peer_send_bitfield(ctx, peer) < 0) 
			{
				peer->state=PEER_ERROR;
				bttrace( "check_handshake error");
				return -1;
			}
		    break;
		}
	    }
	}
	else
	{
	    i=peer->download;
	    if (memcmp( infohash, ctx->downloads[i]->infohash, SHA_DIGEST_LENGTH)!=0) {
	        i=INT_MAX;
	    }
	}
	if(i>=ctx->downloadcount)
	{
	    /* bad infohash from the peer */
	    peer->ios.error = BTERR_HASH_MISMATCH;
	    bttrace( "check_handshake error");
	    return -1;
	}
    }
    if (len >= PROTO_LENGTH + FLAGS_LENGTH + SHA_DIGEST_LENGTH + IDSIZE) {
	memcpy( peer->id, id, IDSIZE);
	memcpy(peer->extendedid, id, IDSIZE);
    }
    bttrace( "check_handshake exit");
    return 0;
}

/* 
 * recv_handshake()
 *
 * Returns 
 *   0 success
 *  -1 error
 *
 * peer->ios.error iff error return is set to one of
 *   EAGAIN - retry later
 *   some error code
 *
 * peer->state is set to one of
 *   PEER_ERROR - handshake failed
 *   PEER_CONNECT - handshake is still pending
 *   PEER_GOOD - handshake successful
 */
int recv_handshake( btContext *ctx, btPeer *peer) {
    char shake[PROTO_LENGTH + FLAGS_LENGTH + SHA_DIGEST_LENGTH + IDSIZE];
    int err;

    bttrace( "recv_handshake");
    err = kStream_fread( &peer->ios, shake, sizeof(shake));
    if (err < 0) {
	if (peer->ios.error == EAGAIN) {
	    /* can't get whole handshake; try for partial handshake */
	    int len = kStream_iqlen(&peer->ios);
	    if (len > 0) {
		err = kStream_fpeek( &peer->ios, shake, len);
		DIE_UNLESS(err == len);
		if (check_handshake( ctx, peer, shake, len)) {
		    peer->state = PEER_ERROR;
		    bttrace( "recv_handshake error");
		    return -1;
		}
	    }
	    peer->ios.error = EAGAIN;
	}
	bttrace( "recv_handshake error");
	return -1;
    }
    DIE_UNLESS(err == sizeof(shake));
    if (check_handshake( ctx, peer, shake, sizeof(shake))) {
	bttrace( "recv_handshake error");
	return -1;
    }
    peer->state = PEER_GOOD;
    ctx->downloads[peer->download]->peerset.incomplete++;
#if 0
    fprintf(stderr, "%d: got handshake for peer %s\n", peer->ios.fd, inet_ntoa(peer->sa.sin_addr));
#endif

	if (peer->extended_protocol)
	{
		// Tell this peer we support PEX
		if (sendExtendedHandshake(peer, ctx->listenport, ctx->downloads[peer->download]->privateflag) < 0)
		{
			bttrace( "check_handshake error");
			return -1;
		}
	}
	
    bttrace( "recv_handshake exit");
    return 0;
}

/*
 * queue_request()
 */
static int
queue_request( btRequestQueue *q, int piece, int offset, int len) {
    int oldtail = q->tail;
    int newtail = (oldtail+1) % QUEUESIZE;

    bttrace("queue_request");

    if (len > MAXREQUEST) return -1;
    if (newtail == q->head) return -2;
    q->req[oldtail].block = piece;
    q->req[oldtail].offset = offset;
    q->req[oldtail].length = len;
    q->tail = newtail;

    bttrace("queue_request exit");
    return 0;
}

static int 
clear_request_queue( btRequestQueue *q) {
    bttrace("clear_request_queue");
    q->head = q->tail;
    bttrace("clear_request_queue exit");
    return 0;
}

/* returns number of queued requests that were deleted (usually 1) */
static int 
remove_queued_request( btRequestQueue *q, int block, int offset, int len) {
    btRequest *r;
    int i = q->head;
    int offby=0;

    bttrace("remove_queued_request");
    while (i != q->tail) {
	r = &q->req[i];

        if (r->block == block &&
	    r->offset == offset &&
	    r->length == len) {
	    offby++;
	}
	if (offby) {
            q->req[i] = q->req[(i+offby)%QUEUESIZE];
	}
	i = (i+1) % QUEUESIZE;
    }
    q->tail = (q->tail + QUEUESIZE - offby) % QUEUESIZE;

    bttrace("remove_queued_request exit");
    return offby;
}

static btRequest *dequeue_request( btRequestQueue *q) {
    btRequest *req;
    int oldhead = q->head;
    int newhead = (oldhead+1) % QUEUESIZE;

    bttrace("dequeue_request");

    if (oldhead == q->tail) {
	bttrace("dequeue_request error");
    	return NULL;
    }
    req = &q->req[oldhead];
    q->head = newhead;
    bttrace("dequeue_request exit");
    return req;
}

static int
queue_len( btRequestQueue *q ) {
    int ct=q->tail - q->head;
    bttrace("queue_len");

    if (ct < 0) ct += QUEUESIZE;

    bttrace("queue_len exit");
    return ct;
}

static void start_rate_timer( btPeerStatus *ps, time_t now) {
    bttrace("start_rate_timer");
    if (ps->send_time == 0) {
	ps->send_time = now;
    }
    bttrace("start_rate_timer exit");
}

static void stop_rate_timer( btPeerStatus *ps, time_t now) {
    bttrace("stop_rate_timer");
    if (ps->send_time != 0) {
	ps->total_time += now - ps->send_time;
	ps->send_time = 0;
    }
    bttrace("stop_rate_timer exit");
}

int rate_timer( btPeerStatus *ps, time_t now) {
    int total;

    bttrace("rate_timer");

    total = ps->total_time;
    if (ps->send_time != 0) {
	total += now - ps->send_time;
    }
    if (total < 1) total = 1;

    bttrace("rate_timer exit");
    return total;
}

#if 0
static void shift_byte( void *ptr, int ival) {
    void **p = (void **)ptr;
    char *p1 = (char *)*p;

    bttrace("shift_byte");
    *p1++ = ival;
    *p = p1;
    bttrace("shift_byte exit");
}
#endif

/*
 * Return 1 if there are more messages waiting
 * Return 0 on success, 
 * return -1 on error  (peer->ios.error contains the error code)
 * return -2 on unknown message.
 */
int
recv_peermsg( btContext *ctx, btPeer *peer) {
    btDownload *dl = ctx->downloads[peer->download];
    int32_t nbo_len;
    int len;
    char msg[84];
    char *nmsg, *param;
    int res = 0;
    int err;
    int32_t nbo;

    bttrace("recv_peermsg");

    DIE_UNLESS(peer->download<ctx->downloadcount);

    err = kStream_fpeek( &peer->ios, (char *)&nbo_len, sizeof(nbo_len));
    if (err < 0) { 
	bttrace("recv_peermsg error");
    	return -1;
    }
    assert(err == sizeof(nbo_len));
    len = ntohl(nbo_len);
    if (len <= 80) {
	nmsg = msg;
    } else if (len > 0 && len <= MAXMESSAGE) {
	nmsg = btmalloc(len+sizeof(int32_t));
    } else {	/* Too big a packet, kill the peer. */
        peer->ios.error = BTERR_LARGEPACKET;
	bttrace("recv_peermsg error");
	return -1;
    }
#if 0
    printf("A/%d: looking for %d bytes %d available (addr %d)\n", peer->ios.fd, len+4, kStream_iqlen( &peer->ios), kStream_in_addr(&peer->ios));
#endif
    if (len == 0) {
	/* keep alive */
	err = kStream_fread( &peer->ios, (char *)&nbo_len, sizeof(nbo_len));
	DIE_UNLESS(err == sizeof(nbo_len));
	bttrace("recv_peermsg error");
	return 0;
    }
    DIE_UNLESS(len <= MAXMESSAGE && len >= 0);
    err = kStream_fread( &peer->ios, nmsg, len + sizeof(int32_t));
    if (err < 0) goto cleanup;
    DIE_UNLESS( err == len + (int)sizeof(int32_t));

#if 0
    printf("A/%d: got message %d (len %d)\n", peer->ios.fd, nmsg[4], len+sizeof(int32_t));
#endif

    /* got message */
    param = nmsg+5;
    switch (nmsg[4]) {
	case BT_MSG_CHOKE:
	    {	    
		if (peer->remote.choked == 0) {
		    int rst;
		    peer->remote.choked = 1;

                    if (peer->currentPiece) {
			btPartialPiece *piece = peer->currentPiece;
			/* cancel this peer's piece and reassign */
			bs_clr( &dl->requested, piece->piecenumber);
			rst = bs_firstClr( &piece->filled);
			if (rst >= 0) {
			    piece->nextbyteReq = rst;
			}
		    }
		    peer->currentPiece=NULL;
		    
		    clear_request_queue( &peer->inqueue);
		    if (peer->local.interested) {
			/* stop the rate counter */
			stop_rate_timer( &peer->remote, time(NULL));
		    }
		}
	    }
	    break;
	case BT_MSG_UNCHOKE:
	    time(&peer->lastreceived);
	    if (peer->remote.choked == 1) {
		peer->remote.choked = 0;
		/* queue requests */
		if (!peer->local.interested) {
		    /* recheck interest */
		    int interest = update_interested( ctx, peer);
		    if (!interest) break;
		}
		start_rate_timer( &peer->remote, time(NULL));
		if (peer_send_request( ctx, peer)) { res = -1; goto cleanup; }
	    }
	    break;
	case BT_MSG_INTERESTED:
	    peer->remote.interested = 1;
	    ctx->downloads[peer->download]->peerset.interestedpeers++;
	    if (!peer->local.choked) {
		start_rate_timer( &peer->local, time(NULL));
	    }
	    break;
	case BT_MSG_NOTINTERESTED:
	    peer->remote.interested = 0;
	    ctx->downloads[peer->download]->peerset.interestedpeers--;
	    if (!peer->local.choked) {
		stop_rate_timer( &peer->local, time(NULL));
	    }
	    break;
	case BT_MSG_HAVE:
	    {
		int block;
		int interest;

		UNSHIFT_INT32(param,nbo,block);
		bs_set( &peer->blocks, block);
		if (bs_isFull( &peer->blocks)) {
		    if (peer->remote.complete == 0) { 
			peer->remote.complete = 1;
			dl->peerset.incomplete--;
		    }
		}
		interest = update_interested( ctx, peer);
		if (interest && 
			!peer->remote.snubbed && 
			!peer->remote.choked)
		{
		    start_rate_timer( &peer->remote, time(NULL));
		    if (peer_send_request( ctx, peer)) { res = -1; goto cleanup; }
		}
	    }
	    break;
	case BT_MSG_BITFIELD:
	    kBitSet_readBytes( &peer->blocks, nmsg+5, len-1);
	    peer->gotbits = bs_countBits(&peer->blocks);
	    if (peer->blocks.nbits == peer->gotbits) {
	        if (peer->remote.complete == 0) { 
		    peer->remote.complete = 1;
                    dl->peerset.incomplete--;
		}
	    }
	    update_interested( ctx, peer);
	    break;
	case BT_MSG_REQUEST:
	    {
		int piece;
		int offs;
		int len;

		UNSHIFT_INT32(param,nbo,piece);
		UNSHIFT_INT32(param,nbo,offs);
		UNSHIFT_INT32(param,nbo,len);
	        if (peer->local.choked) {
		    /* ignore requests from choked peers */
		    break;
		}

		if (!bs_isSet(&peer->blocks, piece) && 
		      bs_isSet(&ctx->downloads[peer->download]->fileset.completed, piece)) {
		    res = queue_request( &peer->queue, piece, offs, len);
		    if (res) {
			peer->ios.error = BTERR_QUEUE_OVERFLOW;
		        goto cleanup;
		    }
		} else {
		    time_t now;
		    time(&now);
		    send_choke( peer, 1);
		    stop_rate_timer( &peer->local, now);
		    printf("%d: Choked by invalid request for block %d (%s have it)\n", peer->ios.fd, piece,
			bs_isSet(&peer->blocks, piece)?"they":"we don't");
		}
	    }
	    break;
	case BT_MSG_PIECE:
	    {

		btPartialPiece *pp=peer->currentPiece;
		int piece;
		int offs;

		UNSHIFT_INT32(param,nbo,piece);
		UNSHIFT_INT32(param,nbo,offs);
                
#if 0
		printf("%d: got piece %d, offs %d, len %d\n", 
			peer->ios.fd,
			piece,
			offs,
			len-9);
#endif
		/* check requests with ones we've sent */
		time(&peer->lastreceived);
		if (!pp || !remove_queued_request( &peer->inqueue, piece, offs, len-9)) {
		    printf("%d: Unneeded data: piece %d %d+%d\n", peer->ios.fd, piece, offs, len-9);
		} else {
		    int done = seg_writebuf( &dl->fileset, piece, offs, nmsg+13, len-9);
		    if (done < 0) {
			bts_perror(errno, "error writing buffer");
			abort();
		    }


		    if (done) {
			int i;
			bs_clr( &dl->interested, piece);
#if 0
			bs_dump( "completed", &dl->fileset.completed);
#endif
			for (i=0; i<dl->peerset.len; i++) {
			    btPeer *p = dl->peerset.peer[i];
			    if (p->currentPiece == pp) {
			        btRequest *req;

				p->currentPiece = NULL;
				/* send cancels, check that requests work */
				while ((req = dequeue_request( &p->inqueue)) != NULL) {
				    if (send_cancel(p, req->block, req->offset, 
					    req->length
					)) {
					p->state = PEER_ERROR;
					continue;
				    }
				}

				peer_send_request( ctx, p);
			    }
			    if (p->state == PEER_GOOD) {
				if (send_have( p, piece)) {
				    p->state = PEER_ERROR;
				    continue;
				}
			    }
			} /* for i in peerset.len */
		    } /* if done */
		    if (peer_send_request( ctx, peer)) { res = -1; goto cleanup; }
		} /* if !unknown piece */
	    }
	    break;
	case BT_MSG_CANCEL:
	    {
	        int piece, offs, len;
		/* this cancels a specific request */
		UNSHIFT_INT32(param,nbo,piece);
		UNSHIFT_INT32(param,nbo,offs);
		UNSHIFT_INT32(param,nbo,len);
		
#if 0
		printf("%d: got cancel %d, offs %d, len %d\n", 
			peer->ios.fd,
			piece,
			offs,
			len);
#endif
		remove_queued_request( &peer->queue, piece, offs, len);
	    }
	    break;
	    
	case BT_MSG_EXTENDED:
	    {
		btObject *messageDictionary, *flagsDictionary;
		btInteger *pexFlag;
		btStream *messageStream;
#if PEX_DEBUG
		// dump extended messages
		fprintf(stderr, "Got extended message %i from peer %s:\n",
			nmsg[5], 
			inet_ntoa(peer->sa.sin_addr));
		hexdump( nmsg+6, len-2);
#endif
		messageStream = bts_create_strstream(BTS_OUTPUT); // create output stream
		writebts(nmsg+6, len-2, 1, messageStream);
		if (bts_rewind(messageStream, BTS_INPUT)) DIE("bts_rewind");
		if (benc_get_object(messageStream, &messageDictionary)) DIE("bad response");
		bts_destroy(messageStream);

		if (nmsg[5] == 0)
		{
		    // message is a handshake
		    // Has peer sent listen port?
		    uint16_t peerlistenport = 0;
		    if ((peerlistenport = BTINTEGER(btObject_val(messageDictionary, "p"))->ival) > 0)
		    {
			peer->sa.sin_port = htons(peerlistenport);
		    }
		
						   
		    // Has peer sent clientid?
		    btString *s;
		    if ((s = (BTSTRING(btObject_val(messageDictionary, "v")))))
		    {
			strncpy(peer->extendedid, (char *)s->buf, 19);
		    }
		    // Does peer support ut_pex?
		    if ((flagsDictionary = btObject_val(messageDictionary, "m")))
		    {
			if ((pexFlag = BTINTEGER(btObject_val(flagsDictionary, "ut_pex"))))
			{
			    peer->pex_supported = pexFlag->ival;
			    time(&peer->pex_timer);
			    sendPeerExchange(ctx->downloads[peer->download], peer);
			}
			else
			{
			    peer->pex_supported = -1;
			}
		    }
		    
		}
		else // extended message
		{
		    // Check message matches peer's UT_PEX ID and message is a dict
		    if (nmsg[5] != peer->pex_supported || messageDictionary->t != BT_DICT)
		    {
			break;
		    }               
		    btresponse(ctx, peer->download, messageDictionary);
		}                       
		btObject_destroy(messageDictionary);
	    }
	    break;

        
	default:
	    /* unknown message */
	    res = -2;
	    break;
    }

cleanup:
    /* cleanup */
    if (err < 0) {
	/* if there has been an error, report it */
	res = -1;
    } 

    if (res == 0) {
	/* check if there is another message waiting */
	err = kStream_fpeek( &peer->ios, (char *)&nbo_len, sizeof(nbo_len));
	if (err == sizeof(nbo_len)) {
	    int tlen;
	    tlen = ntohl(nbo_len) + sizeof(nbo_len);
#if 0
	    printf("B/looking for %d bytes %d available (addr %d)\n", tlen, kStream_iqlen( &peer->ios), kStream_in_addr(&peer->ios));
#endif
            if (tlen < 0 || tlen > MAXMESSAGE) {
		/* out of sync with peer, or packet too large */
		peer->ios.error = BTERR_LARGEPACKET;
		bttrace("recv_peermsg error");
		return -1;
	    }
	    DIE_UNLESS(tlen <= MAXMESSAGE && tlen >= 0);
	    if (kStream_iqlen( &peer->ios) >= tlen) res = 1;
	}
    }

    if (len > 80) {
	btfree(nmsg);
    }
    bttrace("recv_peermsg exit");
    return res;
}


/* 
 * returns 
 *  0 - no error
 * -1 - permanent error sending msg size 
 * -2 - permanent error sending message
 */
int
send_message( btPeer *peer, char *buf, int len) {
    int nslen = htonl( len);

    bttrace("send_message");
#if 0
	printf("%d: send message %d len=%d addr=%d\n", peer->ios.fd, buf?buf[0]:-1, len, kStream_out_addr(&peer->ios));
#endif
    if (kStream_fwrite( &peer->ios, (void*)&nslen, sizeof(nslen)) < 0) {
	bttrace("send_message error");
	return -1;
    }
    if (len > 0) {
	if (kStream_fwrite( &peer->ios, buf, len) < 0) {
	    bttrace("send_message error");
	    return -2;
	}
    }
    bttrace("send_message exit");
    return 0;
}

int
send_keepalive( btPeer *peer) {
    int res;
    bttrace("send_keepalive entry");
    res = send_message( peer, NULL, 0);
    bttrace("send_keepalive exit");
    return res;
}

int
send_choke( btPeer *peer, int choke) {
    char type=choke?BT_MSG_CHOKE:BT_MSG_UNCHOKE;
    int res;

    bttrace("send_choke");

    if (peer->local.choked != choke) {
	/* cancel all requests */
	clear_request_queue( &peer->queue);
	peer->local.choked = choke;
    }

    res=send_message( peer, &type, 1);

    bttrace("send_choke exit");
    return res;
}

int
send_interested( btPeer *peer, int interest) {
    char type=interest?BT_MSG_INTERESTED:BT_MSG_NOTINTERESTED;
    int res;

    bttrace("send_interested");

    peer->local.interested = interest;
    res=send_message( peer, &type, 1);

    bttrace("send_interested exit");
    return res;
}


int
send_have( btPeer *peer, int piece) {
    char buf[5];
    char *p=buf;
    int32_t nbo;
    int res;

    bttrace("send_have");
    SHIFT_BYTE( p, BT_MSG_HAVE);
    SHIFT_INT32( p, nbo, piece);
    res = send_message( peer, buf, p-buf);

    bttrace("send_have exit");
    return res;
}

int 
send_bitfield( btPeer *peer, kBitSet *set) {
    int32_t nslen;
    char type=BT_MSG_BITFIELD;
    int res;
    int nbytes = (set->nbits + 7)/8;

    bttrace("send_bitfield");

    nslen = htonl( 1 + nbytes);
    res = kStream_fwrite( &peer->ios, (void *)&nslen, sizeof(nslen));
    if (res < 0) return -1;
    res = kStream_fwrite( &peer->ios, &type, 1);
    if (res < 0) return -2;
    res = kStream_fwrite( &peer->ios, set->bits, nbytes);
    if (res < 0) return -3;

    bttrace("send_bitfield exit");
    return res;
}

int 
send_request( btPeer *peer, int piece, int offs, int len) {
    char buf[13];
    char *p=buf;
    int32_t nbo;
    int res;

    bttrace("send_request");

    SHIFT_BYTE( p, BT_MSG_REQUEST);
    SHIFT_INT32( p, nbo, piece);
    SHIFT_INT32( p, nbo, offs);
    SHIFT_INT32( p, nbo, len);
#if 0
    printf("%d: send_request( ..., %d, %d, %d)\n", peer->ios.fd, piece, offs, len);
#endif
    res = send_message( peer, buf, p-buf);

    bttrace("send_request exit");
    return res;
}

int 
send_piece( btPeer *peer, int piece, int offs, char* cbuf, int len) {
    int tlen;
    int32_t nslen;
    char buf[9];
    char *p=buf;
    int res;
    int32_t nbo;

    bttrace("send_piece");

    SHIFT_BYTE( p, BT_MSG_PIECE);
    SHIFT_INT32( p, nbo, piece);
    SHIFT_INT32( p, nbo, offs);
    tlen = (p-buf) + len;
    nslen = htonl( tlen);
#if 0
    printf("%d: send message %d len=%d addr=%d\n", peer->ios.fd, buf[0], tlen, kStream_out_addr(&peer->ios));
#endif
    res = kStream_fwrite( &peer->ios, (void *)&nslen, sizeof(nslen));
    if (res < 0) return -1;
    res = kStream_fwrite( &peer->ios, buf, p-buf);
    if (res < 0) return -2;
    res = kStream_fwrite( &peer->ios, cbuf, len);
    if (res < 0) return -3;

    bttrace("send_piece exit");
    return res;
}

int 
process_queue( btFileSet *fs, btPeer *peer) {
    btRequest *req;
    int err;

    bttrace("process_queue");

    req = dequeue_request( &peer->queue);
    if (!req) {
	bttrace("process_queue exit");
	return 0;
    }

    /* send the requested block */
    err = seg_readbuf( fs, &fs->completed, req->block, req->offset, g_filebuffer, req->length);
    if (err < 0) {
	bttrace("process_queue error");
    	return err;
    }
    
    err = send_piece( peer, req->block, req->offset, g_filebuffer, req->length);
    if (err < 0) {
	bttrace("process_queue error");
	return err;
    }

    fs->ul += req->length;

    bttrace("process_queue exit");
    return 1;
}

int 
send_cancel( btPeer *peer, int piece, int offs, int len) {
    char buf[13];
    char *p=buf;
    int32_t nbo;
    int res;

    bttrace("send_cancel");
    SHIFT_BYTE( p, 8);
    SHIFT_INT32( p, nbo, piece);
    SHIFT_INT32( p, nbo, offs);
    SHIFT_INT32( p, nbo, len);
#if 0
    printf("%d: send_cancel (%d, %d + %d)\n", peer->ios.fd, piece, offs, len);
#endif
    res = send_message( peer, buf, p-buf);

    bttrace("send_cancel exit");
    return res;
}

#if 0
int peer_send_cancel( btPeer *peer) {
    for (i = 0; i < queue_size( &peer->iqueue); i++) {
        btRequest rq = queue_pop( &peer->iqueue);
	send_cancel( peer, rq->block, rq->offs, rq->len);
    }
}
#endif

int peer_answer( btContext *ctx, int sock) {
    /*struct btPeerset *pset = &ctx->peerset;*/
    struct btPeer *p;
    struct sockaddr_in sin;
    int sin_len;
    long flags;
#if 0
    struct hostent *hent;
#endif

    bttrace("peer_answer");

    /* allocate status */
    if (ctx_addstatus( ctx, sock)) {
	/* over the connection limit */
	close( sock);
	bttrace("peer_answer error");
        return -1;
    }

    /* initialize peer */
    p = btcalloc(1, sizeof(struct btPeer));

    /* get remote host address */
    sin_len = sizeof(struct sockaddr_in);
    getpeername( sock, (struct sockaddr*)&sin, &sin_len);
    
    memcpy(&p->sa, &sin, sizeof(struct sockaddr_in));
    p->sa.sin_port = 0;  /* inbound connection, not a valid connection port */
    printf("%d: New peer connected %s\n", sock, inet_ntoa(p->sa.sin_addr));
    p->currentPiece = NULL;
    p->remote.choked = 1;
    p->local.choked = 1;
    p->state = PEER_INCOMING;
    p->download = INT_MAX;
    kStream_create( &p->ios, sock);
    p->last_exchange = btPeerCache_Create(NULL);
    
#if 0
    idx = pset->len++;
    pset->peer = btrealloc( pset->peer, sizeof(struct btPeer*) * pset->len);
    /* index the peer by peerid */
    pset->peer[ idx] = p;
#endif
    /* index the peer by socket */
    ctx->sockpeer[sock]=p;

    /* change socket to non-blocking */
#if WIN32
    flags = ioctlsocket( sock, FIONBIO, (unsigned long *) 1);
    if (flags != 0) {
        bts_perror(errno, "ioctlsocket");
	bttrace("peer_answer error");
        return -1;
    }
#else
    flags = fcntl( sock, F_GETFL);
    if (flags < 0) {
        bts_perror(errno, "fcntl F_GETFL");
	bttrace("peer_answer error");
        return -1;
    }
    
    flags |= O_NONBLOCK;
    if ( fcntl( sock, F_SETFL, flags)) {
        bts_perror(errno, "fcntl F_SETFL");
	bttrace("peer_answer error");
        return -1;
    }
#endif

    ctx_setevents( ctx, sock, POLLIN);

    bttrace("peer_answer exit");
    return 0;
}

/* 
 * peer_process_queue()
 *
 * returns
 *    <0   Error in process_queue
 *    -1   Error writing message length
 *    -2   Error writing message params
 *    -3   Error writing bulk data
 *    =0   Request done, no bytes still waiting in output buffer
 *    1    Output buffer is still in progress
 */

int peer_process_queue( btFileSet *fs, btPeer *p) {
    int res;

    bttrace("peer_process_queue");
    if ( kStream_oqlen(&p->ios) <= 8096 ) {
	/* output buffer has drained, grab next request from queue */
	res = process_queue( fs, p);

	bttrace("peer_process_queue exit");
	return res;
    } else {
#if 0
	printf("%d: kStream_oqlen(&p->ios) = %d\n", p->ios.fd, kStream_oqlen(&p->ios));
#endif
	bttrace("peer_process_queue exit");
	return 0;
    }
}

void peer_shutdown( btContext *ctx, btPeer *peer, char *error) {
    int fd = peer->ios.fd;

    bttrace("peer_shutdown");
#if 0
    printf("%d: peer_shutdown()\n", fd);
#endif
    
    if(peer->currentPiece) {
        int rst;
	bs_clr( &ctx->downloads[peer->download]->requested, peer->currentPiece->piecenumber);
	rst = bs_firstClr( &peer->currentPiece->filled);
	if (rst >= 0) {
	    peer->currentPiece->nextbyteReq = rst;
	}
    }
    if (peer->ios.fd != -1 && ctx->statmap[peer->ios.fd] != -1) {
	ctx_delstatus( ctx, peer->ios.fd);
    }
    ctx->sockpeer[fd]=NULL;

    peer->state = PEER_ERROR;
    peer->local.unreachable = 1;
    peer->error = error;

    kStream_finit( &peer->ios);
    kBitSet_finit( &peer->blocks);

    btPeerCache_Destroy( peer->last_exchange);
    bttrace("peer_shutdown exit");
}    

/*
 * Returns 1 if there is another message waiting 
 * Returns 0 on success
 * Returns -1 on error
 * returns -2 on invalid message received
 * returns -3 on invalid state
 */
int 
peer_recv_message( btContext *ctx, btPeer *p) {
    int res=0;

    bttrace("peer_recv_message");
#if 0
    printf("%d: before message state %d\n", p->ios.fd, p->state);
#endif
    if (p->state == PEER_INIT) {
	/* socket isn't yet completely initialized */
	/* int stat = ctx->statmap[p->ios.fd]; */
	printf("Waiting for %d to complete connection.\n", p->ios.fd);
#if 0
	printf ("statblock %d, events %d, revents %d, fd %d\n", 
		stat, ctx->status[ stat].events, ctx->status[ stat].revents,
		ctx->status[ stat].fd);
#endif
	bttrace("peer_recv_message exit");
	return 0;
    } else if (p->state == PEER_OUTGOING || p->state == PEER_INCOMING) {
	res = recv_handshake( ctx, p);
	if (res==0) {
	    res = 1;  /* assume there is another message */
	    p->state = PEER_GOOD;
	}
    } else if (p->state == PEER_GOOD) {
	res = recv_peermsg( ctx, p);
    } else {
	printf("%d: Peer in unexpected state %d\n", p->ios.fd, p->state);
	res = -3;
    }
#if 0
    printf("%d: after message state %d\n", p->ios.fd, p->state);
#endif
    bttrace("peer_recv_message exit");
    return res;
}

int
update_interested( btContext *ctx, btPeer *p) {
    int interest;

    bttrace("update_interested");
    btDownload *dl=ctx->downloads[p->download];
    DIE_UNLESS (p->download<ctx->downloadcount);
    interest = bs_hasInteresting( &dl->fileset.completed, &p->blocks, &dl->interested);
    if (interest != p->local.interested) {
	if (send_interested( p, interest)) return -1;
	if (interest == 0 && !p->remote.choked) {
	    /* stop the rate counter */
	    stop_rate_timer( &p->remote, time(NULL));
	}
    } 
    bttrace("update_interested exit");
    return interest;
}

static int countpeers( btPeerset *peers, btPartialPiece *piece) {
    int count = 0;
    int i;

    bttrace("countpeers");
    for (i=0; i < peers->len; i++) {
        btPeer *p = peers->peer[i];
	if (p->currentPiece == piece) {
	    count ++;
	}
    }
    bttrace("countpeers exit");
    return count;
}


btPartialPiece * 
peer_assign_block( struct btContext *ctx, btPeer *p) {
    btPartialPiece *piece;
    btDownload *dl=ctx->downloads[p->download];

    bttrace("peer_assign_block");
    DIE_UNLESS (p->download<ctx->downloadcount);

    piece=dl->fileset.partial;
    if (dl->peerset.interestedpeers > 4 ||
        dl->peerset.incomplete <= 4 ||
        dl->peerset.len - dl->peerset.incomplete > 10) 
    {
        /* assign a partial block if no one else is working on it */
	while(piece && (bs_isSet(&dl->requested, piece->piecenumber) ||
	      !bs_isSet( &p->blocks, piece->piecenumber))) {
	    piece=piece->next;
	}
	if (piece) printf("%d: assigning partial block %d\n", p->ios.fd, piece->piecenumber);
    } else {
	/* use overlapping requests to get more interested peers (but not more than three on a block) */
	while(piece && !bs_isSet( &p->blocks, piece->piecenumber) && countpeers(&dl->peerset, piece)<4) {
	    piece=piece->next;
	}
	if (piece) printf("%d: assigning overlapping block %d\n", p->ios.fd, piece->piecenumber);
    }
    if(!piece) {
	int blk = -1;
	blk = bs_pickblock( &dl->requested, &p->blocks, &dl->interested);
	if (blk < 0) {
	    blk = bs_pickblock( &dl->fileset.completed, &p->blocks, &dl->interested);
	}
	if (blk < 0) return NULL;
	piece = seg_getPiece(&dl->fileset, blk);
    }
    p->currentPiece = piece;
    bs_set( &dl->requested, piece->piecenumber);
    bttrace("peer_assign_block exit");
    return piece;
}

#define INTERESTED_BONUS 2.0
#define CHOKED_PENALTY 0.75
#define NEWPEER_LEVEL 4000
#define OLDPEER_LEVEL 0
static int in_rate( btPeer *a, time_t now) {
    float atime, arate;
	int newpeer;

    bttrace("in_rate");
    atime = (float)rate_timer( &a->remote, now);
    arate = a->ios.read_count / atime;

    newpeer = rate_timer( &a->local, now);
    if (newpeer < 30 && a->remote.interested) {
	/* new peers start at 4k/s assumed rate for first 30s */
	arate = (float)NEWPEER_LEVEL;
    } else if (atime < 30) {
	/* if peer doesn't send reciprocate, then clamp */
	arate = (float)OLDPEER_LEVEL;
    }

    if (a->local.interested) {
	/* interested in this peer, double effective rate */
	arate *= (float)INTERESTED_BONUS;
    }
    if (a->remote.choked || !a->local.interested) {
	/* remote has us choked, or unchoked but we aren't interested */
	arate *= (float)CHOKED_PENALTY;
    }
    bttrace("in_rate exit");
    return (int)arate;
}

static int out_rate( btPeer *a, time_t now) {
    int atime, arate;

    bttrace("out_rate");

    atime = rate_timer( &a->local, now);
    arate = a->ios.write_count / atime;

    bttrace("out_rate exit");
    return arate;
}

int
peer_send_request( btContext *ctx, btPeer *p) {
    btPartialPiece *piece;
    int start;
    int len;
    int res=0;
    int blocklen;
    time_t now = time(NULL);
    int arate = in_rate(p, now);
    int qlen;
    btDownload *dl=ctx->downloads[p->download];
    bttrace("peer_send_request");

    DIE_UNLESS(p->download<ctx->downloadcount);

    /* Queue up to REQMAX outstanding requests */
    if ( p->currentPiece == NULL )
    {
	/* no assigned block, or block complete */
    	piece = peer_assign_block( ctx, p);
        if (!piece) {
            update_interested( ctx, p);
	    bttrace("peer_send_request exit");
            return 0;
        }
#if 0
        printf("new block assigned %d\n", p->currentRequest);
#endif
    } else {
        piece = p->currentPiece;
    }

    qlen = (15 * (arate / 1024)) / 16;	/* try to get 15s of requests queued */
    if (qlen > REQMAX) qlen = REQMAX;
    if (qlen < 2) qlen = 2;
    blocklen = seg_piecelen( &dl->fileset, piece->piecenumber);
    while (queue_len( &p->inqueue) < qlen) {
	start = piece->nextbyteReq;
	len = blocklen - start;
	if (len == 0) {
	    /* reached end of block */
            if (piece->isdone == 1) {
		/* bad block hash: restart the block */
		piece->nextbyteReq = 0;
	    } else {
                /* restart from unreceived */
                int rst;

		if (queue_len( &p->inqueue) > 0) {
		    bttrace("peer_send_request exit");
		    return 0;
		}
                rst = bs_firstClr( &piece->filled);
                if (rst < 0) {
		    bttrace("peer_send_request exit");
		    return 0;
		}
                piece->nextbyteReq = rst;
	    }
	    start = piece->nextbyteReq;
	    len = blocklen - start;

	    if (len == 0) {
		bttrace("peer_send_request exit");
	        return 0;
	    }
	}
	if (len > REQUEST_SIZE) {
	    /* limit length per request */
	    len = REQUEST_SIZE;
	}
	piece->nextbyteReq += len;
	res = send_request(p, piece->piecenumber, start, len);
	queue_request( &p->inqueue, piece->piecenumber, start, len);
    }
    bttrace("peer_send_request exit");
    return res;
}

int 
peer_send_bitfield( btContext *ctx, btPeer *peer) {
    int res;

    bttrace("peer_send_bitfield");
#if 0
    printf("peer_send_bitfield\n");
#endif
    res = send_bitfield( peer, &ctx->downloads[peer->download]->fileset.completed);
    
    bttrace("peer_send_bitfield exit");
    return res;
}

static int compare_rate( btContext *ctx, btPeer *a, btPeer *b, time_t now) {
    int arate, brate;
    int res = 0;

    bttrace("compare_rate");

    if (ctx->downloads[a->download]->fileset.left == 0) {
	arate = out_rate( a, now);
	brate = out_rate( b, now);
    } else {
	arate = in_rate( a, now);
	brate = in_rate( b, now);
    }
    if (arate > brate) res = -1;
    if (arate < brate) res = 1;

    bttrace("compare_rate exit");
    return res;
}

static void prioritize( btContext *ctx, btPeer *p[DOWNLOADS], btPeer *new, time_t now) {
    int i,j;
    
    bttrace("prioritize");
    for (i=0; i<DOWNLOADS; i++) {
	if (!new->local.snubbed && new->remote.interested) 
	{
	    if (p[i] == NULL || p[i]->remote.interested==0 || compare_rate( ctx, new, p[i], now) < 0) {
		for (j=DOWNLOADS-1; j>i; j--) {
		    p[j]=p[j-1];
		}
		p[i]=new;
		bttrace("prioritize exit");
		return;
	    }
	}
    }

    for (i=0; i<DOWNLOADS; i++) {
	if (p[i] == NULL || (p[i]->remote.interested==0 && compare_rate( ctx, new, p[i], now) < 0)) {
	    for (j=DOWNLOADS-1; j>i; j--) {
	    	p[j]=p[j-1];
	    }
	    p[i]=new;
	    bttrace("prioritize exit");
	    return;
	}
    }
    bttrace("prioritize exit");
}

static int isfavorite( btPeer *p[DOWNLOADS], btPeer *check) {
    int i;

    bttrace("isfavorite");
    for (i=0; i<DOWNLOADS; i++) {
	if (p[i] == check) {
	    bttrace("isfavorite exit");
	    return 1;
	}
    }
    bttrace("isfavorite exit");
    return 0;
}

void peer_favorites( btContext *ctx, btPeerset *pset) {
    int i=0;
    time_t now;
    btPeer *p[DOWNLOADS] = { NULL };

    bttrace("peer_favorites");

    time(&now);

    /* select the peers we will let download */
    for (i=0; i<pset->len; i++) {
	btPeer *peer = pset->peer[i];
        if (peer->state == PEER_GOOD) {
	    prioritize( ctx, p, peer, now);
	}
    }

    /* notify all peers if there is a change in their status */
    for (i=0; i<pset->len; i++) {
	btPeer *peer = pset->peer[i];
	if (isfavorite( p, peer)) {
	    if (peer->local.choked) {
		/* need to unchoke this peer */
		if (send_choke( peer, 0)) {
		    peer->state = PEER_ERROR;
		}
		if (peer->remote.interested) {
		    start_rate_timer( &peer->local, now);
		}
	    } 
	} else {
	    if (!peer->local.choked && peer->state == PEER_GOOD) {
		/* need to choke this peer */
		if (send_choke( peer, 1)) {
		    peer->state = PEER_ERROR;
		}
		stop_rate_timer( &peer->local, now);
	    }
	}
    }
    bttrace("peer_favorites exit");
}

void
peer_status_dump( btPeerStatus *ps, int bytes) {
    int total, rate;

    bttrace("peer_status_dump");
    total = rate_timer( ps, time(NULL));
    
    if (total == 0) total = 1;
    rate = bytes/total;
    
    printf("%c%c%c%c%5ds", 
	    ps->choked ? 'C':'c',
	    ps->interested ? 'I':'i',
	    ps->snubbed ? 'B':'b',
	    ps->unreachable ? 'R':'r',
	    total);
    if (rate >= 1000000) {
	printf("(%3dMbs)", rate / 1000000);
    } else if (rate >= 1000) {
	printf("(%3dkbs)", rate / 1000);
    } else {
	printf("(%3dbps)", rate);
    }
    bttrace("peer_status_dump exit");
}

void
peer_dump( btPeerset *pset) {
    int i;

    bttrace("peer_dump");

    for (i=0; i<pset->len; i++) {
	btPeer *p = pset->peer[i];
	if (!p || p->state == PEER_ERROR) continue;
        printf("%2d %15.15s:%-5d", p->ios.fd, inet_ntoa(p->sa.sin_addr), ntohs(p->sa.sin_port));
	if (p->state == PEER_INIT) {
	    printf("(INI)");
	} else if (p->state == PEER_OUTGOING) {
	    printf("(OUT)");
	} else if (p->state == PEER_INCOMING) {
	    printf("(INC)");
	} else if (p->state == PEER_ERROR) {
	    printf("(ERR)");
	} else {
	    int gotbits = bs_countBits( &p->blocks);
	    if (p->blocks.nbits == gotbits) {
		printf("(ALL)");
	    } else {
		printf("(%2d%%)", (gotbits * 100) / p->blocks.nbits);
	    }
	}
	printf("[");
	peer_status_dump( &p->remote, p->ios.read_count);
	printf("^%04d+%d][", p->currentPiece?p->currentPiece->piecenumber:-1, 
		queue_len( &p->inqueue));
	peer_status_dump( &p->local, p->ios.write_count);
	printf("_%d]\n", queue_len(&p->queue));
    }
    bttrace("peer_dump exit");
}

void 
peer_summary( btPeerset *pset) {
    int i;
    int npeers = 0;
    float rtime, ttime;
    float rbytes, tbytes;
    float rrate = 0, trate = 0;

    bttrace("peer_summary");

    for (i=0; i<pset->len; i++) {
	btPeer *p = pset->peer[i];
	if (p == NULL) continue;
	if (p->state == PEER_GOOD) npeers++;
	if (!p->remote.choked && p->local.interested) {
	    rtime = (float)rate_timer( &p->remote, time(NULL));
	    rbytes = (float)p->ios.read_count;
	    rrate += rbytes / rtime;
	}

	if (!p->local.choked && p->remote.interested) {
	    ttime = (float)rate_timer( &p->local, time(NULL));
	    tbytes = (float)p->ios.write_count;
	    trate += tbytes / ttime;
	}
    }
    
    printf("%d Peers, Download ", npeers);
    if (rrate >= 1000000) {
	printf("%.0fMbs", rrate / 1000000);
    } else if (rrate >= 1000) {
	printf("%.0fkbs", rrate / 1000);
    } else {
	printf("%.0fbps", rrate);
    }
    printf(" Upload ");
    if (trate >= 1000000) {
	printf("%.0fMbs", trate / 1000000);
    } else if (trate >= 1000) {
	printf("%.0fkbs", trate / 1000);
    } else {
	printf("%.0fbps", trate);
    }
    printf("\r");
    fflush(stdout);
    bttrace("peer_summary exit");
}

/*
 * returns 1 if all peers are seeds. else 0.
 */
int
peer_allcomplete( btPeerset *pset) {
  int i;

  bttrace("peer_allcomplete");
  for (i=0; i<pset->len; i++) {
    btPeer *p = pset->peer[i];
    if (p->state == PEER_ERROR)
      continue;
    if (p->state != PEER_GOOD) {
      /* Something in progress */
      bttrace("peer_allcomplete exit");
      return 0;
    }
    if (!bs_isFull (&p->blocks)) {
      bttrace("peer_allcomplete exit");
      return 0;
    }
  }
  bttrace("peer_allcomplete exit");
  return 1;
}
