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

/**@ingroup su_time
 * @CFILE su_time0.c
 * @brief su_time() implementation
 *
 * The file su_time0.c contains implementation of OS-independent wallclock
 * time with microsecond resolution.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Jari Selin <Jari.Selin@nokia.com>
 *
 * @date Created: Fri May 10 18:13:19 2002 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "sofia-sip/su_types.h"
#include "sofia-sip/su_time.h"
#include "su_module_debug.h"

#include <time.h>

#if HAVE_SYS_TIME_H
#include <sys/time.h> /* Get struct timeval */
#endif

#if defined(__MINGW32__)
#define HAVE_FILETIME 1
#include <windows.h>
#endif

#if HAVE_FILETIME
#define HAVE_FILETIME 1
#include <windows.h>
#endif

/** Seconds from 1.1.1900 to 1.1.1970 */
#define NTP_EPOCH 2208988800UL
#define E9 (1000000000U)
#define E7 (10000000U)

static const su_time64_t su_res64 = (su_time64_t)1000000UL;

/* Hooks for testing timing and timers */
static void noop(su_time_t *tv)
{
  (void)tv;
}

void (*_su_time)(su_time_t *tv) = noop;

su_time64_t (*_su_nanotime)(su_time64_t *);
su_time64_t (*_su_monotime)(su_time64_t *);

/** Get current time.
 *
 * The function @c su_time() fills its argument with the current NTP
 * timestamp expressed as a su_time_t structure.
 *
 * @param tv pointer to the timeval object
 */
void su_time(su_time_t *tv)
{
#if HAVE_CLOCK_GETTIME
  if (tv) {
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    tv->tv_sec = ts.tv_sec + NTP_EPOCH;
    tv->tv_usec = ts.tv_nsec / 1000;
  }
#elif HAVE_GETTIMEOFDAY
  if (tv) {
    gettimeofday((struct timeval *)tv, NULL);
    tv->tv_sec += NTP_EPOCH;
  }
#elif HAVE_FILETIME
  union {
    FILETIME       ft[1];
    ULARGE_INTEGER ull[1];
  } date;

  GetSystemTimeAsFileTime(date.ft);

  tv->tv_usec = (unsigned long) ((date.ull->QuadPart % E7) / 10);
  tv->tv_sec = (unsigned long) ((date.ull->QuadPart / E7) -
    /* 1900-Jan-01 - 1601-Jan-01: 299 years, 72 leap years */
    (299 * 365 + 72) * 24 * 60 * (uint64_t)60);
#else
#error no su_time() implementation
#endif

  _su_time(tv);
}

static su_time64_t now64(void)
{
#if HAVE_CLOCK_GETTIME
  {
    struct timespec tv;

    if (clock_gettime(CLOCK_REALTIME, &tv) == 0) {
      return ((su_time64_t)tv.tv_sec + NTP_EPOCH) * E9 + tv.tv_nsec;
    }
  }
#endif

#if HAVE_FILETIME
  {
    union {
      FILETIME       ft[1];
      ULARGE_INTEGER ull[1];
    } date;

    GetSystemTimeAsFileTime(date.ft);

    return 100 *
      (date.ull->QuadPart -
       /* 1900-Jan-01 - 1601-Jan-01: 299 years, 72 leap years */
       ((su_time64_t)(299 * 365 + 72) * 24 * 60 * 60) * E7);
  }
#elif HAVE_GETTIMEOFDAY
  {
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return ((su_time64_t)tv.tv_sec + NTP_EPOCH) * E9 + tv.tv_usec * 1000;
  }
#else
  return ((su_time64_t)time() + NTP_EPOCH) * E9;
#endif
}

/** Get current time as nanoseconds since epoch.
 *
 * Return the current NTP timestamp expressed as nanoseconds since epoch
 * (January 1st 1900).
 *
 * @param return_time optional pointer to current time to return
 *
 * @return Nanoseconds since epoch
 */
su_time64_t su_nanotime(su_time64_t *return_time)
{
  su_time64_t now = now64();

  if (return_time)
    *return_time = now;
  else
    return_time = &now;

  if (_su_nanotime)
    return (*_su_nanotime)(return_time);

  return now;
}

/** Get current time as nanoseconds since epoch.
 *
 * Return the current NTP timestamp expressed as nanoseconds since epoch
 * (January 1st 1900).
 *
 * @return Nanoseconds since epoch
 *
 * @NEW_UNRELEASED
 */
su_time64_t su_now64(void)
{
  su_time64_t now = now64();

  if (_su_nanotime)
    return (*_su_nanotime)(&now);

  return now;
}

static su_time64_t mono64(void)
{
#if HAVE_CLOCK_GETTIME && CLOCK_MONOTONIC
  {
    struct timespec tv;

    if (clock_gettime(CLOCK_MONOTONIC, &tv) == 0)
      return (su_time64_t)tv.tv_sec * E9 + tv.tv_nsec;
  }
#endif

#if HAVE_NANOUPTIME
  {
    struct timespec tv;

    nanouptime(&tv);

    return (su_time64_t)tv.tv_sec * E9 + tv.tv_nsec;
  }
#elif HAVE_MICROUPTIME
  {
    struct timeval tv;

    microuptime(&tv);

    return (su_time64_t)tv.tv_sec * E9 + tv.tv_usec * 1000;
  }
#endif

  return now64();
}

/** Get current monotonic time as nanoseconds.
 *
 * Return the current time expressed as nanoseconds. The time returned is
 * monotonic and never goes back - if the underlying system supports such a
 * clock.
 *
 * @param return_time optional pointer to return the current time
 *
 * @return Current time as nanoseconds
 */
su_time64_t su_monotime(su_time64_t *return_time)
{
  su_time64_t now = mono64();

  if (return_time)
    *return_time = now;
  else
    return_time = &now;

  if (_su_monotime)
    return (*_su_monotime)(return_time);

  return now;
}

/** Get current monotonic timestamp as nanoseconds.
 *
 * Return the current timestamp expressed as nanoseconds. The time
 * returned is monotonic and never goes back - if the underlying system
 * supports such a clock.
 *
 * @return Current timestamp as nanoseconds
 *
 * @NEW_UNRELEASED
 */
su_time64_t su_stamp64(void)
{
  su_time64_t now = mono64();

  if (_su_monotime)
    return (*_su_monotime)(&now);

  return now;
}
