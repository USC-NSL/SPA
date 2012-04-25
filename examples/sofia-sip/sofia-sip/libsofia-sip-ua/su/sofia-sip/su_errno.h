/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef SU_ERRNO_H
/** Defined when <sofia-sip/su_errno.h> has been included. */
#define SU_ERRNO_H

/**@file sofia-sip/su_errno.h Errno handling
 *
 * Source-code compatibility with Windows (having separate errno for
 * socket library and C libraries).
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Dec 22 18:16:06 EET 2005 pessi
 */

#ifndef SU_CONFIG_H
#include "sofia-sip/su_config.h"
#endif

#include <errno.h>

SOFIA_BEGIN_DECLS

/** Return string describing su error code. */
SOFIAPUBFUN char const *su_strerror(int e);

/** The latest su error. */
SOFIAPUBFUN int su_errno(void);

/** Set the su error. */
SOFIAPUBFUN int su_seterrno(int);

#if !SU_HAVE_WINSOCK
#define su_errno() (errno)
#define su_seterrno(n) ((errno = (n)), -1)
#endif

#if defined(__APPLE_CC__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
#ifndef EBADMSG
#define EBADMSG EFAULT
#endif
#ifndef EPROTO
#define EPROTO EPROTOTYPE
#endif
#ifndef EBADMSG
#define EBADMSG EFAULT
#endif
#endif

#if SU_HAVE_WINSOCK
/*
 * Use WinSock errors with Sofia-SIP.
 *
 * VC POSIX runtime defines some of these, undef POSIX definitions.
 */

#undef EWOULDBLOCK
#define EWOULDBLOCK  (10035) /* WSAEWOULDBLOCK */

#undef EINPROGRESS
#define EINPROGRESS  (10036) /* WSAEINPROGRESS */

#undef EALREADY
#define EALREADY (10037) /* WSAEALREADY */

#undef ENOTSOCK
#define ENOTSOCK (10038) /* WSAENOTSOCK */

#undef EDESTADDRREQ
#define EDESTADDRREQ (10039) /* WSAEDESTADDRREQ */

#undef EMSGSIZE
#define EMSGSIZE (10040) /* WSAEMSGSIZE */

#undef EPROTOTYPE
#define EPROTOTYPE (10041) /* WSAEPROTOTYPE */

#undef ENOPROTOOPT
#define ENOPROTOOPT (10042) /* WSAENOPROTOOPT */

#undef EPROTONOSUPPORT
#define EPROTONOSUPPORT (10043) /* WSAEPROTONOSUPPORT */

#undef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT (10044) /* WSAESOCKTNOSUPPORT */

#undef EOPNOTSUPP
#define EOPNOTSUPP (10045) /* WSAEOPNOTSUPP */

#undef EPFNOSUPPORT
#define EPFNOSUPPORT (10046) /* WSAEPFNOSUPPORT */

#undef EAFNOSUPPORT
#define EAFNOSUPPORT (10047) /* WSAEAFNOSUPPORT */

#undef EADDRINUSE
#define EADDRINUSE (10048) /* WSAEADDRINUSE */

#undef EADDRNOTAVAIL
#define EADDRNOTAVAIL (10049) /* WSAEADDRNOTAVAIL */

#undef ENETDOWN
#define ENETDOWN (10050) /* WSAENETDOWN */

#undef ENETUNREACH
#define ENETUNREACH (10051) /* WSAENETUNREACH */

#undef ENETRESET
#define ENETRESET (10052) /* WSAENETRESET */

#undef ECONNABORTED
#define ECONNABORTED (10053) /* WSAECONNABORTED */

#undef ECONNRESET
#define ECONNRESET (10054) /* WSAECONNRESET */

#undef ENOBUFS
#define ENOBUFS (10055) /* WSAENOBUFS */

#undef EISCONN
#define EISCONN (10056) /* WSAEISCONN */

#undef ENOTCONN
#define ENOTCONN (10057) /* WSAENOTCONN */

#undef ESHUTDOWN
#define ESHUTDOWN (10058) /* WSAESHUTDOWN */

#undef ETOOMANYREFS
#define ETOOMANYREFS (10059) /* WSAETOOMANYREFS */

#undef ETIMEDOUT
#define ETIMEDOUT (10060) /* WSAETIMEDOUT */

#undef ECONNREFUSED
#define ECONNREFUSED (10061) /* WSAECONNREFUSED */

#undef ELOOP
#define ELOOP (10062) /* WSAELOOP */

#undef ENAMETOOLONG
#define ENAMETOOLONG (10063) /* WSAENAMETOOLONG */

#undef EHOSTDOWN
#define EHOSTDOWN (10064) /* WSAEHOSTDOWN */

#undef EHOSTUNREACH
#define EHOSTUNREACH (10065) /* WSAEHOSTUNREACH */

#undef ENOTEMPTY
#define ENOTEMPTY (10066) /* WSAENOTEMPTY */

#undef EPROCLIM
#define EPROCLIM (10067) /* WSAEPROCLIM */

#undef EUSERS
#define EUSERS (10068) /* WSAEUSERS */

#undef EDQUOT
#define EDQUOT (10069) /* WSAEDQUOT */

#undef ESTALE
#define ESTALE (10070) /* WSAESTALE */

#undef EREMOTE
#define EREMOTE (10071) /* WSAEREMOTE */

#undef EBADMSG

#  if defined(WSABADMSG)
#   define EBADMSG (WSAEBADMSG)
#  else
#   define EBADMSG (20005)
#  endif

#undef EPROTO

#  if defined(WSAEPROTO)
#    define EPROTO WSAEPROTO
#  else
#    define EPROTO (20006)
#  endif

#endif

SOFIA_END_DECLS

#endif
