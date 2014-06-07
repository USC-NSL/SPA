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

#include "config.h"

#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bts.h"
#include "types.h"
#include "benc.h"

#define V(vers) #vers

#include <curl/curl.h>
#include <curl/easy.h>
#define V(vers) #vers

/* Retrieve number of seeders, leechers and downloaded torrents from tracker */
int scrape(const char *announce, unsigned char digest[SHA_DIGEST_LENGTH])
{

  btStream *io;
  btObject *result;
  int curlret;
  char *p;

  CURL *hdl;
  char url[1024];
  hdl = curl_easy_init();
  p = strstr(announce, "announce");
  if (p == NULL)
    return -1;
  strncpy(url, announce, p-announce);
  strcpy(&url[p-announce], "scrape");
  strcpy(&url[p-announce+6], &announce[p-announce+8]);
  printf("Scraping '%s'\n", url);
  curl_easy_setopt( hdl, CURLOPT_URL, url);
  io = bts_create_strstream( BTS_OUTPUT);
  curl_easy_setopt( hdl, CURLOPT_FILE, io);
  curl_easy_setopt( hdl, CURLOPT_WRITEFUNCTION, writebts);
  if ((curlret = curl_easy_perform( hdl)) != CURLE_OK)
    {
      switch (curlret)
        {
        case CURLE_COULDNT_CONNECT:
          fprintf(stderr, "Failed to transfer URL: could not connect (%d)\n",
                  curlret);
        default:
          fprintf(stderr,
                  "Failed to transfer URL for reason %d (see curl.h)\n",
                  curlret);
        }
      result=NULL;
    }
  else
    { /* parse the response */
      btInteger *downloaded, *complete, *incomplete;
      btDict *files;
      btString *k;
      int idx;
      if (bts_rewind( io, BTS_INPUT)) DIE("bts_rewind");
      if (benc_get_object( io, &result)) DIE("bad response");
      files=BTDICT( btObject_val( result, "files"));
      k = btString_create(NULL);
      btString_setbuf(k, digest, SHA_DIGEST_LENGTH);
      for (idx=0; idx<files->len; idx++) {
        if (btString_cmp(k, &files->key[idx]) == 0) {
          complete = BTINTEGER( btObject_val(files->value[idx], "complete"));
          incomplete = BTINTEGER( btObject_val(files->value[idx], "incomplete"));
          downloaded = BTINTEGER( btObject_val(files->value[idx], "downloaded"));
          printf ("seeders: %d leechers: %d downloaded: %d\n",
                  (int)complete->ival, (int)incomplete->ival,
                  (int)downloaded->ival);
        }
      }   
    }

  bts_destroy (io);
  curl_easy_cleanup( hdl);

  return 0;
}


int main( int argc, char **argv) {
    char *fname;
    btStream *in;
    btObject *o;
    btString *s;
    btInteger *i;
    btList *l;
    btDict *d;
    int idx;
    int optdebug = 0;
    int opthelp = 0;
    int optscrape = 0;
    int optquiet = 0;
    int opt;

    while ((opt = getopt( argc, argv, "dsq")) != -1) {
	switch (opt) {
        case 'd':
	    	optdebug = 1;
            break;
        case 'q':
            optquiet = 1;
            break;
        case 's':
          optscrape = 1;
          break;
	    default:
		printf("Unknown option '%c'\n", opt);
		opthelp = 1;
		break;
	}
    }

    if (optind >= argc || opthelp) {
	printf("Usage: btlist [options] <torrent>...\n");
	printf("Version: %.2f\n", VERSION);
	printf("Options:\n");
	printf("  -d            Debug dump\n");
    printf("  -s            Retrieve scrape from tracker\n");
    printf("  -q            Quiet. No file list\n");
	exit(1);
    }

    fname = argv[optind];
    in = bts_create_filestream( fname, BTS_INPUT);

    if (benc_get_object( in, &o)) {
       printf("read failed.\n");
       exit(1);
    } 

    /* 
    * Metainfo files are bencoded dictionaries with the following keys -
    * 
    *    md={
    *      announce=>'url',
    *      announce-list=>[
    *          'primary-url' or [ 'primary', 'url', 'list' ],
    *          'secondary-url' or [ 'seondary', 'url', 'list' ],
    *          ...
               ]
    *      info=>{
    *          name=>'top-level-file-or-directory-name',
    *          piece length=>12345,
    *          pieces=>'md5sums',
    * 
    *          length=>12345,      
    *            -or-
    *          files=>[
    *              {
    *                  length=>12345,
    *                  path=>['sub','directory','path','and','filename']
    *                           
    *              }, ... {}
    *          ]
    *      }
    * 
    */

    printf("metainfo file.: %s\n", fname);

    if (optdebug) {
	btObject_dump(0,o);
    }

    unsigned char digest[SHA_DIGEST_LENGTH];
    
    {
       /* SHA1 */ 
       btStream *tmpbts; 
       struct btstrbuf out;

       d=BTDICT( btObject_val( o, "info"));
       tmpbts = bts_create_strstream();
       benc_put_dict( tmpbts, d);
       out = bts_get_buf( tmpbts);
       SHA1( out.buf, out.len, digest);
       printf("info hash.....: ");
       for (idx=0; idx<sizeof(digest); idx++) {
	   printf("%02x",digest[idx]);
       }
       bts_destroy( tmpbts);
       printf("\n");
    }

    if (optquiet != 1) {
	i=BTINTEGER( btObject_val(o, "info/length"));
	if (i) {
	   /* file mode */
	   btInteger *pl;
	   s=BTSTRING( btObject_val( o, "info/name")); 
	   printf("file name.....: %s\n", s->buf);
	   s=BTSTRING( btObject_val( o, "info/pieces"));
	   pl=BTINTEGER( btObject_val(o, "info/piece length"));
	   printf("file size.....: %lld (%lld * %lld + %lld)\n", i->ival, i->ival/pl->ival, pl->ival, i->ival % pl->ival);
	} else {
	   /* dir mode */
	   _int64 tsize=0;
	   s=BTSTRING( btObject_val( o, "info/name")); 
	   printf("directory name: %s\n", s->buf);

	   l=BTLIST( btObject_val(o, "info/files"));
	   printf("files.........: %d\n", l->len);

	   for (idx=0; idx<l->len; idx++) {
	       int pathel;
	       btList *filepath;
	       btInteger *filesize;
	       filepath=BTLIST( btObject_val( l->list[idx], "path"));
	       filesize=BTINTEGER( btObject_val( l->list[idx], "length"));
	       printf("   ");
	       for (pathel=0; pathel<filepath->len; pathel++) {
		   if (pathel>0) printf("/");
		   printf("%s", BTSTRING(filepath->list[pathel])->buf);
	       }
	       printf(" (%lld)\n", filesize->ival);
	       tsize+=filesize->ival;
	   }

	   s=BTSTRING( btObject_val( o, "info/pieces"));
	   i=BTINTEGER( btObject_val(o, "info/piece length"));
	   printf("archive size..: %lld (%lld * %lld + %lld)\n",
	       tsize, tsize/i->ival, i->ival, tsize%i->ival);
	}
    }

    s=BTSTRING( btObject_val( o, "announce"));
    printf("announce url..: %s\n", s->buf);
    l=BTLIST( btObject_val( o, "announce-list"));
    if (l) {
	printf("announce list.: [ ");
	for (idx = 0; idx < l->len; idx++) {
	    btObject *o;
	    o = btList_index( l, idx);
	    if (idx > 0) printf( ",\n              :   ");
	    if (o->t == BT_LIST) {
		btList *xl = BTLIST(o);
		int j;
		printf("[ ");
		for (j = 0; j < xl->len; j++) {
		    if (j > 0) printf( ",\n              :     ");
		    printf("%s", BTSTRING(btList_index( xl, j))->buf);
		}
		printf(" ]");
	    } else {
		s = BTSTRING(o);
		printf("%s", s->buf);
	    }
	}
	printf("\n              : ]\n");
    }

    printf("\n");
    if (optscrape) {
	scrape( s->buf, digest);
    }

    btObject_destroy( o);
    bts_destroy( in);
    return 0;
}
