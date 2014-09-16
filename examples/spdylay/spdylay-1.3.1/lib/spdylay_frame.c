/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "spdylay_frame.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

#include "spdylay_helper.h"
#include "spdylay_net.h"

size_t spdylay_frame_get_len_size(uint16_t version)
{
  if(SPDYLAY_PROTO_SPDY2 == version) {
    return 2;
  } else if(SPDYLAY_PROTO_SPDY3 == version) {
    return 4;
  } else {
    /* Unsupported version */
    return 0;
  }
}

static uint8_t* spdylay_pack_str(uint8_t *buf, const char *str, size_t len,
                                 size_t len_size)
{
  spdylay_frame_put_nv_len(buf, len, len_size);
  buf += len_size;
  memcpy(buf, str, len);
  return buf+len;
}

static void spdylay_frame_pack_ctrl_hd(uint8_t* buf, const spdylay_ctrl_hd *hd)
{
  spdylay_put_uint16be(&buf[0], hd->version);
  buf[0] |= 1 << 7;
  spdylay_put_uint16be(&buf[2], hd->type);
  spdylay_put_uint32be(&buf[4], hd->length);
  buf[4] = hd->flags;
}

static void spdylay_frame_unpack_ctrl_hd(spdylay_ctrl_hd *hd,
                                         const uint8_t* buf)
{
  hd->version = spdylay_get_uint16(buf) & SPDYLAY_VERSION_MASK;
  hd->type = spdylay_get_uint16(&buf[2]);
  hd->flags = buf[4];
  hd->length = spdylay_get_uint32(&buf[4]) & SPDYLAY_LENGTH_MASK;
}

ssize_t spdylay_frame_alloc_pack_nv(uint8_t **buf_ptr,
                                    size_t *buflen_ptr,
                                    uint8_t **nvbuf_ptr,
                                    size_t *nvbuflen_ptr,
                                    char **nv, size_t nv_offset,
                                    size_t len_size,
                                    spdylay_zlib *deflater)
{
  size_t nvspace;
  size_t maxframelen;
  ssize_t framelen;
  int r;
  nvspace = spdylay_frame_count_nv_space(nv, len_size);
  r = spdylay_reserve_buffer(nvbuf_ptr, nvbuflen_ptr, nvspace);
  if(r != 0) {
    return SPDYLAY_ERR_NOMEM;
  }
  maxframelen = nv_offset+spdylay_zlib_deflate_hd_bound(deflater, nvspace);
  r = spdylay_reserve_buffer(buf_ptr, buflen_ptr, maxframelen);
  if(r != 0) {
    return SPDYLAY_ERR_NOMEM;
  }
  spdylay_frame_pack_nv(*nvbuf_ptr, nv, len_size);
  framelen = spdylay_zlib_deflate_hd(deflater,
                                     (*buf_ptr)+nv_offset,
                                     maxframelen-nv_offset,
                                     *nvbuf_ptr, nvspace);
  if(framelen < 0) {
    return framelen;
  }
  framelen += nv_offset;

  if(framelen - SPDYLAY_FRAME_HEAD_LENGTH > SPDYLAY_LENGTH_MASK) {
    /* In SPDY/2 and 3, Max frame size is 2**24 - 1. */
    return SPDYLAY_ERR_FRAME_TOO_LARGE;
  }
  return framelen;
}

int spdylay_frame_count_unpack_nv_space(size_t *nvlen_ptr, size_t *buflen_ptr,
                                        spdylay_buffer *in, size_t len_size)
{
  uint32_t n;
  size_t buflen = 0;
  size_t nvlen = 0;
  size_t off = 0;
  size_t inlen = spdylay_buffer_length(in);
  size_t i;
  spdylay_buffer_reader reader;
  if(inlen < len_size) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  spdylay_buffer_reader_init(&reader, in);

  /* TODO limit n in a reasonable number */
  n = spdylay_frame_get_nv_len(&reader, len_size);
  off += len_size;
  for(i = 0; i < n; ++i) {
    uint32_t len;
    size_t j;
    for(j = 0; j < 2; ++j) {
      if(inlen-off < len_size) {
        return SPDYLAY_ERR_INVALID_FRAME;
      }
      len = spdylay_frame_get_nv_len(&reader, len_size);
      off += len_size;
      if(inlen-off < len) {
        return SPDYLAY_ERR_INVALID_FRAME;
      }
      buflen += len+1;
      off += len;
      if(j == 0) {
        spdylay_buffer_reader_advance(&reader, len);
      }
    }
    nvlen += spdylay_buffer_reader_count(&reader, len, '\0');
    ++nvlen;
  }
  if(inlen == off) {
    *nvlen_ptr = nvlen;
    *buflen_ptr = buflen+(nvlen*2+1)*sizeof(char*);
    return 0;
  } else {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
}

static int VALID_HD_VALUE_CHARS[] = {
  1 /* NUL  */, 0 /* SOH  */, 0 /* STX  */, 0 /* ETX  */,
  0 /* EOT  */, 0 /* ENQ  */, 0 /* ACK  */, 0 /* BEL  */,
  0 /* BS   */, 1 /* HT   */, 0 /* LF   */, 0 /* VT   */,
  0 /* FF   */, 0 /* CR   */, 0 /* SO   */, 0 /* SI   */,
  0 /* DLE  */, 0 /* DC1  */, 0 /* DC2  */, 0 /* DC3  */,
  0 /* DC4  */, 0 /* NAK  */, 0 /* SYN  */, 0 /* ETB  */,
  0 /* CAN  */, 0 /* EM   */, 0 /* SUB  */, 0 /* ESC  */,
  0 /* FS   */, 0 /* GS   */, 0 /* RS   */, 0 /* US   */,
  1 /* SPC  */, 1 /* !    */, 1 /* "    */, 1 /* #    */,
  1 /* $    */, 1 /* %    */, 1 /* &    */, 1 /* '    */,
  1 /* (    */, 1 /* )    */, 1 /* *    */, 1 /* +    */,
  1 /* ,    */, 1 /* -    */, 1 /* .    */, 1 /* /    */,
  1 /* 0    */, 1 /* 1    */, 1 /* 2    */, 1 /* 3    */,
  1 /* 4    */, 1 /* 5    */, 1 /* 6    */, 1 /* 7    */,
  1 /* 8    */, 1 /* 9    */, 1 /* :    */, 1 /* ;    */,
  1 /* <    */, 1 /* =    */, 1 /* >    */, 1 /* ?    */,
  1 /* @    */, 1 /* A    */, 1 /* B    */, 1 /* C    */,
  1 /* D    */, 1 /* E    */, 1 /* F    */, 1 /* G    */,
  1 /* H    */, 1 /* I    */, 1 /* J    */, 1 /* K    */,
  1 /* L    */, 1 /* M    */, 1 /* N    */, 1 /* O    */,
  1 /* P    */, 1 /* Q    */, 1 /* R    */, 1 /* S    */,
  1 /* T    */, 1 /* U    */, 1 /* V    */, 1 /* W    */,
  1 /* X    */, 1 /* Y    */, 1 /* Z    */, 1 /* [    */,
  1 /* \    */, 1 /* ]    */, 1 /* ^    */, 1 /* _    */,
  1 /* `    */, 1 /* a    */, 1 /* b    */, 1 /* c    */,
  1 /* d    */, 1 /* e    */, 1 /* f    */, 1 /* g    */,
  1 /* h    */, 1 /* i    */, 1 /* j    */, 1 /* k    */,
  1 /* l    */, 1 /* m    */, 1 /* n    */, 1 /* o    */,
  1 /* p    */, 1 /* q    */, 1 /* r    */, 1 /* s    */,
  1 /* t    */, 1 /* u    */, 1 /* v    */, 1 /* w    */,
  1 /* x    */, 1 /* y    */, 1 /* z    */, 1 /* {    */,
  1 /* |    */, 1 /* }    */, 1 /* ~    */, 0 /* DEL  */,
  1 /* 0x80 */, 1 /* 0x81 */, 1 /* 0x82 */, 1 /* 0x83 */,
  1 /* 0x84 */, 1 /* 0x85 */, 1 /* 0x86 */, 1 /* 0x87 */,
  1 /* 0x88 */, 1 /* 0x89 */, 1 /* 0x8a */, 1 /* 0x8b */,
  1 /* 0x8c */, 1 /* 0x8d */, 1 /* 0x8e */, 1 /* 0x8f */,
  1 /* 0x90 */, 1 /* 0x91 */, 1 /* 0x92 */, 1 /* 0x93 */,
  1 /* 0x94 */, 1 /* 0x95 */, 1 /* 0x96 */, 1 /* 0x97 */,
  1 /* 0x98 */, 1 /* 0x99 */, 1 /* 0x9a */, 1 /* 0x9b */,
  1 /* 0x9c */, 1 /* 0x9d */, 1 /* 0x9e */, 1 /* 0x9f */,
  1 /* 0xa0 */, 1 /* 0xa1 */, 1 /* 0xa2 */, 1 /* 0xa3 */,
  1 /* 0xa4 */, 1 /* 0xa5 */, 1 /* 0xa6 */, 1 /* 0xa7 */,
  1 /* 0xa8 */, 1 /* 0xa9 */, 1 /* 0xaa */, 1 /* 0xab */,
  1 /* 0xac */, 1 /* 0xad */, 1 /* 0xae */, 1 /* 0xaf */,
  1 /* 0xb0 */, 1 /* 0xb1 */, 1 /* 0xb2 */, 1 /* 0xb3 */,
  1 /* 0xb4 */, 1 /* 0xb5 */, 1 /* 0xb6 */, 1 /* 0xb7 */,
  1 /* 0xb8 */, 1 /* 0xb9 */, 1 /* 0xba */, 1 /* 0xbb */,
  1 /* 0xbc */, 1 /* 0xbd */, 1 /* 0xbe */, 1 /* 0xbf */,
  1 /* 0xc0 */, 1 /* 0xc1 */, 1 /* 0xc2 */, 1 /* 0xc3 */,
  1 /* 0xc4 */, 1 /* 0xc5 */, 1 /* 0xc6 */, 1 /* 0xc7 */,
  1 /* 0xc8 */, 1 /* 0xc9 */, 1 /* 0xca */, 1 /* 0xcb */,
  1 /* 0xcc */, 1 /* 0xcd */, 1 /* 0xce */, 1 /* 0xcf */,
  1 /* 0xd0 */, 1 /* 0xd1 */, 1 /* 0xd2 */, 1 /* 0xd3 */,
  1 /* 0xd4 */, 1 /* 0xd5 */, 1 /* 0xd6 */, 1 /* 0xd7 */,
  1 /* 0xd8 */, 1 /* 0xd9 */, 1 /* 0xda */, 1 /* 0xdb */,
  1 /* 0xdc */, 1 /* 0xdd */, 1 /* 0xde */, 1 /* 0xdf */,
  1 /* 0xe0 */, 1 /* 0xe1 */, 1 /* 0xe2 */, 1 /* 0xe3 */,
  1 /* 0xe4 */, 1 /* 0xe5 */, 1 /* 0xe6 */, 1 /* 0xe7 */,
  1 /* 0xe8 */, 1 /* 0xe9 */, 1 /* 0xea */, 1 /* 0xeb */,
  1 /* 0xec */, 1 /* 0xed */, 1 /* 0xee */, 1 /* 0xef */,
  1 /* 0xf0 */, 1 /* 0xf1 */, 1 /* 0xf2 */, 1 /* 0xf3 */,
  1 /* 0xf4 */, 1 /* 0xf5 */, 1 /* 0xf6 */, 1 /* 0xf7 */,
  1 /* 0xf8 */, 1 /* 0xf9 */, 1 /* 0xfa */, 1 /* 0xfb */,
  1 /* 0xfc */, 1 /* 0xfd */, 1 /* 0xfe */, 1 /* 0xff */
};

int spdylay_frame_unpack_nv(char ***nv_ptr, spdylay_buffer *in,
                            size_t len_size)
{
  size_t nvlen, buflen;
  int r;
  size_t i;
  char *buf, **idx, *data;
  uint32_t n;
  int invalid_header_block = 0;
  spdylay_buffer_reader reader;
  r = spdylay_frame_count_unpack_nv_space(&nvlen, &buflen, in, len_size);
  if(r != 0) {
    return r;
  }

  buf = malloc(buflen);
  if(buf == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  spdylay_buffer_reader_init(&reader, in);
  idx = (char**)buf;
  data = buf+(nvlen*2+1)*sizeof(char*);
  n = spdylay_frame_get_nv_len(&reader, len_size);
  for(i = 0; i < n; ++i) {
    uint32_t len;
    char *name, *val;
    char *stop;
    int multival;
    len = spdylay_frame_get_nv_len(&reader, len_size);
    if(len == 0) {
      invalid_header_block = 1;
    }
    name = data;
    spdylay_buffer_reader_data(&reader, (uint8_t*)data, len);
    for(stop = data+len; data != stop; ++data) {
      unsigned char c = *data;
      if(c < 0x20u || c > 0x7eu || ('A' <= c && c <= 'Z')) {
        invalid_header_block = 1;
      }
    }
    *data = '\0';
    ++data;

    len = spdylay_frame_get_nv_len(&reader, len_size);
    val = data;
    spdylay_buffer_reader_data(&reader, (uint8_t*)data, len);

    multival = 0;
    for(stop = data+len; data != stop; ++data) {
      if(*data == '\0') {
        *idx++ = name;
        *idx++ = val;
        if(val == data) {
          invalid_header_block = 1;
        }
        val = data+1;
        multival = 1;
      } else if(!VALID_HD_VALUE_CHARS[(unsigned char)*data]) {
        invalid_header_block = 1;
      }
    }
    *data = '\0';
    /* Check last header value is empty if NULL separator was
       found. */
    if(multival && val == data) {
      invalid_header_block = 1;
    }
    ++data;

    *idx++ = name;
    *idx++ = val;
  }
  *idx = NULL;
  assert((size_t)((char*)idx - buf) == (nvlen*2)*sizeof(char*));
  *nv_ptr = (char**)buf;
  if(!invalid_header_block) {
    spdylay_frame_nv_sort(*nv_ptr);
    for(i = 2; i < nvlen*2; i += 2) {
      if(strcmp((*nv_ptr)[i-2], (*nv_ptr)[i]) == 0 &&
         (*nv_ptr)[i-2] != (*nv_ptr)[i]) {
        invalid_header_block = 1;
        break;
      }
    }
  }
  return invalid_header_block ? SPDYLAY_ERR_INVALID_HEADER_BLOCK : 0;
}

size_t spdylay_frame_count_nv_space(char **nv, size_t len_size)
{
  size_t sum = len_size;
  int i;
  const char *prev = "";
  size_t prevlen = 0;
  size_t prevvallen = 0;
  for(i = 0; nv[i]; i += 2) {
    const char *key = nv[i];
    const char *val = nv[i+1];
    size_t keylen = strlen(key);
    size_t vallen = strlen(val);
    if(prevlen == keylen && memcmp(prev, key, keylen) == 0) {
      if(vallen) {
        if(prevvallen) {
          /* Join previous value, with NULL character */
          sum += vallen+1;
          prevvallen = vallen;
        } else {
          /* Previous value is empty. In this case, drop the
             previous. */
          sum += vallen;
        }
      }
    } else {
      prev = key;
      prevlen = keylen;
      prevvallen = vallen;
      /* SPDY NV header does not include terminating NULL byte */
      sum += keylen+vallen+len_size*2;
    }
  }
  return sum;
}

ssize_t spdylay_frame_pack_nv(uint8_t *buf, char **nv, size_t len_size)
{
  int i;
  uint8_t *bufp = buf+len_size;
  uint32_t num_nv = 0;
  const char *prev = "";
  uint8_t *cur_vallen_buf = NULL;
  uint32_t cur_vallen = 0;
  size_t prevkeylen = 0;
  size_t prevvallen = 0;
  for(i = 0; nv[i]; i += 2) {
    const char *key = nv[i];
    const char *val = nv[i+1];
    size_t keylen = strlen(key);
    size_t vallen = strlen(val);
    if(prevkeylen == keylen && memcmp(prev, key, keylen) == 0) {
      if(vallen) {
        if(prevvallen) {
          /* Join previous value, with NULL character */
          cur_vallen += vallen+1;
          spdylay_frame_put_nv_len(cur_vallen_buf, cur_vallen, len_size);
          *bufp = '\0';
          ++bufp;
          memcpy(bufp, val, vallen);
          bufp += vallen;
        } else {
          /* Previous value is empty. In this case, drop the
             previous. */
          cur_vallen += vallen;
          spdylay_frame_put_nv_len(cur_vallen_buf, cur_vallen, len_size);
          memcpy(bufp, val, vallen);
          bufp += vallen;
        }
      }
    } else {
      ++num_nv;
      bufp = spdylay_pack_str(bufp, key, keylen, len_size);
      prev = key;
      cur_vallen_buf = bufp;
      cur_vallen = vallen;
      prevkeylen = keylen;
      prevvallen = vallen;
      bufp = spdylay_pack_str(bufp, val, vallen, len_size);
    }
  }
  spdylay_frame_put_nv_len(buf, num_nv, len_size);
  return bufp-buf;
}

int spdylay_frame_is_ctrl_frame(uint8_t first_byte)
{
  return first_byte & 0x80;
}

void spdylay_frame_nv_del(char **nv)
{
  free(nv);
}

char** spdylay_frame_nv_copy(const char **nv)
{
  int i;
  char *buf;
  char **idx, *data;
  size_t buflen = 0;
  for(i = 0; nv[i]; ++i) {
    buflen += strlen(nv[i])+1;
  }
  buflen += (i+1)*sizeof(char*);
  buf = malloc(buflen);
  if(buf == NULL) {
    return NULL;
  }
  idx = (char**)buf;
  data = buf+(i+1)*sizeof(char*);

  for(i = 0; nv[i]; ++i) {
    size_t len = strlen(nv[i])+1;
    memcpy(data, nv[i], len);
    *idx++ = data;
    data += len;
  }
  *idx = NULL;
  return (char**)buf;
}

static int spdylay_string_compar(const void *lhs, const void *rhs)
{
  return strcmp(*(char **)lhs, *(char **)rhs);
}

void spdylay_frame_nv_sort(char **nv)
{
  int n;
  for(n = 0; nv[n]; ++n);
  qsort(nv, n/2, 2*sizeof(char*), spdylay_string_compar);
}

void spdylay_frame_nv_downcase(char **nv)
{
  int i, j;
  for(i = 0; nv[i]; i += 2) {
    for(j = 0; nv[i][j] != '\0'; ++j) {
      if('A' <= nv[i][j] && nv[i][j] <= 'Z') {
        nv[i][j] += 'a'-'A';
      }
    }
  }
}

char** spdylay_frame_nv_norm_copy(const char **nv)
{
  char **nv_copy;
  nv_copy = spdylay_frame_nv_copy(nv);
  if(nv_copy != NULL) {
    spdylay_frame_nv_downcase(nv_copy);
    spdylay_frame_nv_sort(nv_copy);
  }
  return nv_copy;
}

/* Table to translate SPDY/3 header names to SPDY/2. */
static const char *spdylay_nv_3to2[] = {
  ":host", "host",
  ":method", "method",
  ":path", "url",
  ":scheme", "scheme",
  ":status", "status",
  ":version", "version",
  NULL
};

void spdylay_frame_nv_3to2(char **nv)
{
  int i, j;
  for(i = 0; nv[i]; i += 2) {
    for(j = 0; spdylay_nv_3to2[j]; j += 2) {
      if(strcmp(nv[i], spdylay_nv_3to2[j]) == 0) {
        nv[i] = (char*)spdylay_nv_3to2[j+1];
        break;
      }
    }
  }
}

void spdylay_frame_nv_2to3(char **nv)
{
  int i, j;
  for(i = 0; nv[i]; i += 2) {
    for(j = 0; spdylay_nv_3to2[j]; j += 2) {
      if(strcmp(nv[i], spdylay_nv_3to2[j+1]) == 0) {
        nv[i] = (char*)spdylay_nv_3to2[j];
        break;
      }
    }
  }
}

#define SPDYLAY_HTTPS_PORT 443

int spdylay_frame_nv_set_origin(char **nv, spdylay_origin *origin)
{
  int scheme_found, host_found;
  int i;
  scheme_found = host_found = 0;
  for(i = 0; nv[i]; i += 2) {
    if(scheme_found == 0 && strcmp(":scheme", nv[i]) == 0) {
      size_t len = strlen(nv[i+1]);
      if(len <= SPDYLAY_MAX_SCHEME) {
        strcpy(origin->scheme, nv[i+1]);
        scheme_found = 1;
      }
    } else if(host_found == 0 && strcmp(":host", nv[i]) == 0) {
      size_t len = strlen(nv[i+1]);
      char *sep = memchr(nv[i+1], ':', len);
      size_t hostlen;
      if(sep == NULL) {
        origin->port = SPDYLAY_HTTPS_PORT;
        sep = nv[i+1]+len;
      } else {
        unsigned long int port;
        errno = 0;
        port = strtoul(sep+1, NULL, 10);
        if(errno != 0 || port == 0 || port > UINT16_MAX) {
          continue;
        }
        origin->port = port;
      }
      hostlen = sep-nv[i+1];
      if(hostlen > SPDYLAY_MAX_HOSTNAME) {
        continue;
      }
      memcpy(origin->host, nv[i+1], hostlen);
      origin->host[hostlen] = '\0';
      host_found = 1;
    }
  }
  if(scheme_found && host_found) {
    return 0;
  } else {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
}

void spdylay_frame_syn_stream_init(spdylay_syn_stream *frame,
                                   uint16_t version, uint8_t flags,
                                   int32_t stream_id, int32_t assoc_stream_id,
                                   uint8_t pri, char **nv)
{
  memset(frame, 0, sizeof(spdylay_syn_stream));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_SYN_STREAM;
  frame->hd.flags = flags;
  frame->stream_id = stream_id;
  frame->assoc_stream_id = assoc_stream_id;
  frame->pri = pri;
  frame->nv = nv;
}

void spdylay_frame_syn_stream_free(spdylay_syn_stream *frame)
{
  spdylay_frame_nv_del(frame->nv);
}

void spdylay_frame_syn_reply_init(spdylay_syn_reply *frame,
                                  uint16_t version, uint8_t flags,
                                  int32_t stream_id, char **nv)
{
  memset(frame, 0, sizeof(spdylay_syn_reply));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_SYN_REPLY;
  frame->hd.flags = flags;
  frame->stream_id = stream_id;
  frame->nv = nv;
}

void spdylay_frame_syn_reply_free(spdylay_syn_reply *frame)
{
  spdylay_frame_nv_del(frame->nv);
}

void spdylay_frame_ping_init(spdylay_ping *frame,
                             uint16_t version, uint32_t unique_id)
{
  memset(frame, 0, sizeof(spdylay_ping));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_PING;
  frame->hd.flags = SPDYLAY_CTRL_FLAG_NONE;
  frame->hd.length = 4;
  frame->unique_id = unique_id;
}

void spdylay_frame_ping_free(spdylay_ping *frame)
{}

void spdylay_frame_goaway_init(spdylay_goaway *frame,
                               uint16_t version, int32_t last_good_stream_id,
                               uint32_t status_code)
{
  memset(frame, 0, sizeof(spdylay_goaway));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_GOAWAY;
  if(version == SPDYLAY_PROTO_SPDY2) {
    frame->hd.length = 4;
  } else if(version == SPDYLAY_PROTO_SPDY3) {
    frame->hd.length = 8;
    frame->status_code = status_code;
  } else {
    frame->hd.length = 0;
  }
  frame->last_good_stream_id = last_good_stream_id;
}

void spdylay_frame_goaway_free(spdylay_goaway *frame)
{}

void spdylay_frame_headers_init(spdylay_headers *frame,
                                uint16_t version, uint8_t flags,
                                int32_t stream_id, char **nv)
{
  memset(frame, 0, sizeof(spdylay_headers));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_HEADERS;
  frame->hd.flags = flags;
  frame->stream_id = stream_id;
  frame->nv = nv;
}

void spdylay_frame_headers_free(spdylay_headers *frame)
{
  spdylay_frame_nv_del(frame->nv);
}

void spdylay_frame_rst_stream_init(spdylay_rst_stream *frame,
                                   uint16_t version,
                                   int32_t stream_id, uint32_t status_code)
{
  memset(frame, 0, sizeof(spdylay_rst_stream));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_RST_STREAM;
  frame->hd.flags = 0;
  frame->hd.length = 8;
  frame->stream_id = stream_id;
  frame->status_code = status_code;
}

void spdylay_frame_rst_stream_free(spdylay_rst_stream *frame)
{}

void spdylay_frame_window_update_init(spdylay_window_update *frame,
                                      uint16_t version,
                                      int32_t stream_id,
                                      int32_t delta_window_size)
{
  memset(frame, 0, sizeof(spdylay_window_update));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_WINDOW_UPDATE;
  frame->hd.flags = 0;
  frame->hd.length = 8;
  frame->stream_id = stream_id;
  frame->delta_window_size = delta_window_size;
}

void spdylay_frame_window_update_free(spdylay_window_update *frame)
{}

void spdylay_frame_settings_init(spdylay_settings *frame,
                                 uint16_t version, uint8_t flags,
                                 spdylay_settings_entry *iv, size_t niv)
{
  memset(frame, 0, sizeof(spdylay_settings));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_SETTINGS;
  frame->hd.flags = flags;
  frame->hd.length = 4+niv*8;
  frame->niv = niv;
  frame->iv = iv;
}

void spdylay_frame_settings_free(spdylay_settings *frame)
{
  free(frame->iv);
}

void spdylay_frame_credential_init(spdylay_credential *frame,
                                   uint16_t version, uint16_t slot,
                                   spdylay_mem_chunk *proof,
                                   spdylay_mem_chunk *certs,
                                   size_t ncerts)
{
  size_t i;
  memset(frame, 0, sizeof(spdylay_credential));
  frame->hd.version = version;
  frame->hd.type = SPDYLAY_CREDENTIAL;
  frame->slot = slot;
  frame->proof = *proof;
  frame->certs = certs;
  frame->ncerts = ncerts;
  frame->hd.length = 2+4+frame->proof.length;
  for(i = 0; i < ncerts; ++i) {
    frame->hd.length += 4+frame->certs[i].length;
  }
}

void spdylay_frame_credential_free(spdylay_credential *frame)
{
  size_t i;
  free(frame->proof.data);
  for(i = 0; i < frame->ncerts; ++i) {
    free(frame->certs[i].data);
  }
  free(frame->certs);
}

void spdylay_frame_data_init(spdylay_data *frame, int32_t stream_id,
                             uint8_t flags,
                             const spdylay_data_provider *data_prd)
{
  memset(frame, 0, sizeof(spdylay_data));
  frame->stream_id = stream_id;
  frame->flags = flags;
  frame->data_prd = *data_prd;
}

void spdylay_frame_data_free(spdylay_data *frame)
{}

ssize_t spdylay_frame_pack_syn_stream(uint8_t **buf_ptr,
                                      size_t *buflen_ptr,
                                      uint8_t **nvbuf_ptr,
                                      size_t *nvbuflen_ptr,
                                      spdylay_syn_stream *frame,
                                      spdylay_zlib *deflater)
{
  ssize_t framelen;
  size_t len_size = spdylay_frame_get_len_size(frame->hd.version);
  if(len_size == 0) {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  framelen = spdylay_frame_alloc_pack_nv(buf_ptr, buflen_ptr,
                                         nvbuf_ptr, nvbuflen_ptr,
                                         frame->nv,
                                         SPDYLAY_SYN_STREAM_NV_OFFSET,
                                         len_size,
                                         deflater);
  if(framelen < 0) {
    return framelen;
  }
  frame->hd.length = framelen-SPDYLAY_FRAME_HEAD_LENGTH;
  memset(*buf_ptr, 0, SPDYLAY_SYN_STREAM_NV_OFFSET);
  /* pack ctrl header after length is determined */
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  spdylay_put_uint32be(&(*buf_ptr)[8], frame->stream_id);
  spdylay_put_uint32be(&(*buf_ptr)[12], frame->assoc_stream_id);
  if(frame->hd.version == SPDYLAY_PROTO_SPDY3) {
    (*buf_ptr)[16] = (frame->pri << 5);
    (*buf_ptr)[17] = frame->slot;
  } else {
    (*buf_ptr)[16] = (frame->pri << 6);
  }
  return framelen;
}

int spdylay_frame_unpack_syn_stream(spdylay_syn_stream *frame,
                                    const uint8_t *head, size_t headlen,
                                    const uint8_t *payload, size_t payloadlen,
                                    spdylay_buffer *inflatebuf)
{
  int r;
  size_t len_size;
  r = spdylay_frame_unpack_syn_stream_without_nv(frame, head, headlen,
                                                 payload, payloadlen);
  len_size = spdylay_frame_get_len_size(frame->hd.version);
  if(len_size == 0) {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  if(r == 0) {
    r = spdylay_frame_unpack_nv(&frame->nv, inflatebuf, len_size);
  }
  return r;
}

int spdylay_frame_unpack_syn_stream_without_nv(spdylay_syn_stream *frame,
                                               const uint8_t *head,
                                               size_t headlen,
                                               const uint8_t *payload,
                                               size_t payloadlen)
{
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  if(headlen + payloadlen != SPDYLAY_SYN_STREAM_NV_OFFSET) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  frame->stream_id = spdylay_get_uint32(payload) & SPDYLAY_STREAM_ID_MASK;
  frame->assoc_stream_id =
    spdylay_get_uint32(payload+4) & SPDYLAY_STREAM_ID_MASK;
  if(frame->hd.version == SPDYLAY_PROTO_SPDY3) {
    frame->pri = (*(payload+8) >> 5);
    frame->slot = payload[9];
  } else {
    frame->pri = (*(payload+8) >> 6);
    frame->slot = 0;
  }
  frame->nv = NULL;
  return 0;
}

ssize_t spdylay_frame_pack_syn_reply(uint8_t **buf_ptr,
                                     size_t *buflen_ptr,
                                     uint8_t **nvbuf_ptr,
                                     size_t *nvbuflen_ptr,
                                     spdylay_syn_reply *frame,
                                     spdylay_zlib *deflater)
{
  ssize_t framelen;
  size_t len_size;
  ssize_t nv_offset;
  len_size = spdylay_frame_get_len_size(frame->hd.version);
  if(len_size == 0) {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  nv_offset = spdylay_frame_nv_offset(SPDYLAY_SYN_REPLY, frame->hd.version);
  assert(nv_offset > 0);
  framelen = spdylay_frame_alloc_pack_nv(buf_ptr, buflen_ptr,
                                         nvbuf_ptr, nvbuflen_ptr,
                                         frame->nv, nv_offset,
                                         len_size, deflater);
  if(framelen < 0) {
    return framelen;
  }
  frame->hd.length = framelen-SPDYLAY_FRAME_HEAD_LENGTH;
  memset(*buf_ptr, 0, nv_offset);
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  spdylay_put_uint32be(&(*buf_ptr)[8], frame->stream_id);
  return framelen;
}

int spdylay_frame_unpack_syn_reply(spdylay_syn_reply *frame,
                                   const uint8_t *head, size_t headlen,
                                   const uint8_t *payload, size_t payloadlen,
                                   spdylay_buffer *inflatebuf)
{
  int r;
  r = spdylay_frame_unpack_syn_reply_without_nv(frame, head, headlen,
                                                payload, payloadlen);
  if(r == 0) {
    size_t len_size;
    len_size = spdylay_frame_get_len_size(frame->hd.version);
    if(len_size == 0) {
      return SPDYLAY_ERR_UNSUPPORTED_VERSION;
    }
    r = spdylay_frame_unpack_nv(&frame->nv, inflatebuf, len_size);
  }
  return r;
}

int spdylay_frame_unpack_syn_reply_without_nv(spdylay_syn_reply *frame,
                                              const uint8_t *head,
                                              size_t headlen,
                                              const uint8_t *payload,
                                              size_t payloadlen)
{
  ssize_t nv_offset;
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  nv_offset = spdylay_frame_nv_offset(SPDYLAY_SYN_REPLY, frame->hd.version);
  assert(nv_offset > 0);
  if((ssize_t)(headlen + payloadlen) != nv_offset) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  frame->stream_id = spdylay_get_uint32(payload) & SPDYLAY_STREAM_ID_MASK;
  frame->nv = NULL;
  return 0;
}

ssize_t spdylay_frame_pack_ping(uint8_t **buf_ptr, size_t *buflen_ptr,
                                spdylay_ping *frame)
{
  ssize_t framelen = 12;
  int r;
  r = spdylay_reserve_buffer(buf_ptr, buflen_ptr, framelen);
  if(r != 0) {
    return r;
  }
  memset(*buf_ptr, 0, framelen);
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  spdylay_put_uint32be(&(*buf_ptr)[8], frame->unique_id);
  return framelen;
}

int spdylay_frame_unpack_ping(spdylay_ping *frame,
                              const uint8_t *head, size_t headlen,
                              const uint8_t *payload, size_t payloadlen)
{
  if(payloadlen != 4) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  frame->unique_id = spdylay_get_uint32(payload);
  return 0;
}

ssize_t spdylay_frame_pack_goaway(uint8_t **buf_ptr, size_t *buflen_ptr,
                                  spdylay_goaway *frame)
{
  ssize_t framelen;
  int r;
  if(frame->hd.version == SPDYLAY_PROTO_SPDY2) {
    framelen = 12;
  } else if(frame->hd.version == SPDYLAY_PROTO_SPDY3) {
    framelen = 16;
  } else {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  r = spdylay_reserve_buffer(buf_ptr, buflen_ptr, framelen);
  if(r != 0) {
    return r;
  }
  memset(*buf_ptr, 0, framelen);
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  spdylay_put_uint32be(&(*buf_ptr)[8], frame->last_good_stream_id);
  if(frame->hd.version == SPDYLAY_PROTO_SPDY3) {
    spdylay_put_uint32be(&(*buf_ptr)[12], frame->status_code);
  }
  return framelen;
}

int spdylay_frame_unpack_goaway(spdylay_goaway *frame,
                                const uint8_t *head, size_t headlen,
                                const uint8_t *payload, size_t payloadlen)
{
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  if(frame->hd.version == SPDYLAY_PROTO_SPDY2) {
    if(payloadlen != 4) {
      return SPDYLAY_ERR_INVALID_FRAME;
    }
  } else if(frame->hd.version == SPDYLAY_PROTO_SPDY3) {
    if(payloadlen != 8) {
      return SPDYLAY_ERR_INVALID_FRAME;
    }
  } else {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  frame->last_good_stream_id = spdylay_get_uint32(payload) &
    SPDYLAY_STREAM_ID_MASK;
  if(frame->hd.version == SPDYLAY_PROTO_SPDY3) {
    frame->status_code = spdylay_get_uint32(payload+4);
  } else {
    frame->status_code = 0;
  }
  return 0;
}

ssize_t spdylay_frame_pack_headers(uint8_t **buf_ptr, size_t *buflen_ptr,
                                   uint8_t **nvbuf_ptr, size_t *nvbuflen_ptr,
                                   spdylay_headers *frame,
                                   spdylay_zlib *deflater)
{
  ssize_t framelen;
  size_t len_size;
  ssize_t nv_offset;
  len_size = spdylay_frame_get_len_size(frame->hd.version);
  if(len_size == 0) {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  nv_offset = spdylay_frame_nv_offset(SPDYLAY_HEADERS, frame->hd.version);
  assert(nv_offset > 0);
  framelen = spdylay_frame_alloc_pack_nv(buf_ptr, buflen_ptr,
                                         nvbuf_ptr, nvbuflen_ptr,
                                         frame->nv, nv_offset,
                                         len_size, deflater);
  if(framelen < 0) {
    return framelen;
  }
  frame->hd.length = framelen-SPDYLAY_FRAME_HEAD_LENGTH;
  memset(*buf_ptr, 0, nv_offset);
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  spdylay_put_uint32be(&(*buf_ptr)[8], frame->stream_id);
  return framelen;
}

int spdylay_frame_unpack_headers(spdylay_headers *frame,
                                 const uint8_t *head, size_t headlen,
                                 const uint8_t *payload, size_t payloadlen,
                                 spdylay_buffer *inflatebuf)
{
  int r;
  r = spdylay_frame_unpack_headers_without_nv(frame, head, headlen,
                                              payload, payloadlen);
  if(r == 0) {
    size_t len_size;
    len_size = spdylay_frame_get_len_size(frame->hd.version);
    if(len_size == 0) {
      return SPDYLAY_ERR_UNSUPPORTED_VERSION;
    }
    r = spdylay_frame_unpack_nv(&frame->nv, inflatebuf, len_size);
  }
  return r;
}

int spdylay_frame_unpack_headers_without_nv(spdylay_headers *frame,
                                            const uint8_t *head,
                                            size_t headlen,
                                            const uint8_t *payload,
                                            size_t payloadlen)
{
  ssize_t nv_offset;
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  nv_offset = spdylay_frame_nv_offset(SPDYLAY_HEADERS, frame->hd.version);
  assert(nv_offset > 0);
  if((ssize_t)(headlen + payloadlen) != nv_offset) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  frame->stream_id = spdylay_get_uint32(payload) & SPDYLAY_STREAM_ID_MASK;
  frame->nv = NULL;
  return 0;
}

ssize_t spdylay_frame_pack_rst_stream(uint8_t **buf_ptr, size_t *buflen_ptr,
                                      spdylay_rst_stream *frame)
{
  ssize_t framelen = 16;
  int r;
  r = spdylay_reserve_buffer(buf_ptr, buflen_ptr, framelen);
  if(r != 0) {
    return r;
  }
  memset(*buf_ptr, 0, framelen);
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  spdylay_put_uint32be(&(*buf_ptr)[8], frame->stream_id);
  spdylay_put_uint32be(&(*buf_ptr)[12], frame->status_code);
  return framelen;
}

int spdylay_frame_unpack_rst_stream(spdylay_rst_stream *frame,
                                    const uint8_t *head, size_t headlen,
                                    const uint8_t *payload, size_t payloadlen)
{
  if(payloadlen != 8) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  frame->stream_id = spdylay_get_uint32(payload) & SPDYLAY_STREAM_ID_MASK;
  frame->status_code = spdylay_get_uint32(payload+4);
  return 0;
}

ssize_t spdylay_frame_pack_window_update(uint8_t **buf_ptr, size_t *buflen_ptr,
                                         spdylay_window_update *frame)
{
  ssize_t framelen = 16;
  int r;
  r = spdylay_reserve_buffer(buf_ptr, buflen_ptr, framelen);
  if(r != 0) {
    return r;
  }
  memset(*buf_ptr, 0, framelen);
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  spdylay_put_uint32be(&(*buf_ptr)[8], frame->stream_id);
  spdylay_put_uint32be(&(*buf_ptr)[12], frame->delta_window_size);
  return framelen;
}

int spdylay_frame_unpack_window_update(spdylay_window_update *frame,
                                       const uint8_t *head, size_t headlen,
                                       const uint8_t *payload,
                                       size_t payloadlen)
{
  if(payloadlen != 8) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  frame->stream_id = spdylay_get_uint32(payload) & SPDYLAY_STREAM_ID_MASK;
  frame->delta_window_size = spdylay_get_uint32(&payload[4]) &
    SPDYLAY_DELTA_WINDOW_SIZE_MASK;
  return 0;
}

ssize_t spdylay_frame_pack_settings(uint8_t **buf_ptr, size_t *buflen_ptr,
                                    spdylay_settings *frame)
{
  ssize_t framelen = SPDYLAY_FRAME_HEAD_LENGTH+frame->hd.length;
  size_t i;
  int r;
  if(frame->hd.version != SPDYLAY_PROTO_SPDY2 &&
     frame->hd.version != SPDYLAY_PROTO_SPDY3) {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  r = spdylay_reserve_buffer(buf_ptr, buflen_ptr, framelen);
  if(r != 0) {
    return r;
  }
  memset(*buf_ptr, 0, framelen);
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  spdylay_put_uint32be(&(*buf_ptr)[8], frame->niv);
  if(frame->hd.version == SPDYLAY_PROTO_SPDY2) {
    for(i = 0; i < frame->niv; ++i) {
      int off = i*8;
      /* spdy/2 spec says ID is network byte order, but publicly
         deployed server sends little endian host byte order. */
      (*buf_ptr)[12+off+0] = (frame->iv[i].settings_id) & 0xff;
      (*buf_ptr)[12+off+1] = (frame->iv[i].settings_id >> 8) & 0xff;
      (*buf_ptr)[12+off+2] = (frame->iv[i].settings_id >>16) & 0xff;
      (*buf_ptr)[15+off] = frame->iv[i].flags;
      spdylay_put_uint32be(&(*buf_ptr)[16+off], frame->iv[i].value);
    }
  } else {
    for(i = 0; i < frame->niv; ++i) {
      int off = i*8;
      spdylay_put_uint32be(&(*buf_ptr)[12+off], frame->iv[i].settings_id);
      (*buf_ptr)[12+off] = frame->iv[i].flags;
      spdylay_put_uint32be(&(*buf_ptr)[16+off], frame->iv[i].value);
    }
  }
  return framelen;
}

int spdylay_frame_unpack_settings(spdylay_settings *frame,
                                  const uint8_t *head, size_t headlen,
                                  const uint8_t *payload, size_t payloadlen)
{
  size_t i;
  if(payloadlen < 4) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  if(frame->hd.version != SPDYLAY_PROTO_SPDY2 &&
     frame->hd.version != SPDYLAY_PROTO_SPDY3) {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  frame->niv = spdylay_get_uint32(payload);
  if(payloadlen != 4+frame->niv*8) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  frame->iv = malloc(frame->niv*sizeof(spdylay_settings_entry));
  if(frame->iv == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  if(frame->hd.version == SPDYLAY_PROTO_SPDY2) {
    for(i = 0; i < frame->niv; ++i) {
      size_t off = i*8;
      /* ID is little endian. See comments in
         spdylay_frame_pack_settings(). */
      frame->iv[i].settings_id = payload[4+off] + (payload[4+off+1] << 8)
        +(payload[4+off+2] << 16);
      frame->iv[i].flags = payload[7+off];
      frame->iv[i].value = spdylay_get_uint32(&payload[8+off]);
    }
  } else {
    for(i = 0; i < frame->niv; ++i) {
      size_t off = i*8;
      frame->iv[i].settings_id = spdylay_get_uint32(&payload[4+off]) &
        SPDYLAY_SETTINGS_ID_MASK;
      frame->iv[i].flags = payload[4+off];
      frame->iv[i].value = spdylay_get_uint32(&payload[8+off]);
    }
  }
  return 0;
}

ssize_t spdylay_frame_pack_credential(uint8_t **buf_ptr, size_t *buflen_ptr,
                                      spdylay_credential *frame)
{
  ssize_t framelen;
  int r;
  size_t i, offset;
  framelen = SPDYLAY_FRAME_HEAD_LENGTH+2+4+frame->proof.length;
  for(i = 0; i < frame->ncerts; ++i) {
    framelen += 4+frame->certs[i].length;
  }
  r = spdylay_reserve_buffer(buf_ptr, buflen_ptr, framelen);
  if(r != 0) {
    return r;
  }
  memset(*buf_ptr, 0, framelen);
  spdylay_frame_pack_ctrl_hd(*buf_ptr, &frame->hd);
  offset = SPDYLAY_FRAME_HEAD_LENGTH;
  spdylay_put_uint16be(&(*buf_ptr)[offset], frame->slot);
  offset += 2;
  spdylay_put_uint32be(&(*buf_ptr)[offset], frame->proof.length);
  offset += 4;
  memcpy(&(*buf_ptr)[offset], frame->proof.data, frame->proof.length);
  offset += frame->proof.length;
  for(i = 0; i < frame->ncerts; ++i) {
    spdylay_put_uint32be(&(*buf_ptr)[offset], frame->certs[i].length);
    offset += 4;
    memcpy(&(*buf_ptr)[offset], frame->certs[i].data, frame->certs[i].length);
    offset += frame->certs[i].length;
  }
  return framelen;
}

/*
 * Counts number of client certificate in CREDENTIAL frame payload
 * |payload| with length |payloadlen|. The |payload| points to the
 * length of the first certificate. This function also checks the
 * frame payload is properly composed.
 *
 * This function returns the number of certificates in |payload| if it
 * succeeds, or one of the following negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The frame payload is invalid.
 */
static int spdylay_frame_count_unpack_cert(const uint8_t *payload,
                                           size_t payloadlen)
{
  size_t n, offset = 0;
  for(n = 1; 1; ++n) {
    size_t len;
    if(offset+4 > payloadlen) {
      return SPDYLAY_ERR_INVALID_FRAME;
    }
    len = spdylay_get_uint32(&payload[offset]);
    offset += 4;
    if(len > payloadlen || offset+len > payloadlen) {
      return SPDYLAY_ERR_INVALID_FRAME;
    } else {
      offset += len;
      if(offset == payloadlen) {
        break;
      }
    }
  }
  return n;
}

/*
 * Unpacks client certificates in the |payload| with length
 * |payloadlen|.  First allocates memory to store the |ncerts|
 * certificates.  Stores certificates in the allocated space and set
 * its pointer to |*certs_ptr|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory
 */
static int spdylay_frame_unpack_cert(spdylay_mem_chunk **certs_ptr,
                                     size_t ncerts,
                                     const uint8_t *payload, size_t payloadlen)
{
  size_t offset, i, j;
  spdylay_mem_chunk *certs;
  certs = malloc(sizeof(spdylay_mem_chunk)*ncerts);
  if(certs == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  offset = 0;
  for(i = 0; i < ncerts; ++i) {
    certs[i].length = spdylay_get_uint32(&payload[offset]);
    offset += 4;
    certs[i].data = malloc(certs[i].length);
    if(certs[i].data == NULL) {
      goto fail;
    }
    memcpy(certs[i].data, &payload[offset], certs[i].length);
    offset += certs[i].length;
  }
  *certs_ptr = certs;
  return 0;
 fail:
  for(j = 0; j < i; ++j) {
    free(certs[j].data);
  }
  free(certs);
  return SPDYLAY_ERR_NOMEM;
}

int spdylay_frame_unpack_credential(spdylay_credential *frame,
                                    const uint8_t *head, size_t headlen,
                                    const uint8_t *payload,
                                    size_t payloadlen)
{
  size_t offset;
  int rv;
  if(payloadlen < 10) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  spdylay_frame_unpack_ctrl_hd(&frame->hd, head);
  offset = 0;
  frame->slot = spdylay_get_uint16(&payload[offset]);
  offset += 2;
  frame->proof.length = spdylay_get_uint32(&payload[offset]);
  offset += 4;
  if(frame->proof.length > payloadlen ||
     offset+frame->proof.length > payloadlen) {
    return SPDYLAY_ERR_INVALID_FRAME;
  }
  frame->proof.data = malloc(frame->proof.length);
  if(frame->proof.data == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  memcpy(frame->proof.data, &payload[offset], frame->proof.length);
  offset += frame->proof.length;
  rv = spdylay_frame_count_unpack_cert(payload+offset, payloadlen-offset);
  if(rv < 0) {
    goto fail;
  }
  frame->ncerts = rv;
  rv = spdylay_frame_unpack_cert(&frame->certs, frame->ncerts,
                                 payload+offset, payloadlen-offset);
  if(rv != 0) {
    goto fail;
  }
  return 0;
 fail:
  free(frame->proof.data);
  return rv;
}

spdylay_settings_entry* spdylay_frame_iv_copy(const spdylay_settings_entry *iv,
                                              size_t niv)
{
  spdylay_settings_entry *iv_copy;
  size_t len = niv*sizeof(spdylay_settings_entry);
  iv_copy = malloc(len);
  if(iv_copy == NULL) {
    return NULL;
  }
  memcpy(iv_copy, iv, len);
  return iv_copy;
}

static int spdylay_settings_entry_compar(const void *lhs, const void *rhs)
{
  return ((spdylay_settings_entry *)lhs)->settings_id
    -((spdylay_settings_entry *)rhs)->settings_id;
}

void spdylay_frame_iv_sort(spdylay_settings_entry *iv, size_t niv)
{
  qsort(iv, niv, sizeof(spdylay_settings_entry), spdylay_settings_entry_compar);
}

ssize_t spdylay_frame_nv_offset(spdylay_frame_type type, uint16_t version)
{
  switch(type) {
  case SPDYLAY_SYN_STREAM:
    return SPDYLAY_SYN_STREAM_NV_OFFSET;
  case SPDYLAY_SYN_REPLY: {
    if(version == SPDYLAY_PROTO_SPDY2) {
      return SPDYLAY_SPDY2_SYN_REPLY_NV_OFFSET;
    } else if(version == SPDYLAY_PROTO_SPDY3) {
      return SPDYLAY_SPDY3_SYN_REPLY_NV_OFFSET;
    }
    break;
    }
  case SPDYLAY_HEADERS: {
    if(version == SPDYLAY_PROTO_SPDY2) {
      return SPDYLAY_SPDY2_HEADERS_NV_OFFSET;
    } else if(version == SPDYLAY_PROTO_SPDY3) {
      return SPDYLAY_SPDY3_HEADERS_NV_OFFSET;
    }
    break;
  }
  default:
    break;
  }
  return -1;
}

int spdylay_frame_nv_check_null(const char **nv)
{
  size_t i, j;
  for(i = 0; nv[i]; i += 2) {
    if(nv[i][0] == '\0' || nv[i+1] == NULL) {
      return 0;
    }
    for(j = 0; nv[i][j]; ++j) {
      unsigned char c = nv[i][j];
      if(c < 0x20 || c > 0x7e) {
        return 0;
      }
    }
  }
  return 1;
}
