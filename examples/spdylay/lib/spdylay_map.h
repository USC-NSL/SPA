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
#ifndef SPDYLAY_MAP_H
#define SPDYLAY_MAP_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <spdylay/spdylay.h>
#include "spdylay_int.h"

/* Implementation of ordered map */

typedef uint32_t key_type;
typedef uint32_t pri_type;

typedef struct spdylay_map_entry {
  key_type key;
  struct spdylay_map_entry *parent, *left, *right;
  pri_type priority;
} spdylay_map_entry;

typedef struct {
  spdylay_map_entry *root;
  size_t size;
} spdylay_map;

/*
 * Initializes the map |map|.
 */
void spdylay_map_init(spdylay_map *map);

/*
 * Deallocates any resources allocated for |map|. The stored entries
 * are not freed by this function. Use spdylay_map_each_free() to free
 * each entries.
 */
void spdylay_map_free(spdylay_map *map);

/*
 * Deallocates each entries using |func| function and any resources
 * allocated for |map|. The |func| function is responsible for freeing
 * given the |entry| object. The |ptr| will be passed to the |func| as
 * send argument. The return value of the |func| will be ignored.
 */
void spdylay_map_each_free(spdylay_map *map,
                           int (*func)(spdylay_map_entry *entry, void *ptr),
                           void *ptr);

/*
 * Initializes the |entry| with the |key|. All entries to be inserted
 * to the map must be initialized with this function.
 */
void spdylay_map_entry_init(spdylay_map_entry *entry, key_type key);

/*
 * Inserts the new |entry| with the key |entry->key| to the map |map|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error code:
 *
 * SPDYLAY_ERR_INVALID_ARGUMENT
 *     The item associated by |key| already exists.
 */
int spdylay_map_insert(spdylay_map *map, spdylay_map_entry *entry);

/*
 * Returns the entry associated by the key |key|.  If there is no such
 * entry, this function returns NULL.
 */
spdylay_map_entry* spdylay_map_find(spdylay_map *map, key_type key);

/*
 * Removes the entry associated by the key |key| from the |map|.  The
 * removed entry is not freed by this function.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_ARGUMENT
 *     The entry associated by |key| does not exist.
 */
int spdylay_map_remove(spdylay_map *map, key_type key);

/*
 * Returns the number of items stored in the map |map|.
 */
size_t spdylay_map_size(spdylay_map *map);

/*
 * Applies the function |func| to each entry in the |map| with the
 * optional user supplied pointer |ptr|.
 *
 * If the |func| returns 0, this function calls the |func| with the
 * next entry. If the |func| returns nonzero, it will not call the
 * |func| for further entries and return the return value of the
 * |func| immediately.  Thus, this function returns 0 if all the
 * invocations of the |func| return 0, or nonzero value which the last
 * invocation of |func| returns.
 *
 * Don't use this function to free each entry. Use
 * spdylay_map_each_free() instead.
 */
int spdylay_map_each(spdylay_map *map,
                     int (*func)(spdylay_map_entry *entry, void *ptr),
                     void *ptr);

#endif /* SPDYLAY_MAP_H */
