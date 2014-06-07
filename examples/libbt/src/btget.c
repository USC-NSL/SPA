/* 
 * Copyright 2003,2004,2005 Kevin Smathers, All Rights Reserved
 *
 * This file may be used or modified without the need for a license.
 *
 * Redistribution of this file in either its original form, or in an
 * updated form may be done under the terms of the GNU GENERAL
 * PUBLIC LICENSE.  If this license is unacceptable to you then you
 * may not redistribute this work.
 * 
 * See the file COPYING for details.
 */

/* btget.c */
#include "config.h"

#include "util.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#if !WIN32
	#include <sys/param.h>
#endif
#include <time.h>
#include <signal.h>
#include <locale.h>

/* inet sockets */
#include <sys/types.h>
#if WIN32
	#include <winsock2.h>
	#include "getopt.h"
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#if HAVE_UNISTD_H
		#include <unistd.h>
	#endif
	#include <netdb.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

/* libbt */
#include "bts.h"
#include "benc.h"
#include "random.h"
#include "peer.h"
#include "stream.h"
#include "segmenter.h"
#include "context.h"
#include "bterror.h"
#include "types.h"
#include "udpproto.h"
#include "peerexchange.h"

#if WIN32
	#define close(cs) closesocket(cs)
#endif

#define READABLE 1
#define EXECUTE 2

#define V(vers) #vers

/* globals */
btContext context;
int opthelp = 0;
int optverbose = 0;
float optexaggerate = 1.0;
int optseed = 0;
int optsnub = 1;
char *optignore = NULL;

/* functions */
int client_init( btContext *ctx, int cs) {
    return peer_answer( ctx, cs);
}



/*
 * Return 0 - no more work
 * Return 1 - more work to do
 * Return <0 - error
 */
int client_run( btContext *ctx, int cs, int cmd) {
    int res = 0;
    int err;
    btPeer *p = ctx->sockpeer[cs];

    /* check for new messages */
    if (p->state == PEER_ERROR) return -1;
    do {
	err = peer_recv_message( ctx, p);
    } while (err == 1);
#if 0
    printf("Still have %d bytes buffered\n", kStream_iqlen(&p->ios));
#endif
    if (err < 0 && p->ios.error != EAGAIN) return -1;

    /* do queue processing */
    if(p->download!=INT_MAX)
	err = peer_process_queue( &ctx->downloads[p->download]->fileset, p);
    if (err < 0 && p->ios.error != EAGAIN) return -1;
    if (err == 1 && cmd==EXECUTE) res = 1;
    return res;
}

/*
 * Returns 1 to indicate that more output is still queued
 * Returns 0 if the output buffer has drained, no more writing
 * Returns -1 on error
 */
int client_write( btContext *ctx, int cs) {
    /* writable player socket */
    int res = 0;
    int err;
    btPeer *p = ctx->sockpeer[cs];
    if (p->state == PEER_INIT) {
	/* unconnected socket */
	err = peer_connect_complete( ctx, p);
	if (err<0 && p->ios.error == EAGAIN) {
	    return 1;	/* keep waiting */
	}
	if (err) return -1;

	/* connection complete, add to read set */
	ctx_setevents( ctx, cs, POLLIN);
	peer_send_handshake( ctx, p);
	peer_send_bitfield( ctx, p);
	p->state = PEER_OUTGOING;
    } else {
	/* connected/good/etc. socket */
	err = kStream_flush( &p->ios);
	if (err < 0 && p->ios.error != EAGAIN) {
	    return -1;
	}
    }

    if (kStream_oqlen( &p->ios) > 0) {
	res = 1;
    }
    return res;
}

int client_finit( btContext *ctx, int cs) {
    int i, j, dl;
    char *err = "connection closed";
    btPeer *p = ctx->sockpeer[cs];
    if (p->ios.error != 0) err = bts_strerror( p->ios.error);

    dl=p->download;
    peer_shutdown( ctx, p, err);
    /* find where peer is */
    if(dl!=INT_MAX) {
        btPeerset *pset=&ctx->downloads[dl]->peerset;
	for (i=0; i < pset->len; i++) {
	    if (pset->peer[i] == p) {
		j = i;
		pset->peer[i] = NULL;
		break;
	    }
	}
	/* shift down the rest */
	for (i=j; i < pset->len-1; i++) {
	    pset->peer[i] = pset->peer[i+1];
	}
	pset->len--;
    }
    btfree(p);
    return 0;
}

void client_error( btContext *ctx, char *activity, int cs) {
    /* errors on a socket */
    printf( "%d: Peer %s shutdown %s (%s)\n", 
	    cs, inet_ntoa(ctx->sockpeer[cs]->sa.sin_addr),
	    activity,
	    bts_strerror(ctx->sockpeer[cs]->ios.error));
    client_finit( ctx, cs);
    close(cs);

    /* clear the execute bit if it was set */
    if (ctx->x_set[cs]) {
	ctx->xsock--;
	ctx->x_set[cs] = 0;
    }
}

#if 0
void
fdset_dump( fd_set fset) {
    int i;
    for (i=0; i<sizeof(fd_set)*8; i++) {
	if (FD_IS_SET( i, &fset)) {
	    putchar('1');
	} else {
	    putchar('0');
	}
    }
}
#endif

RETSIGTYPE sigint_handler( int signal) {
    exit(0);
}

#if !WIN32
void sigepipe_handler( int signal) {
    printf("Caught EPIPE for unknown file.\n");
}

void epipe_setup() {
    int err;
    struct sigaction sa = {};
    sa.sa_handler = sigepipe_handler;
    sigemptyset( &sa.sa_mask);
    sa.sa_flags = SA_RESETHAND | SA_RESTART;
    err = sigaction( SIGPIPE, &sa, NULL);
    if (err) SYSDIE("sigaction"); 
}
#endif

btStream *fetchtorrent( char *url) {
    btStream *io;
    int curlret;
    CURL *hdl = curl_easy_init();
    curl_easy_setopt( hdl, CURLOPT_URL, url);
    io = bts_create_strstream( BTS_OUTPUT);
    curl_easy_setopt( hdl, CURLOPT_FILE, io);
    curl_easy_setopt( hdl, CURLOPT_WRITEFUNCTION, writebts);

    if ((curlret = curl_easy_perform( hdl)) != CURLE_OK)
    {
      switch (curlret)
      {
      case CURLE_COULDNT_CONNECT:
        fprintf(stderr, "Failed to transfer URL: could not connect (%d)\n", curlret);
      default:
        fprintf(stderr, "Failed to transfer URL for reason %d (see curl.h)\n", curlret);
      }
      exit(1);
    }
    curl_easy_cleanup( hdl);
    return io;
}

int main( int argc, char **argv) {
    btStream *bts;
    struct btContext *ctx = &context;
    int cs;
    struct sockaddr csin;
    int err;
    time_t choke_timer;
    time_t report_timer;
    time_t now;
    int ttv;
    int tv_slow = 1; /*ms*/
    int tv_fast = 0; /*ms*/
    btPeer *peer;
    int opt;
    char *optfile = NULL;
    int optquiettmo = 0;
    int opturl = 0;
    int dl;
    int opttime = 0;

    /* initialize */
    time(&choke_timer);
    time(&report_timer);
#if WIN32
#else
    epipe_setup();
#endif
    signal( SIGINT, sigint_handler);
    signal( SIGTERM, sigint_handler);
    /* main */
    while ((opt = getopt( argc, argv, "bvhse:f:q:ui:t:")) != -1) {
	switch (opt) {
	    case 'b':
		optsnub = 0;
		break;
	    case 'v': 
		optverbose = 1; 
		break;
	    case 'h': 
		opthelp = 1; 
		break;
	    case 'f':
	        optfile = strdup(optarg);
		break;
	    case 'e':
	    	optexaggerate = atof(optarg);
		break;
	    case 'i':
	    	optignore = strdup(optarg);
		break;
	    case 's':
	    	optseed = 1;
		break;
	    case 'q':
	        optquiettmo = atoi(optarg);
		break;
	    case 'u':
	    	opturl = 1;
		break;
	    case 't':
	        opttime = atoi(optarg);
		break;
	    default:
		printf("Unknown option '%c'\n", optopt);
		opthelp = 1;
		break;
	}
    }

    if (optind >= argc || opthelp) {
	printf("Usage: btget [options] <torrent>...\n");
	printf("Version: %.2f\n", VERSION);
	printf("Options:\n");
	printf("  -h            This help message\n");
	printf("  -b            Disable snub detection\n");
	printf("  -f<filename>  Download file\n");
	printf("  -q<timeout>   Exit after no transfers for timeout secs\n");
	printf("  -u            Interpret <torrent> as a URL from which to fetch torrent\n");
	printf("  -i<pattern>   Ignore multi-tracker entries matching the pattern\n");
	printf("  -e<ratio>     Exaggerate upload speed [1.0]\n");
	printf("  -s            Seed the file (assume all blocks ok)\n");
	printf("  -v            Verbose (show all peers and transfer rates)\n");
    printf("  -t<interval>  Time between report intervals\n");
	exit(1);
    }

    btContext_create( ctx, optexaggerate, ".libbtrc");
    while(optind<argc) {
	if (opturl) {
	    bts = fetchtorrent( argv[optind]);
	} else {
	    bts = bts_create_filestream( argv[optind], BTS_INPUT);
	}
    
	/* load tracker file */
	dl=ctx_loadfile( bts, ctx, ".", optseed, optignore);
	if (dl < 0) {
	    bts_perror( errno, "Unable to load .torrent");
	    optind++;
	    continue;
	}
	
	bts_destroy( bts);
	
	// Check hash
	kBitSet partialData;
	kBitSet_create(&partialData, ctx->downloads[dl]->fileset.npieces);
	
	int igood = ctx_readfastresume(ctx->downloads[dl], &partialData);
	if (igood < 0)
	{    
	    printf("\rNo fast resume data for torrent - hash checking...\n");
	    ctx_hashpartialdata(&ctx->downloads[dl]->fileset, &partialData);
	}
	ctx_writehashtodownload(ctx->downloads[dl], &partialData);
	kBitSet_finit(&partialData);
 
	if (optfile) {
	    seg_markFile( &ctx->downloads[dl]->fileset, optfile, &ctx->downloads[dl]->interested);
	}
	optind++;
    }

    /* server listener */
    ctx_startserver( ctx);

    /* initial contact with the tracker */
    for(dl=0; dl<ctx->downloadcount; dl++) {
        if (ctx->downloads[dl]) {
	    ctx_register( ctx, dl);
	}
    }

    on_exit( ctx_exit, ctx);

    printf("%d: Server ready...\n", ctx->ss);
    for (;;) {
        int i;
	int readerr;
	int writeerr;
	int execerr;
	int pollerr;
	int sa_len;
	int complt;

        /*
	 * Select a socket or time out
	 */
	if (ctx->xsock) {
	    ttv = tv_fast;
	} else {
	    ttv = tv_slow;
	}
	/* sleep(1); */
        err = poll( ctx->status, ctx->nstatus, ttv);
#if 0
	if (err == EINTR) {
	    /* caught a signal */
	    continue;
	}
#endif
	if (err < 0) { bts_perror(errno, "poll"); abort(); }
#if 0
	printf("poll returned %d (xsock=%d)\n", err, ctx->xsock);
#endif
	time( &now);

	for (cs=0; cs < SOCKID_MAX; cs++) {
	    /* loop through executable sockets */
            if (ctx->x_set[ cs]) {
		btPeer *p = ctx->sockpeer[cs];
#if 0
	        printf("executable %d\n", cs);
#endif
		execerr = client_run( ctx, cs, EXECUTE);
		if (execerr == 0) {
		    if ( ctx->x_set[ cs]) {
			ctx->x_set[ cs] = 0;
			ctx->xsock--;
		    }
		}
		
		if ( kStream_oqlen( &p->ios)) {
		    /* data is waiting on the output buffer */
#if 0
		    printf("%d: %d bytes queued for output\n",
			    cs, kStream_oqlen( &p->ios));
#endif
                    ctx_setevents( ctx, cs, POLLOUT);
		}

		if (execerr < 0) {
		    client_error( ctx, "execute", cs);
		} /* if */
	    } /* if */
	} /* for */


        for (i=0; i<ctx->nstatus; i++) {
	    /* for each poll event */
	    cs = ctx->status[i].fd;

            if (ctx->statmap[cs] != i) {
		printf("Map synch error stat=%d -> socket=%d -> stat=%d\n",
			i, cs, ctx->statmap[cs]);
	    }
	    DIE_UNLESS( ctx->statmap[cs] == i);
	    readerr=0;
	    writeerr=0;
	    execerr=0;
	    pollerr=0;

	    if (CTX_STATUS_REVENTS( ctx, i) & POLLIN) {
		DIE_UNLESS( ctx->status[i].events & POLLIN);
#if 0
	        printf("readable %d\n", cs);
#endif
		/* readable */
	        if (cs == ctx->ss) {
		    /* service socket */
		    sa_len = sizeof(csin);
		    cs = accept( ctx->ss, &csin, &sa_len);
#if 0
		    printf("%d: Client connect on %d\n", cs, ctx->listenport);
#endif
		    if (cs < 0) {
			bts_perror( errno, "accept");
		    } else {
			client_init( ctx, cs);
		    } /* if */
		} else if (cs == ctx->udpsock) {
		    printf("--UDP Ready\n");
		    int err = udp_ready( ctx);
		    if (err) {
			printf("Error %d processing UDP packet.\n", err);
		    }
		} else {
		    /* client socket */
		    btPeer *p = ctx->sockpeer[ cs];
#if 0
		    printf("peer state is %d\n", p->state);
#endif
		    readerr = client_run( ctx, cs, READABLE);
		    if (readerr == 1) {
			/* more to read */
			if (!ctx->x_set[cs]) {
			    ctx->x_set[cs] = 1;
			    ctx->xsock++;
			}
		    }
		    if ( kStream_oqlen( &p->ios)) {
		        /* data is waiting on the output buffer */
#if 0
			printf("%d: %d bytes queued for output\n",
				cs, kStream_oqlen( &p->ios));
#endif
			ctx_setevents( ctx, cs, POLLOUT);
		    }
		}
	    } /* if */

	    if (CTX_STATUS_REVENTS( ctx, i) & POLLOUT) {
#if 0
	        printf("writeable %d\n", cs);
#endif
		writeerr = client_write( ctx, cs);
#if 0
		printf("%d: %d bytes queued for output\n",
			cs, kStream_oqlen( &ctx->sockpeer[cs]->ios));
#endif
		if (writeerr == 0) {
		    /* output drained */
		    ctx_clrevents( ctx, cs, POLLOUT);
		    if (!ctx->x_set[ cs]) {
			/* output buffer is empty, check for more work */
			ctx->x_set[ cs] = 1;
			ctx->xsock++;
		    }
		}
	    } /* if */

	    if (CTX_STATUS_REVENTS( ctx, i) & (POLLERR | POLLHUP | POLLNVAL)) 
	    {
	        int events = CTX_STATUS_REVENTS( ctx, i);
		if (events & POLLHUP) {
		    ctx->sockpeer[cs]->ios.error = BTERR_POLLHUP;
		} else if (events & POLLERR) {
		    ctx->sockpeer[cs]->ios.error = BTERR_POLLERR;
		} else if (events & POLLNVAL) {
		    ctx->sockpeer[cs]->ios.error = BTERR_POLLNVAL;
		}
		pollerr = -1;
	    }

	    if (readerr < 0 || writeerr < 0 || execerr < 0 || pollerr < 0) {
	        char *act = NULL;
		if (readerr<0) act = "read";
		if (writeerr<0) act = "write";
		if (execerr<0) act = "execute";
		if (pollerr<0) act = "poll";
		client_error( ctx, act, cs);
	    } /* if */

	    peer = ctx->sockpeer[cs];
	    if ( optsnub && peer && 
		    !peer->remote.choked && 
		    peer->local.interested &&
		    !peer->local.snubbed &&
		    now - peer->lastreceived > 120) 
	    {
		printf("%d: Peer unchoked and interesting but not sending data for %ld seconds\n", 
			peer->ios.fd, (long)(now - peer->lastreceived));
		peer->local.snubbed = 1;
	    }
	    
	    if (peer &&
		    peer->pex_supported > 0 &&
		    peer->pex_timer > 0 &&
		    now - peer->pex_timer >= 60)
	    {
		sendPeerExchange(ctx->downloads[peer->download], peer);
	    }

	    
	} /* for */


	for(dl=0; dl<ctx->downloadcount; dl++) 
	{
	    // Do reregister processing
	    if (!ctx->downloads[dl]) continue;
	    
	    if (ctx->downloads[dl]->reregister_interval != 0 &&
		    now - ctx->downloads[dl]->reregister_timer > ctx->downloads[dl]->reregister_interval) 
	    {
		ctx->downloads[dl]->reregister_timer = now;
		ctx_reregister( ctx, dl);
	    }
	}


	if (now - report_timer > opttime) {
	    /* show connection status */
	    report_timer=now;
	    printf( "Time %ld\n", (long)now);
	    for(dl=0; dl<ctx->downloadcount; dl++) {
	      if (!ctx->downloads[dl]) continue;
	      complt = bs_countBits( &ctx->downloads[dl]->fileset.completed);
	      if ((complt == ctx->downloads[dl]->fileset.npieces) &&
		  !ctx->downloads[dl]->complete) {
		  /* close all files, only once */
		  int i;
		  ctx_complete (ctx, dl);
		  cacheclose();
		  for (i=0; i<ctx->downloads[dl]->peerset.len; i++) {
		      if (ctx->downloads[dl]->peerset.peer[i] == NULL) continue;
		      /* All blocks should've been cancelled during reception,
		       * completion is filled in as we write to disk. */
		      DIE_UNLESS (ctx->downloads[dl]->peerset.peer[i]->currentPiece == NULL);
		  }
	      }
	      if (optverbose) {
		  peer_dump( &ctx->downloads[dl]->peerset);
	      }
	      printf("%d%% (%d of %d) ", 
		      complt * 100 / ctx->downloads[dl]->fileset.npieces,
		      complt, ctx->downloads[dl]->fileset.npieces);
	      peer_summary( &ctx->downloads[dl]->peerset);
	      if (optverbose) {
		  printf("\n----\n");
	      } else {
		  printf("%79s\r", "");
	      }
	    }
	}

	if (now - choke_timer > 30) {
	    /* recalculate favorite peers */
	    choke_timer=now;
	    /* should this be done for all torrents at the same time? */
	    for(dl=0; dl<ctx->downloadcount; dl++) {
	        if (!ctx->downloads[dl]) continue;
		peer_favorites( ctx, &ctx->downloads[dl]->peerset);
	    }
	}
 
#if 0
	if (optquiettmo > 0) {
	    static int lastcheck = 0; 
	    static int donetime  = 0;
	    int check = (ctx->complete == 1) && peer_allcomplete( &ctx->downloads[0]->peerset);
	    if (check) {
	        if (!lastcheck) {
		    printf("=== All peers done!\n");
		    donetime = now;
		} else {
		    if (now - donetime > optquiettmo) {
			printf("Peers have been quiescent for %ld secs, exiting.\n", (now - donetime));
			exit (0);
		    }
		}
	    }
	    lastcheck = check;
	}
#endif
    }

    return 0;
}


