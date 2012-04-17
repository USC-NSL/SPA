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

#ifndef MSG_INTERNAL_H
/** Defined when <msg_internal.h> has been included. */
#define MSG_INTERNAL_H

/**@IFILE msg_internal.h
 * @brief Abstract messages - internal interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun 29 15:58:06 2000 ppessi
 *
 */

#ifdef MSG_H
#error "msg_internal.h" should be included before "msg.h"
#endif

#include "sofia-sip/msg.h"
#include "sofia-sip/msg_addr.h"
#include "sofia-sip/msg_buffer.h"

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

SOFIA_BEGIN_DECLS

/* ---------------------------------------------------------------------- */
/* Types used when handling streaming */

typedef struct msg_buffer_s msg_buffer_t;

/* ---------------------------------------------------------------------- */

struct msg_s {
  su_home_t           m_home[1]; /**< Memory home */

  msg_mclass_t const *m_class;	/**< Message class */
  int                 m_oflags;	/**< Original flags */

  msg_pub_t          *m_object;	/**< Public view to parsed message */

  size_t              m_maxsize;/**< Maximum size */
  size_t              m_size;	/**< Total size of fragments */

  msg_header_t       *m_chain;	/**< Fragment chain */
  msg_header_t      **m_tail;	/**< Tail of fragment chain */

  msg_payload_t      *m_chunk;	/**< Incomplete payload fragment */

  /* Parsing/printing buffer */
  struct msg_mbuffer_s {
    char     *mb_data;		/**< Pointer to data */
    usize_t   mb_size;		/**< Size of buffer */
    usize_t   mb_used;		/**< Used data */
    usize_t   mb_commit;	/**< Data committed to msg */
    unsigned  mb_eos:1;		/**< End-of-stream flag */
    unsigned :0;
  } m_buffer[1];

  msg_buffer_t  *m_stream;	/**< User-provided buffers */
  size_t         m_ssize;	/**< Stream size */

  unsigned short m_extract_err; /**< Bitmask of erroneous headers */
  /* Internal flags */
  unsigned     	 m_set_buffer:1;/**< Buffer has been set */
  unsigned     	 m_streaming:1; /**< Use streaming with message */
  unsigned       m_prepared:1;	/**< Prepared/not */
  unsigned  :0;

  msg_t        	*m_next;	/**< Next message */

  msg_t        	*m_parent;	/**< Reference to a parent message */
  int          	 m_refs;	/**< Number of references to this message */

  su_addrinfo_t	 m_addrinfo;	/**< Message addressing info (protocol) */
  su_sockaddr_t	 m_addr[1];	/**< Message address */

  int          	 m_errno;	/**< Errno */
};

/** Buffer for message body. */
struct msg_buffer_s {
  char           *b_data;	    /**< Data - may contain NUL */
  size_t          b_size;	    /**< Length of message payload */
  size_t          b_used;	    /**< Used data */
  size_t          b_avail;	    /**< Available data */
  int             b_complete;	    /**< This buffer completes the message */
  msg_buffer_t   *b_next;	    /**< Next buffer */
  msg_payload_t  *b_chunks;	    /**< List of body chunks */
};

/** Maximum size when streaming. */
#define MSG_SSIZE_MAX (USIZE_MAX)

/* ---------------------------------------------------------------------- */
/* Header-kind predicate functions. */
su_inline int msg_is_single(msg_header_t const *h)
{
  return h->sh_class->hc_kind == msg_kind_single;
}

su_inline int msg_is_prepend(msg_header_t const *h)
{
  return h->sh_class->hc_kind == msg_kind_prepend;
}

su_inline int msg_is_append(msg_header_t const *h)
{
  return
    h->sh_class->hc_kind == msg_kind_append ||
    h->sh_class->hc_kind == msg_kind_apndlist;
}

su_inline int msg_is_list(msg_header_t const *h)
{
  return h->sh_class->hc_kind == msg_kind_list;
}

su_inline int msg_is_special(msg_header_t const *h)
{
  return h->sh_class->hc_hash < 0;
}

SOFIA_END_DECLS

#endif /* MSG_INTERNAL_H */
