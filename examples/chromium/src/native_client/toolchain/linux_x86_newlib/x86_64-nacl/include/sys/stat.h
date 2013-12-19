/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl kernel / service run-time system call ABI.  stat/fstat.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_SYS_STAT_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_SYS_STAT_H_

#ifdef __native_client__
#include <sys/types.h>
#else
#include <machine/_types.h>

#endif
#include <bits/stat.h>


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Linux <bit/stat.h> uses preprocessor to define st_atime to
 * st_atim.tv_sec etc to widen the ABI to use a struct timespec rather
 * than just have a time_t access/modification/inode-change times.
 * this is unfortunate, since that symbol cannot be used as a struct
 * member elsewhere (!).
 *
 * just like with type name conflicts, we avoid it by using 
 * as a prefix for struct members too.  sigh.
 */

struct stat {  /* must be renamed when ABI is exported */
  dev_t     st_dev;       /* not implemented */
  ino_t     st_ino;       /* not implemented */
  mode_t    st_mode;      /* partially implemented. */
  nlink_t   st_nlink;     /* link count */
  uid_t     st_uid;       /* not implemented */
  gid_t     st_gid;       /* not implemented */
  dev_t     st_rdev;      /* not implemented */
  off_t     st_size;      /* object size */
  blksize_t st_blksize;   /* not implemented */
  blkcnt_t  st_blocks;    /* not implemented */
  time_t    st_atime;     /* access time */
  int64_t            st_atimensec; /* possibly just pad */
  time_t    st_mtime;     /* modification time */
  int64_t            st_mtimensec; /* possibly just pad */
  time_t    st_ctime;     /* inode change time */
  int64_t            st_ctimensec; /* possibly just pad */
};

#ifdef __native_client__
extern int stat(char const *path, struct stat *stbuf);
extern int fstat(int d, struct stat *stbuf);
extern int mkdir(const char *path, mode_t mode);
#endif

#ifdef __cplusplus
}
#endif

#endif
