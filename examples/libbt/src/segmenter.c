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

#include "config.h"

#include "util.h"
#include <stdlib.h>
#include <openssl/sha.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#if !WIN32
#   if HAVE_UNISTD_H
#      define __USE_LARGEFILE64
#      include <unistd.h>
#   endif
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#include <stdio.h>
#include "types.h"

#include "segmenter.h"
#include "bitset.h"
#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#if WIN32
#   include <io.h>
#   define lseek64 _lseeki64
#   define lseek _lseek
#endif

static void
pp_addtail( btFileSet *fs, btPartialPiece *pp) {
    btPartialPiece *pos, **last;
    last = &fs->partial;
    for (pos = fs->partial ; pos; pos = pos->next) {
	last = &pos->next;
    }
    *last = pp;
    pp->next = NULL;
}

btPartialPiece *
seg_getPiece( btFileSet *fs, int piece) {
    btPartialPiece *p;

    DIE_UNLESS(piece>=0 && piece<fs->npieces);
    p=fs->partial;
    while(p && p->piecenumber!=piece) {
	p=p->next;
    }
    if(!p) {
	int blocksize=seg_piecelen(fs, piece);
        /* Allocation trick: the memory after the structure
	 * is the piece contents */
        p=btcalloc(1, sizeof(btPartialPiece)+blocksize);
	p->piecenumber=piece;
	kBitSet_create( &p->filled, blocksize);
	pp_addtail( fs, p);
    }
    return p;
}

int 
seg_piecelen( btFileSet *fs, int piece) {
    int blocklen;
    blocklen = fs->blocksize;
    if (piece == fs->npieces-1) {
	/* last block may be short */
	_int64 modulus = fs->tsize % (_int64)fs->blocksize;
	if (modulus) blocklen = (int)modulus;
    }
    DIE_UNLESS(piece>=0 && piece<fs->npieces);
    return blocklen;
}

btFile *
seg_findFile( btFileSet *fs, int piece) {
    int ifile;
    btFile *f;
    _int64 addr = (_int64)piece * fs->blocksize;
    for (ifile=0; ifile < fs->nfiles; ifile++) {
	int blocklen = seg_piecelen( fs, piece);
	f=fs->file[ifile];
	if ( f->start+f->len >= addr &&
		f->start < addr + blocklen) 
	{
	    /* this file (partly) includes this piece */
	    return f;
	}
    }
    abort();
}

void
seg_markFile( btFileSet *fs, char *filename, kBitSet *interest) {
    btFile *f;
    int ifile;
    int i;

    /* not interested in anything yet */
    for (i = 0; i<interest->nbits; i++) {
	bs_clr( interest, i);
    }

    /* find the file to be downloaded, and mark interest in it */
    for (ifile = 0; ifile < fs->nfiles; ifile++) {
        int startblock, endblock;

	f = fs->file[ifile];
	startblock = (int)(f->start / (_int64)fs->blocksize);
	endblock = (int)((f->start + f->len - 1) / (_int64)fs->blocksize) + 1;

        if (strstr(f->path, filename)!=NULL) {
	    int i;
	    for (i=startblock; i<endblock; i++) {
		bs_set( interest, i);
	    }
	}
    } 
}

/*
 * Check the hash of the piece against the list of hashes
 * Return 1 if the hash matches.
 * Return 0 if the hash doesn't match
 */

static int
_checkhash( btFileSet *fs, int piece, char *buf) {
    int r;
    unsigned char digest[SHA_DIGEST_LENGTH];
    int len = seg_piecelen( fs, piece);

    SHA1( buf, len, digest);
    r = memcmp( digest, &fs->hashes[piece*SHA_DIGEST_LENGTH], SHA_DIGEST_LENGTH);
    return r==0;
}


btFileSet*
btFileSet_create( btFileSet *fs, int npieces, int blocksize, const char *hashbuf) {
    if (!fs) {
	fs = btcalloc( 1, sizeof(btFileSet));
	DIE_UNLESS(fs);
    }
    fs->npieces = npieces;
    fs->blocksize = blocksize;
    fs->dl = 0;
    fs->ul = 0;
    fs->hashes = btmalloc( npieces * SHA_DIGEST_LENGTH);
    memcpy(fs->hashes, hashbuf, npieces * SHA_DIGEST_LENGTH);
    kBitSet_create(&fs->completed, npieces);
    return fs;
}

void btFileSet_destroy( btFileSet *fs) {
    btPartialPiece *p, *oldp;
    int i;
    for (i=0; i< fs->nfiles; i++) {
	btfree( fs->file[i]->path);
	btfree( fs->file[i]);
    }
    btfree( fs->file);
    btfree( fs->hashes);
    kBitSet_finit(&fs->completed);
    p=fs->partial;
    while(p) {
	kBitSet_finit(&p->filled);
	oldp=p;
	p=p->next;
	btfree(oldp);
    }
    memset( fs, 0, sizeof(*fs));
}

int
btFileSet_addfile( btFileSet *fs, const char *path, _int64 len) {
    int ifile=fs->nfiles++;
    btFile *f;

#if 0
    printf("addfile( ..., %s, %d)\n", path, len);
#endif
    fs->file = btrealloc(fs->file, sizeof(btFile *)*fs->nfiles);
    DIE_UNLESS(fs->file);
    f=btcalloc(1,sizeof(btFile));
    DIE_UNLESS(f);
    f->path = strdup(path);
    f->len = len;
    if (ifile > 0) {
	btFile *lf = fs->file[ifile-1];
        f->start = lf->start + lf->len;
    }
    fs->file[ifile]=f;
    return 0;
}

static int
seg_write( btFileSet *fs, btPartialPiece *piece) {
    btFile *f;
    _int64 addr = (_int64)piece->piecenumber * fs->blocksize;
    int ifile;
    for (ifile=0; ifile < fs->nfiles; ifile++) {
	int blocklen = seg_piecelen( fs, piece->piecenumber);
	f=fs->file[ifile];
	if ( f->start+f->len >= addr &&
		f->start < addr + blocklen) 
	{
	    /* this file (partly) includes this piece */
	    _int64 beg = f->start - addr;	
	    off_t fpos=0;
	    _int64 len;
	    int fd = openPath( f->path, O_CREAT | O_WRONLY);
	    if (fd < 0) {
		SYSDIE("open failed");
	    }
	    if (beg<0) {
		fpos=-beg;
		beg=0;
	    }
	    len = min(f->len - fpos, (_int64)blocklen - beg);

	    if (lseek( fd, fpos, SEEK_SET) != fpos) {
		SYSDIE("lseek failed");
	    }

	    DIE_UNLESS( len <= fs->blocksize);
	    if (write( fd, piece->buffer+beg, len) != len) {
		SYSDIE("write failed");
	    }
	}
    }
    bs_set( &fs->completed, piece->piecenumber);
    return 0;
}

/* 
 * Return 0 if continue downloading
 * Return 1 if block complete
 */
int
seg_writebuf( btFileSet *fs, int piece, int offset, char *buf, int len) {
    int res=0;
    int blocksize;
    btPartialPiece *p=fs->partial;

    while(p && p->piecenumber!=piece) {
        p=p->next;
    }
    if (!p) {
	printf("got extra packet %d, offset %d\n", piece, offset);
	return 0;
    }
    blocksize = seg_piecelen( fs, piece);
    DIE_UNLESS(offset>=0 && offset<blocksize &&
	offset+len<=blocksize && len>0);

    memcpy(p->buffer + offset, buf, len);
    bs_setRange( &p->filled, offset, offset+len);

#if 0
    printf("packet %d of %d, offset %d, blocksize %d\n",
	    piece, fs->npieces, offset, blocksize);
#endif

    if ( p->isdone || bs_isFull( &p->filled))
    {
	/* last data for this piece */
	p->isdone=1;
	if (_checkhash( fs, piece, p->buffer)) {
	    /* Remove from partial list */
	    btPartialPiece *pp=fs->partial;
	    if(pp==p) {
	        /* First in list */
		fs->partial=p->next;
	    } else {
	        while(pp->next != p)
		    pp=pp->next;
		pp->next=p->next;
	    }

	    fs->dl += blocksize;
	    fs->left -= blocksize;
#if 1
	    printf("hash ok for block %d\n", piece);
#endif
	    seg_write( fs, p);
	    kBitSet_finit( &p->filled);
	    btfree(p);
	    res = 1;
	} else {
	    printf("hash bad for block %d\n", piece);
	}
    }
    return res;
}

int seg_readbuf( btFileSet *fs, kBitSet *writeCache, int piece, int start, char *buf, int len) {
    btFile *f;
    _int64 addr = ((_int64)piece * fs->blocksize)+start;
    int ifile;

    if (!bs_isSet(writeCache, piece) && !bs_isSet(&fs->completed, piece)) 
	{
        printf("Attempted to read uncompleted block %d from disk.\n", piece);
        return -1;
    }
    for (ifile=0; ifile < fs->nfiles; ifile++) {
	f=fs->file[ifile];
	if ( f->start+f->len >= addr &&
		f->start < addr + len) 
	{
	    /* this file (partly) includes this piece */
	    int fd = cacheopen( f->path, O_RDONLY, 0);
	    off_t fpos = addr - f->start;	

	    _int64 rlen;
	    _int64 beg = 0;
	    ssize_t res;

#if 0
	    fprintf( stderr, "cacheopen( %s, O_RDONLY)\n", f->path);
#endif
	    if (fd == -1)  {
#if 0
		bts_perror(errno, "readbuf.cacheopen");
#endif
		return -1;
	    }
	    if (fpos < 0) {
		beg = -fpos;
		fpos = 0;
	    }
	    rlen = min(f->len - fpos, len - beg);

	    if (lseek( fd, fpos, SEEK_SET) != fpos) {
		SYSDIE("lseek failed");
		return -1;
	    }


	    DIE_UNLESS( rlen <= fs->blocksize);
	    res = read(fd, buf+beg, rlen);
	    if (res != rlen) {
	        if (res == 0) {
		    /* EOF */
		    return -1;
		}
		fprintf(stderr, "On file '%s'\n", f->path);
		fprintf(stderr, "read( ..., buf[%lld], %lld) = %d\n", beg, rlen, res);
		bts_perror(errno, "readbuf.read");
		return -1;
	    }
	}
    }
    return 0;
}

int seg_review( btFileSet *fs, kBitSet *writeCache, int piece) {
    char *buf;
    int rs = 0;
    int len = seg_piecelen( fs, piece);
    buf = btmalloc(len);
    /* Trick readbuf into reading the block */
    bs_set(writeCache, piece);
    if (seg_readbuf(fs, writeCache, piece, 0, buf, len)) {
        rs = -1;
    } else if (_checkhash( fs, piece, buf)) {
	rs = 1;
    }
    if(rs!=1) {
         bs_clr(writeCache, piece);
    }
    btfree(buf);
    return rs;
}
