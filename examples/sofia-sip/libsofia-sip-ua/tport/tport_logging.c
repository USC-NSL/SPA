/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

/**@CFILE tport_logging.c Logging transported messages.
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Mar 24 08:45:49 EET 2006 ppessi
 */

#include "config.h"

#include "tport_internal.h"

#include <sofia-sip/su_string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

/**@var TPORT_LOG
 *
 * Environment variable determining if parsed message contents are logged.
 *
 * If the TPORT_LOG environment variable is set, the tport module logs the
 * contents of parsed messages. This eases debugging the signaling greatly.
 *
 * @sa TPORT_DUMP, TPORT_DEBUG, tport_log
 */
#if DOXYGEN_ONLY
char const TPORT_LOG[];	/* dummy declaration for Doxygen */
#endif

/**@var TPORT_DUMP
 *
 * Environment variable for transport data dump.
 *
 * The received and sent data is dumped to the file specified by TPORT_DUMP
 * environment variable. This can be used to save message traces and help
 * hairy debugging tasks.
 *
 * @sa TPORT_LOG, TPORT_DEBUG, tport_log
 */
#if DOXYGEN_ONLY
char const TPORT_DUMP[];	/* dummy declaration for Doxygen */
#endif

/**@var TPORT_DEBUG
 *
 * Environment variable determining the debug log level for @b tport module.
 *
 * The TPORT_DEBUG environment variable is used to determine the debug logging
 * level for @b tport module. The default level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, tport_log, SOFIA_DEBUG
 */
#if DOXYGEN_ONLY
char const TPORT_DEBUG[]; /* dummy declaration for Doxygen */
#endif

/**Debug log for @b tport module.
 *
 * The tport_log is the log object used by @b tport module. The level of
 * #tport_log is set using #TPORT_DEBUG environment variable.
 */
su_log_t tport_log[] = {
  SU_LOG_INIT("tport", "TPORT_DEBUG", SU_DEBUG)
};


/** Initialize logging. */
int tport_open_log(tport_master_t *mr, tagi_t *tags)
{
  int log_msg = mr->mr_log != 0;
  char const *dump = NULL;
  int n;

  n = tl_gets(tags,
	      TPTAG_LOG_REF(log_msg),
	      TPTAG_DUMP_REF(dump),
	      TAG_END());

  if (getenv("MSG_STREAM_LOG") != NULL || getenv("TPORT_LOG") != NULL)
    log_msg = 1;
  mr->mr_log = log_msg ? MSG_DO_EXTRACT_COPY : 0;

  if (getenv("MSG_DUMP"))
    dump = getenv("MSG_DUMP");
  if (getenv("TPORT_DUMP"))
    dump = getenv("TPORT_DUMP");

  if (dump) {
    time_t now;
    char *dumpname;

    if (mr->mr_dump && strcmp(dump, mr->mr_dump) == 0)
      return n;
    dumpname = su_strdup(mr->mr_home, dump);
    if (dumpname == NULL)
      return n;
    su_free(mr->mr_home, mr->mr_dump);
    mr->mr_dump = dumpname;

    if (mr->mr_dump_file && mr->mr_dump_file != stdout)
      fclose(mr->mr_dump_file), mr->mr_dump_file = NULL;

    if (strcmp(dumpname, "-"))
      mr->mr_dump_file = fopen(dumpname, "ab"); /* XXX */
    else
      mr->mr_dump_file = stdout;

    if (mr->mr_dump_file) {
      time(&now);
      fprintf(mr->mr_dump_file, "dump started at %s\n\n", ctime(&now));
    }
  }

  return n;
}

/** Create log stamp */
void tport_stamp(tport_t const *self, msg_t *msg,
		 char stamp[128], char const *what,
		 size_t n, char const *via,
		 su_time_t now)
{
  char label[24] = "";
  char *comp = "";
  char name[SU_ADDRSIZE] = "";
  su_sockaddr_t const *su;
  unsigned short second, minute, hour;

  assert(self); assert(msg);

  second = (unsigned short)(now.tv_sec % 60);
  minute = (unsigned short)((now.tv_sec / 60) % 60);
  hour = (unsigned short)((now.tv_sec / 3600) % 24);

  su = msg_addr(msg);

#if SU_HAVE_IN6
  if (su->su_family == AF_INET6) {
    if (su->su_sin6.sin6_flowinfo)
      snprintf(label, sizeof(label), "/%u", ntohl(su->su_sin6.sin6_flowinfo));
  }
#endif

  if (msg_addrinfo(msg)->ai_flags & TP_AI_COMPRESSED)
    comp = ";comp=sigcomp";

  su_inet_ntop(su->su_family, SU_ADDR(su), name, sizeof(name));

  snprintf(stamp, 128,
	   "%s "MOD_ZU" bytes %s %s/[%s]:%u%s%s at %02u:%02u:%02u.%06lu:\n",
	   what, (size_t)n, via, self->tp_name->tpn_proto,
	   name, ntohs(su->su_port), label[0] ? label : "", comp,
	   hour, minute, second, now.tv_usec);
}

/** Dump the data from the iovec */
void tport_dump_iovec(tport_t const *self, msg_t *msg,
		      size_t n, su_iovec_t const iov[], size_t iovused,
		      char const *what, char const *how)
{
  tport_master_t *mr;
  char stamp[128];
  size_t i;

  assert(self); assert(msg);

  mr = self->tp_master;
  if (!mr->mr_dump_file)
    return;

  tport_stamp(self, msg, stamp, what, n, how, su_now());
  fputs(stamp, mr->mr_dump_file);

  for (i = 0; i < iovused && n > 0; i++) {
    size_t len = iov[i].mv_len;
    if (len > n)
      len = n;
    if (fwrite(iov[i].mv_base, len, 1, mr->mr_dump_file) != len)
      break;
    n -= len;
  }

  fputs("\v\n", mr->mr_dump_file);
  fflush(mr->mr_dump_file);
}

/** Log the message. */
void tport_log_msg(tport_t *self, msg_t *msg,
		   char const *what, char const *via,
		   su_time_t now)
{
  char stamp[128];
  msg_iovec_t iov[80];
  size_t i, iovlen = msg_iovec(msg, iov, 80);
  size_t linelen = 0, n, logged = 0, truncated = 0;
  int skip_lf = 0;

#define MSG_SEPARATOR \
  "------------------------------------------------------------------------\n"
#define MAX_LINELEN 2047

  for (i = n = 0; i < iovlen && i < 80; i++)
    n += iov[i].mv_len;

  tport_stamp(self, msg, stamp, what, n, via, now);
  su_log("%s   " MSG_SEPARATOR, stamp);

  for (i = 0; truncated == 0 && i < iovlen && i < 80; i++) {
    char *s = iov[i].mv_base, *end = s + iov[i].mv_len;

    if (skip_lf && s < end && s[0] == '\n') { s++; logged++; skip_lf = 0; }

    while (s < end) {
      if (s[0] == '\0') {
	truncated = logged;
	break;
      }

      n = su_strncspn(s, end - s, "\r\n");

      if (linelen + n > MAX_LINELEN) {
	n = MAX_LINELEN - linelen;
	truncated = logged + n;
      }

      su_log("%s%.*s", linelen > 0 ? "" : "   ", (int)n, s);
      s += n, linelen += n, logged += n;

      if (truncated)
	break;
      if (s == end)
	break;

      linelen = 0;
      su_log("\n");

      /* Skip eol */
      if (s[0] == '\r') {
	s++, logged++;
	if (s == end) {
	  skip_lf = 1;
	  continue;
	}
      }
      if (s[0] == '\n')
	s++, logged++;
    }
  }

  su_log("%s   " MSG_SEPARATOR, linelen > 0 ? "\n" : "");

  if (!truncated && i == 80)
    truncated = logged;

  if (truncated)
    su_log("   *** message truncated at "MOD_ZU" ***\n", truncated);
}
