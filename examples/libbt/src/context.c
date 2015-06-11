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

/* context.c */
#include "config.h"
#include "random.h"
#include "udpproto.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#if WIN32
#   include <winsock2.h>
#else
#   include <sys/param.h>
#   include <sys/socket.h>
#   include <unistd.h>
#   include <netdb.h>
#endif
#include <time.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "context.h"
#include "udpproto.h"
#include "bts.h"
#include "types.h"
#include "benc.h"
#include "random.h"
#include "peer.h"
#include "stream.h"
#include "util.h"
#include "segmenter.h"

#define MINPORT 6881
#define MAXPORT 6889
#define UNKNOWN_ID "--unknown--peer-id--"
#if WIN32
#   define snprintf _snprintf
#endif

#include "spa/spaRuntime.h"


static int parse_config_int( 
	const char *file, int lineno, const char *token, 
	int *value,
	const char *linebuf) 
{
    int tlen = strlen(token);
    const char *line;
    char *endp;
    int res=-1;
    if (strncmp( linebuf, token, tlen) == 0) {
	line = linebuf + tlen;
	line += strspn( line, " \t");
	*value = strtol( line, &endp, 10);

	if (*endp != 0) {
	    res = -1;
	    printf("In configuration file %s, line %d, the token %s failed to decode (%d)\n", file, lineno, token, res);
	    abort();
	} else {
	    res = 0;
	}
    }
    return res;
}

static int parse_config_digest( 
	const char *file, int lineno, const char *token, 
	unsigned char *digest, int len, 
	const char *linebuf) 
{
    int tlen = strlen(token);
    const char *line;
    int res=-1;
    if (strncmp( linebuf, token, tlen) == 0) {
	line = linebuf + tlen;
	line += strspn( line, " \t");
	res = hexdecode( digest, len, line, strlen(line));
	if (res) {
	    printf("In configuration file %s, line %d, the token %s failed to decode (%d)\n", file, lineno, token, res);
	    abort();
	}
    }
    return res;
}

void genconfig( btContext *ctx, char *rcfile) {
    char hexbuf[80];
    FILE *config;

    randomid( ctx->myid, IDSIZE);
    randomid( ctx->mykey, KEYSIZE);
    ctx->minport = MINPORT;
    ctx->maxport = MAXPORT;

    config=fopen( rcfile, "w");

    hexencode( ctx->myid, IDSIZE, hexbuf, sizeof(hexbuf));
    fprintf( config, "myid: %s\n", hexbuf);
    hexencode( ctx->mykey, KEYSIZE, hexbuf, sizeof(hexbuf));
    fprintf( config, "mykey: %s\n", hexbuf);
    fprintf( config, "minport: %d\n", ctx->minport);
    fprintf( config, "maxport: %d\n", ctx->maxport);
    fclose( config);
}

btContext *btContext_create( btContext *ctx, float ulfactor, char *rcfile) {
    FILE *config;
    char *env;
    char hexbuf[80];
    int lineno = 0;
    int i;

    if (!ctx) {
        ctx = btmalloc( sizeof(*ctx));
    }
    /* default initialize the entire context */
    memset( ctx, 0, sizeof(*ctx));
    /* initialize the socket to status map */
    for (i=0; i<SOCKID_MAX; i++) {
	ctx->statmap[i] = -1;
    }

    /* read defaults */
    if (!rcfile)
    {
#ifndef ENABLE_KLEE
	randomid( ctx->myid, IDSIZE);
	randomid( ctx->mykey, KEYSIZE);
	ctx->minport = MINPORT;
	ctx->maxport = MAXPORT;
#else
	//spa_api_input(ctx->myid, IDSIZE, "myid");
	//spa_api_input(ctx->mykey, KEYSIZE, "mykey");
	// spa_api_input(&(ctx->minport), sizeof(ctx->minport), "ctx_minport");
	// spa_api_input(&(ctx->maxport), sizeof(ctx->minport), "ctx_maxport");
	// spa_assume(ctx->minport > 0);
	// spa_assume(ctx->maxport == ctx->minport + 10);
	ctx->minport = MINPORT;
	ctx->maxport = MAXPORT;
#endif
    } else {
	config=fopen(rcfile, "r");
	if (!config) {
	    genconfig( ctx, rcfile);
	} else {
	    int got_id=0, got_key=1;
	    int got_minport=0, got_maxport=0;
	    randomid( ctx->mykey, KEYSIZE);

	    while (!feof(config)) {
		lineno++;
		fgets( hexbuf, sizeof(hexbuf), config);
		if (parse_config_digest( rcfile, lineno, "myid:", ctx->myid, IDSIZE, hexbuf)==0) {
		    got_id=1;
		}
		if (parse_config_digest( rcfile, lineno, "mykey:", ctx->mykey, KEYSIZE, hexbuf)==0) {
		    got_key=1;
		}
		if (parse_config_int( rcfile, lineno, "minport:", &ctx->minport, hexbuf)==0) {
		    got_minport=1;
		}
		if (parse_config_int( rcfile, lineno, "maxport:", &ctx->maxport, hexbuf)==0) {
		    got_maxport=1;
		}
	    } /* while */

	    env = getenv( "BTLIB_MINPORT");
	    if (env) {
		ctx->minport = atoi( env);
		got_minport = 1;
	    } 

	    env = getenv( "BTLIB_MAXPORT");
	    if (env) {
		ctx->maxport = atoi( env);
		got_maxport = 1;
	    }

	    if (!got_id || !got_key || !got_minport || !got_maxport) {
		fprintf( stderr, "%s configuration is corrupt, using defaults.\n", rcfile);
		genconfig( ctx, rcfile);
	    }

	    fclose(config);
	}
    }
    ctx->ulfactor = ulfactor;
    ctx->listenport = ctx->minport;
    return ctx;
}

void ctx_closedownload(btContext *ctx, unsigned download) {
  int i;
  btDownload *dl=ctx->downloads[download];
  btPeerset *pset = &dl->peerset;

  for (i=0; i<pset->len; i++) {
      if (pset->peer[i] != NULL) {
	  peer_shutdown (ctx, pset->peer[i], "exiting");
	  btfree(pset->peer[i]);
	  pset->peer[i] = NULL;
      }
  }
  btfree(pset->peer);
  pset->peer = NULL;

  for (i=0; i<dl->nurl; i++) {
     btfree (dl->url[i]);
  }
  btFileSet_destroy( &dl->fileset);
  kBitSet_finit( &dl->requested);
  kBitSet_finit( &dl->interested);
  if(dl->md)
    btObject_destroy( dl->md);
  ctx->downloads[download]=NULL;
  btfree(dl);
}

void btContext_destroy( btContext *ctx) {
    int i;

    for(i=0; i<ctx->downloadcount; i++) {
      if(ctx->downloads[i])
	ctx_closedownload(ctx, i);
    }

    /* Note: btfree(NULL) is a no-op */
    btfree (ctx->downloads);
    /* Shouldn't be necessary - one must recreate the context */
    ctx->downloads=NULL;
    ctx->downloadcount=0;
}

#ifdef ENABLE_SPA
static int isValidHex(char c) {
	return (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
}

#define SPA_HEX_BUFFER(pointer, len, name)					\
	do {													\
		int __i;											\
		pointer = btmalloc(3*len + 1);						\
		spa_api_input(pointer, 3*len+1, name);				\
		for (__i = 0; __i < len; __i++) {					\
			spa_assume(pointer[__i*3] == '%');				\
			spa_assume(isValidHex( pointer[__i*3 + 1]));	\
			spa_assume(isValidHex( pointer[__i*3 + 2]));	\
		}													\
	} while (0)

#endif

static char* hexdigest(unsigned char *digest, int len) {
    int i;
    char *buf = btmalloc(3*len+1);;
#ifndef ENABLE_KLEE
    for (i=0; i<len; i++) {
	sprintf(buf+3*i, "%%%02x", digest[i]);
    }
#else

    for (i=0; i<len; i++) {
    	buf[3*i] = '%';
    }
#endif

    return buf;
}

int
btresponse( btContext *ctx, int download, btObject *resp) {
    int ret = 0;
    btDownload *dl=ctx->downloads[download];
    btString *err;
    btPeer *p;
    int i,j, skip=0;
    struct sockaddr_in target;
    time_t now;
  
    if (!dl) 
    {
	/* fix for thread timing in interactive clients */
	fprintf(stderr, "Tracker response does not map to active torrent\n");
    }
	
    if (dl->peerset.len >= TORRENT_MAXCONN)
    {
	fprintf(stderr, "Skipping peers - max connections reached for this torrent\n");
	return ret;
    }
    
    if (!resp)
    {
	fprintf(stderr, "Error from tracker: null response\n");
	ret = -1;
    } else { 
	err = BTSTRING(btObject_val( resp, "failure reason"));
    }
    if (err) {
	printf("Error from tracker: %s\n", err->buf);
	ret = -1;
    } else {
        btObject *peersgen;

	peersgen = btObject_val( resp, "added");
	if (peersgen) {
	    /* if from peer */
#if 0
	    printf("Got new peer from PEX\n");
#endif
	} else {
	    /* if from tracker */
	    peersgen = btObject_val( resp, "peers");
	    btInteger *interval;
	    time(&now);
	    interval = BTINTEGER( btObject_val( resp, "interval"));
	    dl->reregister_interval = (int)interval->ival;
	    dl->reregister_timer = now;
	    printf("Interval %lld\n", interval->ival);
        } 

	if (peersgen->t == BT_LIST) {
	    btList *peers = BTLIST( peersgen);
	    if (peers->len == 0) { ret = -1; }
	    for (i=0; i<peers->len; i++) {
		btObject *o = peers->list[i];
		btString *peerid = BTSTRING( btObject_val( o, "peer id"));
		btString *ip = BTSTRING( btObject_val( o, "ip"));
		btInteger *port = BTINTEGER( btObject_val( o, "port"));
		int iport = (int)port->ival;
		const struct addrinfo ai_template={
		  .ai_family=AF_INET,	/* PF_INET */
		  .ai_socktype=SOCK_STREAM,
		  /*.ai_protocol=IPPROTO_TCP,*/
		};
		struct addrinfo *ai=NULL;

		skip=0;

		/* Detect collisions properly (incl. ourselves!) */
		if (memcmp(ctx->myid, peerid->buf, IDSIZE)==0) {
		    printf("Skipping myself %s:%d\n", ip->buf, iport);
		    continue;
		}
		
		for (j = 0; j < dl->peerset.len; j++) {
		    btPeer *p = dl->peerset.peer[j];
		    if ( memcmp( p->id, peerid->buf, IDSIZE)==0) {
			/* Simply tests by ID. */
			fprintf( stderr, 
				   "Skipping old peer: %s:%d", inet_ntoa(target.sin_addr), ntohs(target.sin_port));
			skip = 1;
			break;
		    }
		}
		if (skip) continue;

		printf( "Contacting peer %s:%d\n", ip->buf, iport);
		if(getaddrinfo(ip->buf, NULL, &ai_template, &ai)==0) {	
		    /* Just pick the first one, they are returned in varying order */
		    struct sockaddr_in sa;
		    if (ai->ai_addr->sa_family == AF_INET) {
			/*result of lookup is an IPV4 address */
			sa = *(struct sockaddr_in *)ai->ai_addr;
			sa.sin_port = htons(iport);
			p = peer_add( ctx, download, peerid->buf, &sa);
		    } else {
		        printf( "Peer lookup returned unsupported address type %d. \n", 
				ai->ai_addr->sa_family);
		    }
		}
		if(ai) {
		    freeaddrinfo(ai);
		}
	    } /* for each peer */
	} else if (peersgen->t == BT_STRING) {
	    btString *peers=BTSTRING(peersgen);

	    fprintf( stderr, "Parsing compact peer list (%d)\n", peers->len/6);
	    if (peers->len == 0) { ret = -1; }
	    for (i=0; i<=peers->len - 6; i += 6) {
		struct sockaddr_in target;

		skip = 0;

		/* Collect the address */
		target.sin_family=AF_INET;
		memcpy(&target.sin_addr, peers->buf+i, 4);
		memcpy(&target.sin_port, peers->buf+i+4, 2); /* already nbo */
		
		// Skip peers on port 0
		if (!ntohs(target.sin_port))
		{
		    fprintf(stderr, "Skipping peer %s on port 0", 
			    inet_ntoa(target.sin_addr));
		    continue;
		}
		
		if (peer_seen( ctx, download, &target)) continue;
		p = peer_add( ctx, download, UNKNOWN_ID, &target);
	    } /* for */
	
	}
/*	peer_dump( &ctx->peerset); */
    }
    return ret;
}

static btObject* btrequest( 
	char *announce,
	unsigned char myid[IDSIZE], 
	unsigned char mykey[KEYSIZE],
	unsigned char digest[SHA_DIGEST_LENGTH], 
	int port,
	_int64 downloaded, 
	_int64 uploaded,
	_int64 left,
	char *event) 
{
    /*
    * Tracker GET requests have the following keys urlencoded -
    *   req = {
    *REQUEST
    *      info_hash => 'hash'
    *      peer_id => 'random-20-character-name'
    *      port => '12345'
    *      ip => 'ip-address' -or- 'dns-name'  iff ip
    *      uploaded => '12345'
    *      downloaded => '12345'
    *      left => '12345'
    *
    *      last => last iff last
    *      trackerid => trackerid iff trackerid
    *      numwant => 0 iff howmany() >= maxpeers
    *      event => 'started', 'completed' -or- 'stopped' iff event != 'heartbeat'
    *
    *   }
    */

    /* contact tracker */
    CURL *hdl;
    char url[1024];
    char *dgurl;
    char *idurl;
    char *keyurl;
    btStream *io;
    btObject *result;
    int curlret;

#ifndef ENABLE_SPA
    dgurl=hexdigest(digest, SHA_DIGEST_LENGTH);
    idurl=hexdigest(myid, IDSIZE);
    keyurl=hexdigest(mykey, KEYSIZE);
#else
    spa_api_input_var(left);
    SPA_HEX_BUFFER(dgurl, SHA_DIGEST_LENGTH, "dgurl");
    SPA_HEX_BUFFER(idurl, IDSIZE, "idurl");
    SPA_HEX_BUFFER(keyurl, KEYSIZE, "keyurl");
#endif
#ifndef ENABLE_KLEE
    hdl = curl_easy_init();
    if (event) {
	snprintf( url, sizeof(url)-1, "%s?info_hash=%s&peer_id=%s&key=%s&port=%d&uploaded=%lld&downloaded=%lld&left=%lld&event=%s&compact=1", 
		announce,
		dgurl,
		idurl,
		keyurl,
		port,
		uploaded, 
		downloaded,
		left,
		event
	    );
    } else {
	snprintf( url, sizeof(url)-1, "%s?info_hash=%s&peer_id=%s&key=%s&port=%d&uploaded=%lld&downloaded=%lld&left=%lld&compact=1", 
		announce,
		dgurl,
		idurl,
		keyurl,
		port,
		uploaded, 
		downloaded,
		left
	    );
    }
#else
    //Pretend it is a full get request
    if (event) {
	snprintf( url, sizeof(url)-1, "GET /announce?info_hash=%s&peer_id=%s&key=%s&port=%d&uploaded=%lld&downloaded=%lld&left=%lld&event=%s&compact=1", 
		dgurl,
		idurl,
		keyurl,
		port,
		uploaded, 
		downloaded,
		left,
		event
	    );
    } else {
	snprintf( url, sizeof(url)-1, "GET /announce?info_hash=%s&peer_id=%s&key=%s&port=%d&uploaded=%lld&downloaded=%lld&left=%lld&compact=1", 
		dgurl,
		idurl,
		keyurl,
		port,
		uploaded, 
		downloaded,
		left
	    );
    }
#endif
    url[sizeof(url)-1]=0;
    spa_msg_output_var(url);
    spa_valid_path();

    btfree(idurl); btfree(dgurl); btfree(keyurl);
    curl_easy_setopt( hdl, CURLOPT_URL, url);
    io = bts_create_strstream( BTS_OUTPUT);
    curl_easy_setopt( hdl, CURLOPT_FILE, io);
    curl_easy_setopt( hdl, CURLOPT_WRITEFUNCTION, writebts);
    if ((curlret = curl_easy_perform( hdl)) != CURLE_OK)
    {
      spa_done();
      switch (curlret)
      {
      case CURLE_COULDNT_CONNECT:
	fprintf(stderr, "Failed to transfer URL: could not connect (%d)\n", curlret);
      default:
	fprintf(stderr, "Failed to transfer URL for reason %d (see curl.h)\n", curlret);
      }
      result=NULL;
    }
    else
    {
    	spa_done();
      /* parse the response */
      if (bts_rewind( io, BTS_INPUT)) DIE("bts_rewind");
      if (benc_get_object( io, &result)) DIE("bad response");
    }
    bts_destroy (io);
    curl_easy_cleanup( hdl);

    return result;
}

btObject* btannounce(
	btContext *ctx, btDownload *dl, char *state, int err)
{
    btObject *resp = NULL;
    int i;

    if (err) dl->tracker++;
    for (;dl->tracker < dl->nurl; dl->tracker++) {
        if (strncmp(dl->url[dl->tracker], "udp://", 6) == 0) {
	    int err;
	    if (!dl->cxid) {
		err = udp_connect( ctx, dl);
		dl->reregister_interval=60;
		if (err) {
		    printf("udp connect failed %d\n", err);
		    continue;
		}
	    } else {

	        err = udp_announce( ctx, dl, state);
		if (err) {
		    dl->cxid = 0;
		    printf("udp_announce failed %d\n", err);
		    continue;
		}
	    }
	    return NULL;
	} else {
	    resp = btrequest( 
		dl->url[dl->tracker], ctx->myid, ctx->mykey, dl->infohash, 
		ctx->listenport, dl->fileset.dl, 
		dl->fileset.ul * ctx->ulfactor, dl->fileset.left, 
		state
	    );
	}
	if (resp) break;
    }

    if (!resp) {
        dl->tracker=0;
	return NULL;
    }

    if (resp && dl->tracker != 0 && dl->tracker < dl->nurl) {
	btString *err = BTSTRING(btObject_val( resp, "failure reason"));
	if (!err) {
	    char *url = dl->url[dl->tracker];
	    for (i = dl->tracker; i>0; i--) {
		dl->url[i] = dl->url[i-1];
	    }
	    dl->url[0] = url;
	    dl->tracker=0;
	}
    }

#if 1
    if (resp) {
	btObject_dump( 0, resp);
    }
#endif
    return resp;
}

int ctx_addstatus( btContext *ctx, int fd) {
    int statblock;

    DIE_UNLESS(fd>=0 && fd<=SOCKID_MAX); /* include TMPLOC */

    /* allocate status bits */
    statblock = ctx->nstatus;
    if (statblock >= MAXCONN) {
	return -1;
    }
    ctx->nstatus++;

    ctx->statmap[ fd] = statblock;
    ctx->status[ statblock].fd = fd;
    ctx->status[ statblock].events = 0;;
    return 0;
}

void 
ctx_setevents( btContext *ctx, int fd, int events) {
    int statblock;

    DIE_UNLESS(fd>=0 && fd<SOCKID_MAX);

    statblock = ctx->statmap[ fd];
    ctx->status[ statblock].events |= events;
}

void 
ctx_clrevents( btContext *ctx, int fd, int events) {
    int statblock;

    DIE_UNLESS(fd>=0 && fd<SOCKID_MAX);

    statblock = ctx->statmap[ fd];
    ctx->status[ statblock].events &= ~events;
}

void
ctx_delstatus( btContext *ctx, int fd) {
    /* free up the status slot */
    int sid;
    int i;

    DIE_UNLESS(fd>=0 && fd<=SOCKID_MAX);

    sid = ctx->statmap[fd];
    for (i=sid; i<ctx->nstatus; i++) {
        ctx->status[i] = ctx->status[i+1];
    }
    ctx->nstatus--;
    for (i=0; i<SOCKID_MAX; i++) {
	if (ctx->statmap[i] > sid) ctx->statmap[i]--;
    }
}

void
ctx_fixtmp( btContext *ctx, int fd) {
    int statblock;
    
    DIE_UNLESS(fd>=0 && fd<SOCKID_MAX);

    statblock = ctx->statmap[ TMPLOC];

    DIE_UNLESS(statblock >= 0 && statblock < MAXCONN);
    DIE_UNLESS(ctx->status[ statblock].fd == TMPLOC);

    /* relink the status block to the statmap */
    ctx->statmap[ fd] = statblock;
    ctx->status[ statblock].fd = fd;
    ctx->statmap[ TMPLOC] = -1;
}

int ctx_register( struct btContext *ctx, unsigned download)
    /*
     * Contact the tracker and update it on our status.  Also
     * add any new peers that the tracker reports back to us.
     */
{
    btDownload *dl=ctx->downloads[download];
    btObject *resp;
    int nok=0;
    DIE_UNLESS(download<ctx->downloadcount);
    do {
	/* contact tracker */
	resp = btannounce( ctx, dl, "started", nok);
	if(!resp)
	    return -EAGAIN;
	nok = btresponse( ctx, download, resp);
	btObject_destroy( resp);
    } while (nok);
    return 0;
}

int ctx_startserver( btContext *ctx) {
    struct sockaddr_in sin;

    /* open server socket */
    ctx->ss = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP);
#if 0
    reuse = 1;
    if (setsockopt( ctx->ss, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
        bts_perror(errno, "setsockopt"); abort();
    }
#endif
    for ( ctx->listenport = ctx->minport;
          ctx->listenport <= ctx->maxport; 
          ctx->listenport++) 
    {
	sin.sin_family = AF_INET;
	sin.sin_port = htons( ctx->listenport);
	sin.sin_addr.s_addr = INADDR_ANY;
	if (!bind( ctx->ss, (struct sockaddr *)&sin, sizeof(sin))) { 
	    ctx->udpsock = udp_init( ctx->listenport);
	    if ( ctx->udpsock > 0) {
		break;
	    }
	}
    }
    if (ctx->listenport > ctx->maxport) {
        bts_perror(errno, "bind"); abort();
    }
    if (listen( ctx->ss, 10)) { bts_perror(errno, "listen"); abort(); }

    /* setup for select */
    ctx_addstatus( ctx, ctx->ss);
    ctx_setevents( ctx, ctx->ss, POLLIN);

    ctx_addstatus( ctx, ctx->udpsock);
    ctx_setevents( ctx, ctx->udpsock, POLLIN);
    return 0;    
}

int ctx_shutdown( btContext *ctx, unsigned download) {
    int result;
    btString *err;
    btObject *resp;
    btDownload *dl=ctx->downloads[download];

    DIE_UNLESS(download<ctx->downloadcount);
    /* contact tracker */
    resp = btannounce( ctx, dl, "stopped", 0);
#if 0
    btObject_dump( 0, resp);
#endif
    if(resp) {
	err = BTSTRING(btObject_val( resp, "failure reason"));
	if (err) {
	    printf("Error in shutdown from tracker: %s\n", err->buf);
	    result=-1;
	} else {
	    printf("Tracker shutdown complete\n");
	    result=0;
	}
	btObject_destroy( resp);
    } else {
#if 0
	/* TODO: check if UDP and don't emit error if so */
        printf("Failed to tell tracker we've stopped\n");
#endif
        result=-1;
    }
    return result; 
}

int ctx_complete( btContext *ctx, unsigned download) {
    btString *err;
    btObject *resp;
    btDownload *dl=ctx->downloads[download];

    DIE_UNLESS(download<ctx->downloadcount);

    dl->complete=1;
    if(dl->fileset.dl==0)	/* don't send complete if we seeded */
        return 0;

    /* contact tracker */
    resp = btannounce( ctx, dl, "completed", 0);
    if(!resp) {
        printf("Failed to tell tracker we completed\n");
	return -EAGAIN;
    }
#if 0
    btObject_dump( 0, resp);
#endif
    err = BTSTRING(btObject_val( resp, "failure reason"));
    if (err) {
	printf("Error in complete from tracker: %s\n", err->buf);
	btObject_destroy( resp);
	return -1;
    } else {
	printf("Tracker notified of complete\n");
    }
    btObject_destroy( resp);
    return 0; 
}

int ctx_reregister( btContext *ctx, unsigned download) {
    btObject *resp;
    int nok = 0;
    btDownload *dl=ctx->downloads[download];

    DIE_UNLESS(download<ctx->downloadcount);
    do {
	/* contact tracker */
	resp = btannounce( ctx, dl, NULL, nok);
	if(!resp)
	    return -EAGAIN;
#if 0
	btObject_dump( 0, resp);
#endif
	nok = btresponse( ctx, download, resp);
	btObject_destroy( resp);
    } while (nok);
    return 0; 
}

void ctx_exit( int exitcode, void *arg) {
    btContext *ctx=arg;
    int i=ctx->downloadcount;
    
    while (i) {
	ctx_writefastresume(ctx->downloads[--i]);
	ctx_shutdown( arg, i);
    }
    btContext_destroy(arg);
}

static int addurl( btDownload *dl, btString *a, int rank, char *ignorepattern) {
    if (!a) return 1;
    if (ignorepattern && strstr(a->buf, ignorepattern)) return 0;
    if (dl->nurl >= TRACKER_MAX) {
#ifndef NDEBUG
	DIE("Too many announce urls.");
#endif
    	return 0;
    }
    dl->url[dl->nurl] = btmalloc( a->len+1);
    memcpy( dl->url[dl->nurl], a->buf, a->len);
    dl->url[dl->nurl][a->len] = '\0';
    dl->urlrank[dl->nurl] = rank;
#if 0
    printf("url[%d]=%s\n", dl->nurl, dl->url[dl->nurl]);
#endif
    dl->nurl++;
    return 0;
}

static void shuffle( btDownload *dl) {
    int rank = 0;
    int start = 0;
    int count = 0;
    int i, pick;
    char *tmp;
    while (start < dl->nurl) {
        rank = dl->urlrank[ start];
        for (count = 0; dl->urlrank[ start+count] == rank && start+count < dl->nurl; count++);
	for (i=0; i<count; i++) {
	    pick = rnd(count);
	    tmp = dl->url[ start+pick];
	    dl->url[ start+pick] = dl->url[ start+i];
	    dl->url[ start+i] = tmp;
	}
	start += count;
    }
}

int ctx_loadfile( 
	btStream *bts, 
	struct btContext *ctx, 
	char *downloaddir, 
	int assumeok, 
	char *ignorepattern) 
{
    btStream *infostr;
    btString *announce;
    btList *announcelist;
    btInteger *size;
    struct btstrbuf strbuf;
    btString *hashdata;
    btInteger *piecelen;
    int npieces;
    int i, j, dlid;
    btDownload *dl;
    btInteger *isprivate;
    /* Allocate the new download */
    for(dlid=0; dlid<ctx->downloadcount; dlid++) {
	if (!ctx->downloads[dlid]) break;
    }

    if(dlid==ctx->downloadcount) {
	btDownload **l=btrealloc(ctx->downloads, sizeof(btDownload*)*++ctx->downloadcount);
	if(l==NULL) {
	    --ctx->downloadcount;
	    return -ENOMEM;
	}
	ctx->downloads=l;
    }
    dl=btcalloc(1, sizeof(btDownload));
    if(!dl) return -ENOMEM;

    /* load the metadata file */
    if (benc_get_object( bts, &dl->md)) {
      btfree(dl);
      return -EINVAL;
      /*DIE("Load metadata failed");*/
    }
    /* calculate infohash */
    infostr = bts_create_strstream( BTS_OUTPUT);
    benc_put_object( infostr, btObject_val( dl->md, "info"));
    strbuf = bts_get_buf( infostr);
#ifndef ENABLE_KLEE
    SHA1( strbuf.buf, strbuf.len, dl->infohash);
#else
    //memset(dl->infohash, 0, SHA_DIGEST_LENGTH);
#endif
    //spa_api_input(dl->infohash, SHA_DIGEST_LENGTH, "dl_infohash");
    bts_destroy( infostr);
    /* copy out url */
    announcelist = BTLIST( btObject_val( dl->md, "announce-list"));
    if (announcelist) {
        for (i = 0; i < announcelist->len; i++) {
	    btObject *o = btList_index( announcelist, i);
	    btString *a;
	    if (o->t == BT_LIST) {
	        btList *l = BTLIST( o);
	    	for (j = 0; j < l->len; j++) {
		    a = BTSTRING( btList_index( l, j));
		    addurl( dl, a, i, ignorepattern);
		}
	    } else if (o->t == BT_STRING) {
	        a = BTSTRING( o);
		addurl( dl, a, i, ignorepattern);
	    } else {
	        DIE( "Bad metadata: 'announce-list' is corrupt");
	    }
	}
	shuffle( dl);
    } else {
	announce = BTSTRING( btObject_val( dl->md, "announce"));
	if (addurl( dl, announce, 0, NULL)) DIE( "Bad metadata file: 'announce' missing.");
    } 
	
    // set private flag
    dl->privateflag = 0;
    isprivate = BTINTEGER(btObject_val( dl->md, "info/private"));
    if (isprivate) dl->privateflag = isprivate->ival;	
	
    /* set up the fileset and */
    /* calculate download size */
    dl->fileset.tsize = 0;
    size = BTINTEGER( btObject_val( dl->md, "info/length"));
    hashdata = BTSTRING( btObject_val( dl->md, "info/pieces"));
    piecelen = BTINTEGER( btObject_val( dl->md, "info/piece length"));
    npieces = hashdata->len / SHA_DIGEST_LENGTH;
    btFileSet_create( &dl->fileset, npieces, (int)piecelen->ival, hashdata->buf);
    kBitSet_create( &dl->requested, npieces);
    kBitSet_create( &dl->interested, npieces);
    for (i=0; i<npieces; i++) {
	bs_set( &dl->interested, i);
    }

    if (size) {
	/* single file mode */
	btString *file = BTSTRING( btObject_val( dl->md, "info/name"));
	dl->fileset.tsize=size->ival;
	char *unixPath = btmalloc(strlen(downloaddir)+file->len+2);
	sprintf(unixPath, "%s/%s", downloaddir, file->buf);
	btFileSet_addfile( &dl->fileset, unixPath, dl->fileset.tsize);
    } else {
	/* directory mode */
	btList *files;
	btFileSet *fs = &dl->fileset;
	btString *dir;

	dir = BTSTRING( btObject_val( dl->md, "info/name"));
	files = BTLIST( btObject_val( dl->md, "info/files"));
	if (!files) DIE( "Bad metadata file: no files.");
	for (i=0; i<files->len; i++) {
	    btInteger *fsize;
	    btList *filepath;
	    kStringBuffer path;

	    /* get file size */
	    fsize = BTINTEGER( btObject_val( files->list[i], "length"));
	    dl->fileset.tsize += fsize->ival;

	    /* get file path */
	    kStringBuffer_create( &path);
	    sbcat(&path, downloaddir, strlen(downloaddir));
	    sbputc( &path, '/');
	    sbcat( &path, dir->buf, dir->len);

	    filepath = BTLIST( btObject_val( files->list[i], "path"));

	    for (j=0; j<filepath->len; j++) {
	        btString *el = BTSTRING( filepath->list[j]);
		sbputc( &path, '/');
		sbcat( &path, el->buf, el->len);
	    }

	    /* add the file */
	    btFileSet_addfile( fs, path.buf, fsize->ival);

	    /* clean up */
	    kStringBuffer_finit( &path);
	}
    }

    dl->fileset.left = dl->fileset.tsize;
    ctx->downloads[dlid]=dl;
    return dlid;
}

int ctx_removetorrent(btContext *ctx, unsigned download)
{
    int i = download, j;
    btPeerset *pset;
    
    // remove from context
    ctx_closedownload(ctx, download);
    
    //Deincrement downloadcount and shift other downloads
    ctx->downloadcount--;
    ctx->downloads[download] = NULL;
    
    while (i<ctx->downloadcount)
    {   
        ctx->downloads[i] = ctx->downloads[i+1];
        pset = &ctx->downloads[i]->peerset;
        for (j=0; j<pset->len; j++)
        {
            pset->peer[j]->download = i;
        }
        i++; 
    }
    
    btDownload **l=btrealloc(ctx->downloads, sizeof(btDownload*)*ctx->downloadcount);
    if(l==NULL) 
    {
        return -ENOMEM;
    }
    ctx->downloads=l;
    
    return 0;   
    
}

#pragma mark -
#pragma mark Fastresume

int ctx_readfastresume(btDownload *dl, kBitSet *partialData)
{
    /* fast resume read - looks for resume data on disk matching 
    // torrent in current directory,
    // returns 0 if successfully loaded, -1 if not.
    */
    char *resumename, *torrentname;
    FILE *resumestream;    
    int res;
    
    //build resume data filename - use infohash eventually
    torrentname = ctx_stringfrommd(dl->md, "info/name");
    resumename = btmalloc(strlen(torrentname)+15);
    sprintf(resumename, "%s.fastresume", torrentname);
    printf("Loading fast resume data for %s\n", torrentname);
    if ((resumestream = fopen(resumename,"r")) != NULL)
    {  
        printf("Reading fast resume data...");
        //set up temp variables
        int bytes = (dl->fileset.completed.nbits+7)/8;
        char *encodedcomplete = btmalloc(bytes*2+1);
        unsigned char *decodedcomplete = btmalloc(bytes);
        // read file and decode
        fscanf(resumestream, "%s", encodedcomplete);
        res = ctx_hexdecode(decodedcomplete, bytes, encodedcomplete);
        if (!res)
        {
            // setup bitset based on fast resume data
            memcpy(partialData->bits, decodedcomplete, bytes);
            btfree(decodedcomplete);
            btfree(encodedcomplete);
            printf("done\n");
        }
        // delete old fast-resume data
        if (fclose(resumestream) == 0)
        {
            remove(resumename);
            btfree(resumename);
            return 0;
        } else {
            printf("Error removing fast resume data");
            return -1;
        }
    }
    return -1;
}

int ctx_hexdecode(unsigned char *digest, int len, const char *buf) 
{
    int i;
    const char *cpos = buf;
    for (i=0; i<len; i++) 
    {
        unsigned int hexval;
	int nextchar;
	if (sscanf(cpos, "%02x%n", &hexval, &nextchar) < 1) 
	{
	    return -1;
	}
	digest[i] = (unsigned char)hexval;
	cpos += nextchar;
    }
    if (*cpos != 0 && *cpos != '\n' && *cpos != '\r') return -2;
    return 0;
}

int ctx_writefastresume(btDownload *dl)
{
    // write resume
    FILE *resumestream;
    int i;
    char *torrentname;
    torrentname = ctx_stringfrommd(dl->md, "info/name");
    char *resumename = btmalloc(strlen(torrentname)+15);
    sprintf(resumename, "%s.fastresume", torrentname);

    /* build resume data filename - use infohash eventually  */
    /* char infohash[SHA_DIGEST_LENGTH]; */
    if ((resumestream = fopen(resumename,"w")) != NULL)
    {  
        printf("Writing fast resume data for %s...", torrentname);
        // hex encode completion data
        int bytes = (dl->fileset.completed.nbits+7)/8;
        char encodedcomplete[bytes*2+1], tempchar[2];
        for (i=0; i < bytes; i++)
        {
            if (i == 0) {
		sprintf(encodedcomplete, "%02x", 
		    (unsigned char)dl->fileset.completed.bits[i]);
	    } else {
                sprintf(tempchar, "%02x", 
                        (unsigned char)dl->fileset.completed.bits[i]);
                strcat(encodedcomplete, tempchar);
            }
        }
        fprintf(resumestream, "%s", encodedcomplete);
        fclose(resumestream);
        btfree(resumename);
        btfree(torrentname);
        printf("done\n");
        return 0;
    } else {
        printf("Failed to write fast resume data");
        return -1;
    }
}

int ctx_deletefastresume(btDownload *dl)
#warning remove in favour of util routine
// removes fast resume data when torrent completes
{
    char *torrentname;
    torrentname = ctx_stringfrommd(dl->md, "info/name");
    char *resumename = btmalloc(strlen(torrentname)+15);
    sprintf(resumename, "%s.fastresume", torrentname);
    if (!remove(resumename)) {return -1;} else {return 0;}
}

#pragma mark -
#pragma mark Hash checking


int ctx_hashpartialdata(btFileSet *templateFileSet, kBitSet *writeCache)
{
    int i, nextpiece, igood;
    //hash
    igood = i = 0;
    while (i<writeCache->nbits) 
    {
        nextpiece = (ctx_checkhashforpiece(templateFileSet, writeCache, i));
        i += (nextpiece > 0 ?nextpiece : 1);
        if (nextpiece == 0) igood++;

        if (i%10==0 || i > writeCache->nbits-5) 
        {
            printf("\r%d of %d completed (%d ok)", i, writeCache->nbits, igood);
            fflush(stdout);
        }
        
    }
    return igood;
}

int ctx_checkhashforpiece(btFileSet *templateFileSet, kBitSet *writeCache, int i)
{
    int ok, offset=-1;
    ok = seg_review(templateFileSet, writeCache, i);
    
    if (ok < 0) 
    {
        if (errno == ENOENT) 
        {
            int ni;
            btFile *f = seg_findFile(templateFileSet, i);
            if (!f) 
            {
                printf("couldn't find block %d\n", i);
                return 0;
            }
            ni = (int)((f->start + f->len) / templateFileSet->blocksize);
            if (ni > i) 
            {
                offset = ni;
#if 1
                printf("Skipping %d blocks at offset %d\n", offset, i);
#endif
            }
        }
#if 0
        bts_perror(errno, "seg_review");
#endif
    }
    return (ok < 0 ? offset : 0);
}

int ctx_writehashtodownload(btDownload *dl, kBitSet *partialData)
/* Writes partial hash data to download
* Only part of hash checking that directly accesses context, ie needs lock */
{
    int igood=0, i;
    
    for (i=0; i < dl->fileset.npieces; i++)
    {
        if (bs_isSet(partialData, i))
        {
            igood++;
            dl->fileset.left -= seg_piecelen( &dl->fileset, i);
            bs_set(&dl->fileset.completed, i);
            bs_set(&dl->requested, i);
            bs_clr(&dl->interested, i);
        }
    }
#ifndef ENABLE_KLEE    
    printf("\n");
    printf("Total good pieces %d (%d%%)\n", igood, igood * 100 / dl->fileset.npieces);
    printf("Total archive size %lld\n", dl->fileset.tsize);
    bs_dump( "completed", &dl->fileset.completed);
#endif
    return igood;
}

#pragma mark -
#pragma mark Utilities

char *ctx_stringfrommd(btObject *md, char *mdkey)
{
    btString *mdstring;
    mdstring = BTSTRING(btObject_val(md, mdkey));
    char *returnstring = btmalloc(mdstring->len);
    sprintf(returnstring, "%s", mdstring->buf);
    return returnstring;
}


