/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl kernel / service run-time system call ABI.
 * This file defines nacl target machine dependent types.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_MACHINE__TYPES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_MACHINE__TYPES_H_

#ifdef __native_client__
# include <stdint.h>
# include <machine/_default_types.h>
#else
# include "native_client/src/include/portability.h"
#endif

#define __need_size_t
#include <stddef.h>

#ifndef NULL
#define NULL 0
#endif

#define WORDSIZE 32

#define MAKE_WORDSIZE_TYPE(T)  T ## WORDSIZE ## _t
#define SIGNED_WORD            MAKE_WORDSIZE_TYPE(int)
#define UNSIGNED_WORD          MAKE_WORDSIZE_TYPE(uint)

#ifndef __native_client__
# if (WORDSIZE == NACL_HOST_WORDSIZE)
#   define WORDSIZE_IS_NATIVE 1
# else
#   define WORDSIZE_IS_NATIVE 0
# endif
#endif

#define NACL_CONCAT3_(a, b, c) a ## b ## c
#define NACL_PRI_(fmt, size) NACL_CONCAT3_(NACL_PRI, fmt, size)

#ifndef __dev_t_defined
#define __dev_t_defined
typedef int64_t __dev_t;
#ifndef __native_client__
typedef __dev_t dev_t;
#endif
#endif

#define NACL_PRIdNACL_DEV NACL_PRI_(d, 64)
#define NACL_PRIiNACL_DEV NACL_PRI_(i, 64)
#define NACL_PRIoNACL_DEV NACL_PRI_(o, 64)
#define NACL_PRIuNACL_DEV NACL_PRI_(u, 64)
#define NACL_PRIxNACL_DEV NACL_PRI_(x, 64)
#define NACL_PRIXNACL_DEV NACL_PRI_(X, 64)

#define DEV_T_MIN ((dev_t) 1 << 63)
#define DEV_T_MAX (~((dev_t) 1 << 63))

#ifndef __ino_t_defined
#define __ino_t_defined
typedef uint64_t __ino_t;
#ifndef __native_client__
typedef __ino_t ino_t;
#endif
#endif

#define NACL_PRIdNACL_INO NACL_PRI_(d, 64)
#define NACL_PRIiNACL_INO NACL_PRI_(i, 64)
#define NACL_PRIoNACL_INO NACL_PRI_(o, 64)
#define NACL_PRIuNACL_INO NACL_PRI_(u, 64)
#define NACL_PRIxNACL_INO NACL_PRI_(x, 64)
#define NACL_PRIXNACL_INO NACL_PRI_(X, 64)

#define INO_T_MIN ((ino_t) 0)
#define INO_T_MAX ((ino_t) -1)

#ifndef __mode_t_defined
#define __mode_t_defined
typedef uint32_t __mode_t;
#ifndef __native_client__
typedef __mode_t mode_t;
#endif
#endif

#define NACL_PRIdNACL_MODE NACL_PRI_(d, WORDSIZE)
#define NACL_PRIiNACL_MODE NACL_PRI_(i, WORDSIZE)
#define NACL_PRIoNACL_MODE NACL_PRI_(o, WORDSIZE)
#define NACL_PRIuNACL_MODE NACL_PRI_(u, WORDSIZE)
#define NACL_PRIxNACL_MODE NACL_PRI_(x, WORDSIZE)
#define NACL_PRIXNACL_MODE NACL_PRI_(X, WORDSIZE)

#define MODE_T_MIN ((mode_t) 0)
#define MODE_T_MAX ((mode_t) -1)

#ifndef __nlink_t_defined
#define __nlink_t_defined
typedef uint32_t __nlink_t;
#ifndef __native_client__
typedef __nlink_t nlink_t;
#endif
#endif

#define NACL_PRIdNACL_NLINK NACL_PRI_(d, WORDSIZE)
#define NACL_PRIiNACL_NLINK NACL_PRI_(i, WORDSIZE)
#define NACL_PRIoNACL_NLINK NACL_PRI_(o, WORDSIZE)
#define NACL_PRIuNACL_NLINK NACL_PRI_(u, WORDSIZE)
#define NACL_PRIxNACL_NLINK NACL_PRI_(x, WORDSIZE)
#define NACL_PRIXNACL_NLINK NACL_PRI_(X, WORDSIZE)

#define NLINK_T_MIN ((nlink_t) 0)
#define NLINK_T_MAX ((nlink_t) -1)

#ifndef __uid_t_defined
#define __uid_t_defined
typedef uint32_t __uid_t;
#ifndef __native_client__
typedef __uid_t uid_t;
#endif
#endif

#define NACL_PRIdNACL_UID NACL_PRI_(d, WORDSIZE)
#define NACL_PRIiNACL_UID NACL_PRI_(i, WORDSIZE)
#define NACL_PRIoNACL_UID NACL_PRI_(o, WORDSIZE)
#define NACL_PRIuNACL_UID NACL_PRI_(u, WORDSIZE)
#define NACL_PRIxNACL_UID NACL_PRI_(x, WORDSIZE)
#define NACL_PRIXNACL_UID NACL_PRI_(X, WORDSIZE)

#define UID_T_MIN ((uid_t) 0)
#define UID_T_MAX ((uid_t) -1)

#ifndef __gid_t_defined
#define __gid_t_defined
typedef uint32_t __gid_t;
#ifndef __native_client__
typedef __gid_t gid_t;
#endif
#endif

#define NACL_PRIdNACL_GID NACL_PRI_(d, WORDSIZE)
#define NACL_PRIiNACL_GID NACL_PRI_(i, WORDSIZE)
#define NACL_PRIoNACL_GID NACL_PRI_(o, WORDSIZE)
#define NACL_PRIuNACL_GID NACL_PRI_(u, WORDSIZE)
#define NACL_PRIxNACL_GID NACL_PRI_(x, WORDSIZE)
#define NACL_PRIXNACL_GID NACL_PRI_(X, WORDSIZE)

#define GID_T_MIN ((gid_t) 0)
#define GID_T_MAX ((gid_t) -1)

#ifndef __off_t_defined
#define __off_t_defined
typedef int64_t _off_t;
#ifndef __native_client__
typedef _off_t off_t;
#endif
#endif

#define NACL_PRIdNACL_OFF NACL_PRI_(d, 64)
#define NACL_PRIiNACL_OFF NACL_PRI_(i, 64)
#define NACL_PRIoNACL_OFF NACL_PRI_(o, 64)
#define NACL_PRIuNACL_OFF NACL_PRI_(u, 64)
#define NACL_PRIxNACL_OFF NACL_PRI_(x, 64)
#define NACL_PRIXNACL_OFF NACL_PRI_(X, 64)

#define OFF_T_MIN ((off_t) 1 << 63)
#define OFF_T_MAX (~((off_t) 1 << 63))

#ifndef __off64_t_defined
#define __off64_t_defined
typedef int64_t _off64_t;
#ifndef __native_client__
typedef _off64_t off64_t;
#endif
#endif

#define NACL_PRIdNACL_OFF64 NACL_PRI_(d, 64)
#define NACL_PRIiNACL_OFF64 NACL_PRI_(i, 64)
#define NACL_PRIoNACL_OFF64 NACL_PRI_(o, 64)
#define NACL_PRIuNACL_OFF64 NACL_PRI_(u, 64)
#define NACL_PRIxNACL_OFF64 NACL_PRI_(x, 64)
#define NACL_PRIXNACL_OFF64 NACL_PRI_(X, 64)

#define OFF64_T_MIN ((off64_t) 1 << 63)
#define OFF64_T_MAX (~((off64_t) 1 << 63))


#if !(defined(__GLIBC__) && defined(__native_client__))

#ifndef __blksize_t_defined
#define __blksize_t_defined
typedef int32_t __blksize_t;
typedef __blksize_t blksize_t;
#endif

#define NACL_PRIdNACL_BLKSIZE NACL_PRI_(d, WORDSIZE)
#define NACL_PRIiNACL_BLKSIZE NACL_PRI_(i, WORDSIZE)
#define NACL_PRIoNACL_BLKSIZE NACL_PRI_(o, WORDSIZE)
#define NACL_PRIuNACL_BLKSIZE NACL_PRI_(u, WORDSIZE)
#define NACL_PRIxNACL_BLKSIZE NACL_PRI_(x, WORDSIZE)
#define NACL_PRIXNACL_BLKSIZE NACL_PRI_(X, WORDSIZE)

#endif


#define BLKSIZE_T_MIN \
  ((blksize_t) 1 << (WORDSIZE - 1))
#define BLKSIZE_T_MAX \
  (~((blksize_t) 1 << (WORDSIZE - 1)))

#ifndef __blkcnt_t_defined
#define __blkcnt_t_defined
typedef int32_t __blkcnt_t;
typedef __blkcnt_t blkcnt_t;
#endif

#define NACL_PRIdNACL_BLKCNT NACL_PRI_(d, WORDSIZE)
#define NACL_PRIiNACL_BLKCNT NACL_PRI_(i, WORDSIZE)
#define NACL_PRIoNACL_BLKCNT NACL_PRI_(o, WORDSIZE)
#define NACL_PRIuNACL_BLKCNT NACL_PRI_(u, WORDSIZE)
#define NACL_PRIxNACL_BLKCNT NACL_PRI_(x, WORDSIZE)
#define NACL_PRIXNACL_BLKCNT NACL_PRI_(X, WORDSIZE)

#define BLKCNT_T_MIN \
  ((blkcnt_t) 1 << (WORDSIZE - 1))
#define BLKCNT_T_MAX \
  (~((blkcnt_t) 1 << (WORDSIZE - 1)))

#ifndef __time_t_defined
#define __time_t_defined
typedef int64_t       __time_t;
typedef __time_t time_t;

struct timespec {
  time_t tv_sec;
#ifdef __native_client__
  long int        tv_nsec;
#else
  int32_t         tv_nsec;
#endif
};
#endif

#define NACL_PRIdNACL_TIME NACL_PRI_(d, 64)
#define NACL_PRIiNACL_TIME NACL_PRI_(i, 64)
#define NACL_PRIoNACL_TIME NACL_PRI_(o, 64)
#define NACL_PRIuNACL_TIME NACL_PRI_(u, 64)
#define NACL_PRIxNACL_TIME NACL_PRI_(x, 64)
#define NACL_PRIXNACL_TIME NACL_PRI_(X, 64)

#define TIME_T_MIN ((time_t) 1 << 63)
#define TIME_T_MAX (~((time_t) 1 << 63))

/*
 * stddef.h defines size_t, and we cannot export another definition.
 * see __need_size_t above and stddef.h
 * (BUILD/gcc-4.2.2/gcc/ginclude/stddef.h) contents.
 */
#define NACL_NO_STRIP(t) nacl_ ## abi_ ## t

/*
 * NO_STRIP blocks stripping of NACL_ABI_ from any symbols between the
 * #define and #undef.  This causes NACL_ABI_WORDSIZE to be undefined below
 * in the NACL_PRI?NACL_SIZE macros.  If it is not defined and we are in
 * __native_client__, then define it in terms of WORDSIZE, which is the
 * stripped result of the #define above.
 */
#if defined(__native_client__) && !defined(NACL_ABI_WORDSIZE)
#define NACL_ABI_WORDSIZE WORDSIZE
#endif  /* defined(__native_client__) && !defined(NACL_ABI_WORDSIZE) */

#ifndef nacl_abi_size_t_defined
#define nacl_abi_size_t_defined
typedef uint32_t NACL_NO_STRIP(size_t);
#endif

#define NACL_ABI_SIZE_T_MIN ((nacl_abi_size_t) 0)
#define NACL_ABI_SIZE_T_MAX ((nacl_abi_size_t) -1)

#ifndef nacl_abi_ssize_t_defined
#define nacl_abi_ssize_t_defined
typedef int32_t NACL_NO_STRIP(ssize_t);
#endif

#define NACL_ABI_SSIZE_T_MAX \
  ((nacl_abi_ssize_t) (NACL_ABI_SIZE_T_MAX >> 1))
#define NACL_ABI_SSIZE_T_MIN \
  (~NACL_ABI_SSIZE_T_MAX)

#define NACL_PRIdNACL_SIZE NACL_PRI_(d, NACL_ABI_WORDSIZE)
#define NACL_PRIiNACL_SIZE NACL_PRI_(i, NACL_ABI_WORDSIZE)
#define NACL_PRIoNACL_SIZE NACL_PRI_(o, NACL_ABI_WORDSIZE)
#define NACL_PRIuNACL_SIZE NACL_PRI_(u, NACL_ABI_WORDSIZE)
#define NACL_PRIxNACL_SIZE NACL_PRI_(x, NACL_ABI_WORDSIZE)
#define NACL_PRIXNACL_SIZE NACL_PRI_(X, NACL_ABI_WORDSIZE)

/**
 * Inline functions to aid in conversion between system (s)size_t and
 * nacl_abi_(s)size_t
 *
 * These are defined as inline functions only if __native_client__ is
 * undefined, since in a nacl module size_t and nacl_abi_size_t are always
 * the same (and we don't have a definition for INLINE, so these won't compile)
 *
 * If __native_client__ *is* defined, these turn into no-ops.
 */
#ifdef __native_client__
  /**
   * NB: The "no-op" version of these functions does NO type conversion.
   * Please DO NOT CHANGE THIS. If you get a type error using these functions
   * in a NaCl module, it's a real error and should be fixed in your code.
   */
  #define nacl_abi_size_t_saturate(x) (x)
  #define nacl_abi_ssize_t_saturate(x) (x)
#else /* __native_client */
  static INLINE nacl_abi_size_t nacl_abi_size_t_saturate(size_t x) {
    if (x > NACL_ABI_SIZE_T_MAX) {
      return NACL_ABI_SIZE_T_MAX;
    } else {
      return (nacl_abi_size_t)x;
    }
  }

  static INLINE nacl_abi_ssize_t nacl_abi_ssize_t_saturate(ssize_t x) {
    if (x > NACL_ABI_SSIZE_T_MAX) {
      return NACL_ABI_SSIZE_T_MAX;
    } else if (x < NACL_ABI_SSIZE_T_MIN) {
      return NACL_ABI_SSIZE_T_MIN;
    } else {
      return (nacl_abi_ssize_t) x;
    }
  }
#endif /* __native_client */

#undef NACL_NO_STRIP

#endif
