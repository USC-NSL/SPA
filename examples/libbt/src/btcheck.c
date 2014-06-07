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

/* btcheck.c */

#include "config.h"

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
	#if HAVE_IO_H
		#include <io.h>
	#endif
	#if HAVE_UNISTD_H
		#include <unistd.h>
	#endif
#endif
#include <time.h>
#include <signal.h>

/* libbt */
#include "types.h"
#include "benc.h"
#include "util.h"
#include "segmenter.h"
#include "context.h"
#include "bitset.h"

/* globals */
btContext context;

/* functions */
int main(int argc, char **argv)
{
	int result=0;
	int dlid;
	char *filename;
	btStream *bts;
	struct btContext *ctx = &context;

	/* main */
	if(argc!=2) goto usage;
	if(argv[1][0]=='-') goto usage;

	filename=argv[1];

	if(!(bts=bts_create_filestream(filename, BTS_INPUT))) {
		perror(filename);
		return 1;
	}
	/* load tracker file */
	if((dlid=ctx_loadfile(bts, ctx, ".", 0, NULL))<0) {
		errno=-dlid; /* ctx_loadfile() returns the error */
		perror(filename);
		return 1;
	}

	// Check hash
	kBitSet partialData;
	kBitSet_create(&partialData, ctx->downloads[dlid]->fileset.npieces);
	ctx_writehashtodownload(ctx->downloads[dlid], &partialData);
	kBitSet_finit(&partialData);
 
	result=!bs_isFull(&ctx->downloads[dlid]->fileset.completed);
	ctx_closedownload(ctx, dlid);
	bts_destroy(bts); 

	btContext_destroy(ctx);
	return result;

usage:
	fprintf(stderr, "Usage: %s torrentfile\n", argv[0]);
	fprintf(stderr, "Version: %.2f\n", VERSION);
	return 1;
}

