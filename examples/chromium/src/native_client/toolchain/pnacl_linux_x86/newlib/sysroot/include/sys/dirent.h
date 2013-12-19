/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_SYS_DIRENT_H
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_SYS_DIRENT_H

#ifdef __native_client__
#include <sys/types.h>

#else
#include <machine/_types.h>

#endif

#ifdef __native_client__
/* check the compiler toolchain */
# ifdef MAXNAMLEN
#  if MAXNAMLEN != 255
#   error "MAXNAMLEN inconsistent"
#  endif
#  ifdef NAME_MAX
#   if NAME_MAX != 255
#    error "NAME_MAX inconsistent"
#   endif
#  endif
# else
#  define MAXNAMLEN 255
# endif
#else /* __native_client__ */
# define MAXNAMLEN 255
#endif

/*
 * _dirdesc contains the state used by opendir/readdir/closedir.
 */
typedef struct _dirdesc {
  int   dd_fd;
  long  dd_loc;
  long  dd_size;
  char  *dd_buf;
  int   dd_len;
  long  dd_seek;
} DIR;

/*
 * dirent represents a single directory entry.
 */
struct dirent {
  ino_t d_ino;
  off_t d_off;
  uint16_t       d_reclen;
  char           d_name[MAXNAMLEN + 1];
};

/*
 * external function declarations
 */
extern DIR           *opendir(const char *dirpath);
extern struct dirent *readdir(DIR *direntry);
extern int                    closedir(DIR *direntry);

#endif
