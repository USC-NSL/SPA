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
#include "shrpx_spdy_session.h"

#include <netinet/tcp.h>
#include <unistd.h>
#include <vector>

#include <openssl/err.h>

#include <event2/bufferevent_ssl.h>

#include "shrpx_upstream.h"
#include "shrpx_downstream.h"
#include "shrpx_config.h"
#include "shrpx_error.h"
#include "shrpx_spdy_downstream_connection.h"
#include "shrpx_client_handler.h"
#include "shrpx_ssl.h"
#include "util.h"

using namespace spdylay;

namespace shrpx {

SpdySession::SpdySession(event_base *evbase, SSL_CTX *ssl_ctx)
  : evbase_(evbase),
    ssl_ctx_(ssl_ctx),
    ssl_(0),
    session_(0),
    bev_(0),
    state_(DISCONNECTED),
    notified_(false),
    wrbev_(0),
    rdbev_(0),
    flow_control_(false)
{}

SpdySession::~SpdySession()
{
  disconnect();
}

int SpdySession::disconnect()
{
  if(ENABLE_LOG) {
    SSLOG(INFO, this) << "Disconnecting";
  }
  spdylay_session_del(session_);
  session_ = 0;

  int fd = -1;
  if(ssl_) {
    fd = SSL_get_fd(ssl_);
    SSL_shutdown(ssl_);
  }
  if(bev_) {
    bufferevent_disable(bev_, EV_READ | EV_WRITE);
    bufferevent_free(bev_);
    bev_ = 0;
  }
  if(ssl_) {
    SSL_free(ssl_);
  }
  ssl_ = 0;

  if(fd != -1) {
    shutdown(fd, SHUT_WR);
    close(fd);
  }

  notified_ = false;
  state_ = DISCONNECTED;

  // Delete all client handler associated to Downstream. When deleting
  // SpdyDownstreamConnection, it calls this object's
  // remove_downstream_connection(). So first dump them in vector and
  // iterate and delete them.
  std::vector<SpdyDownstreamConnection*> vec(dconns_.begin(), dconns_.end());
  for(size_t i = 0; i < vec.size(); ++i) {
    remove_downstream_connection(vec[i]);
    delete vec[i]->get_client_handler();
  }
  dconns_.clear();
  for(std::set<StreamData*>::iterator i = streams_.begin(),
        eoi = streams_.end(); i != eoi; ++i) {
    delete *i;
  }
  streams_.clear();
  return 0;
}

namespace {
void notify_readcb(bufferevent *bev, void *arg)
{
  int rv;
  SpdySession *spdy = reinterpret_cast<SpdySession*>(arg);
  spdy->clear_notify();
  switch(spdy->get_state()) {
  case SpdySession::DISCONNECTED:
    rv = spdy->initiate_connection();
    if(rv != 0) {
      SSLOG(FATAL, spdy) << "Could not initiate notification connection";
      DIE();
    }
    break;
  case SpdySession::CONNECTED:
    rv = spdy->send();
    if(rv != 0) {
      spdy->disconnect();
    }
    break;
  }
}
} // namespace

namespace {
void notify_eventcb(bufferevent *bev, short events, void *arg)
{
  SpdySession *spdy = reinterpret_cast<SpdySession*>(arg);
  // TODO should DIE()?
  if(events & BEV_EVENT_EOF) {
    SSLOG(ERROR, spdy) << "Notification connection lost: EOF";
  }
  if(events & BEV_EVENT_TIMEOUT) {
    SSLOG(ERROR, spdy) << "Notification connection lost: timeout";
  }
  if(events & BEV_EVENT_ERROR) {
    SSLOG(ERROR, spdy) << "Notification connection lost: network error";
  }
}
} // namespace

int SpdySession::init_notification()
{
  int rv;
  int sockpair[2];
  rv = socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair);
  if(rv == -1) {
    SSLOG(FATAL, this) << "socketpair() failed: " << strerror(errno);
    return -1;
  }
  wrbev_ = bufferevent_socket_new(evbase_, sockpair[0],
                                  BEV_OPT_CLOSE_ON_FREE|
                                  BEV_OPT_DEFER_CALLBACKS);
  if(!wrbev_) {
    SSLOG(FATAL, this) << "bufferevent_socket_new() failed";
    for(int i = 0; i < 2; ++i) {
      close(sockpair[i]);
    }
    return -1;
  }
  rdbev_ = bufferevent_socket_new(evbase_, sockpair[1],
                                  BEV_OPT_CLOSE_ON_FREE|
                                  BEV_OPT_DEFER_CALLBACKS);
  if(!rdbev_) {
    SSLOG(FATAL, this) << "bufferevent_socket_new() failed";
    close(sockpair[1]);
    return -1;
  }
  bufferevent_enable(rdbev_, EV_READ);
  bufferevent_setcb(rdbev_, notify_readcb, 0, notify_eventcb, this);
  return 0;
}

namespace {
void readcb(bufferevent *bev, void *ptr)
{
  int rv;
  SpdySession *spdy = reinterpret_cast<SpdySession*>(ptr);
  rv = spdy->on_read();
  if(rv != 0) {
    spdy->disconnect();
  }
}
} // namespace

namespace {
void writecb(bufferevent *bev, void *ptr)
{
  if(evbuffer_get_length(bufferevent_get_output(bev)) > 0) {
    return;
  }
  int rv;
  SpdySession *spdy = reinterpret_cast<SpdySession*>(ptr);
  rv = spdy->on_write();
  if(rv != 0) {
    spdy->disconnect();
  }
}
} // namespace

namespace {
void eventcb(bufferevent *bev, short events, void *ptr)
{
  SpdySession *spdy = reinterpret_cast<SpdySession*>(ptr);
  if(events & BEV_EVENT_CONNECTED) {
    if(ENABLE_LOG) {
      SSLOG(INFO, spdy) << "Connection established";
    }
    spdy->connected();
    if((!get_config()->insecure && spdy->check_cert() != 0) ||
       spdy->on_connect() != 0) {
      spdy->disconnect();
      return;
    }
    int fd = bufferevent_getfd(bev);
    int val = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<char *>(&val), sizeof(val));
  } else if(events & BEV_EVENT_EOF) {
    if(ENABLE_LOG) {
      SSLOG(INFO, spdy) << "EOF";
    }
    spdy->disconnect();
  } else if(events & (BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT)) {
    if(ENABLE_LOG) {
      if(events & BEV_EVENT_ERROR) {
        SSLOG(INFO, spdy) << "Network error";
      } else {
        SSLOG(INFO, spdy) << "Timeout";
      }
    }
    spdy->disconnect();
  }
}
} // namespace

int SpdySession::check_cert()
{
  return ssl::check_cert(ssl_);
}

int SpdySession::initiate_connection()
{
  int rv;
  assert(state_ == DISCONNECTED);
  if(ENABLE_LOG) {
    SSLOG(INFO, this) << "Connecting to downstream server";
  }

  ssl_ = SSL_new(ssl_ctx_);
  if(!ssl_) {
    SSLOG(ERROR, this) << "SSL_new() failed: "
                       << ERR_error_string(ERR_get_error(), NULL);
    return -1;
  }

  if(!ssl::numeric_host(get_config()->downstream_host)) {
    // TLS extensions: SNI. There is no documentation about the return
    // code for this function (actually this is macro wrapping SSL_ctrl
    // at the time of this writing).
    SSL_set_tlsext_host_name(ssl_, get_config()->downstream_host);
  }

  bev_ = bufferevent_openssl_socket_new(evbase_, -1, ssl_,
                                        BUFFEREVENT_SSL_CONNECTING,
                                        BEV_OPT_DEFER_CALLBACKS);
  rv = bufferevent_socket_connect
    (bev_,
     // TODO maybe not thread-safe?
     const_cast<sockaddr*>(&get_config()->downstream_addr.sa),
     get_config()->downstream_addrlen);
  if(rv != 0) {
    bufferevent_free(bev_);
    bev_ = 0;
    return SHRPX_ERR_NETWORK;
  }

  bufferevent_setwatermark(bev_, EV_READ, 0, SHRPX_READ_WARTER_MARK);
  bufferevent_enable(bev_, EV_READ);
  bufferevent_setcb(bev_, readcb, writecb, eventcb, this);
  // No timeout for SPDY session

  state_ = CONNECTING;
  return 0;
}

void SpdySession::connected()
{
  state_ = CONNECTED;
}

void SpdySession::add_downstream_connection(SpdyDownstreamConnection *dconn)
{
  dconns_.insert(dconn);
}

void SpdySession::remove_downstream_connection(SpdyDownstreamConnection *dconn)
{
  dconns_.erase(dconn);
  dconn->detach_stream_data();
}

void SpdySession::remove_stream_data(StreamData *sd)
{
  streams_.erase(sd);
  if(sd->dconn) {
    sd->dconn->detach_stream_data();
  }
  delete sd;
}

int SpdySession::submit_request(SpdyDownstreamConnection *dconn,
                                uint8_t pri, const char **nv,
                                const spdylay_data_provider *data_prd)
{
  assert(state_ == CONNECTED);
  StreamData *sd = new StreamData();
  int rv = spdylay_submit_request(session_, pri, nv, data_prd, sd);
  if(rv == 0) {
    dconn->attach_stream_data(sd);
    streams_.insert(sd);
  } else {
    SSLOG(FATAL, this) << "spdylay_submit_request() failed: "
                       << spdylay_strerror(rv);
    delete sd;
    return -1;
  }
  return 0;
}

int SpdySession::submit_rst_stream(SpdyDownstreamConnection *dconn,
                                   int32_t stream_id, uint32_t status_code)
{
  assert(state_ == CONNECTED);
  int rv = spdylay_submit_rst_stream(session_, stream_id, status_code);
  if(rv != 0) {
    SSLOG(FATAL, this) << "spdylay_submit_rst_stream() failed: "
                       << spdylay_strerror(rv);
    return -1;
  }
  return 0;
}

int SpdySession::submit_window_update(SpdyDownstreamConnection *dconn,
                                      int32_t amount)
{
  assert(state_ == CONNECTED);
  int rv;
  int32_t stream_id;
  stream_id = dconn->get_downstream()->get_downstream_stream_id();
  rv = spdylay_submit_window_update(session_, stream_id, amount);
  if(rv < SPDYLAY_ERR_FATAL) {
    SSLOG(FATAL, this) << "spdylay_submit_window_update() failed: "
                       << spdylay_strerror(rv);
    return -1;
  }
  return 0;
}

int32_t SpdySession::get_initial_window_size() const
{
  return 1 << get_config()->spdy_downstream_window_bits;
}

bool SpdySession::get_flow_control() const
{
  return flow_control_;
}

int SpdySession::resume_data(SpdyDownstreamConnection *dconn)
{
  assert(state_ == CONNECTED);
  Downstream *downstream = dconn->get_downstream();
  int rv = spdylay_session_resume_data(session_,
                                       downstream->get_downstream_stream_id());
  switch(rv) {
  case 0:
  case SPDYLAY_ERR_INVALID_ARGUMENT:
    return 0;
  default:
    SSLOG(FATAL, this) << "spdylay_resume_session() failed: "
                       << spdylay_strerror(rv);
    return -1;
  }
}

namespace {
void call_downstream_readcb(SpdySession *spdy, Downstream *downstream)
{
  Upstream *upstream = downstream->get_upstream();
  if(upstream) {
    (upstream->get_downstream_readcb())
      (spdy->get_bev(),
       downstream->get_downstream_connection());
  }
}
} // namespace

namespace {
ssize_t send_callback(spdylay_session *session,
                      const uint8_t *data, size_t len, int flags,
                      void *user_data)
{
  int rv;
  SpdySession *spdy = reinterpret_cast<SpdySession*>(user_data);

  bufferevent *bev = spdy->get_bev();
  evbuffer *output = bufferevent_get_output(bev);
  // Check buffer length and return WOULDBLOCK if it is large enough.
  if(evbuffer_get_length(output) > Downstream::OUTPUT_UPPER_THRES) {
    return SPDYLAY_ERR_WOULDBLOCK;
  }

  rv = evbuffer_add(output, data, len);
  if(rv == -1) {
    SSLOG(FATAL, spdy) << "evbuffer_add() failed";
    return SPDYLAY_ERR_CALLBACK_FAILURE;
  } else {
    return len;
  }
}
} // namespace

namespace {
ssize_t recv_callback(spdylay_session *session,
                      uint8_t *data, size_t len, int flags, void *user_data)
{
  SpdySession *spdy = reinterpret_cast<SpdySession*>(user_data);

  bufferevent *bev = spdy->get_bev();
  evbuffer *input = bufferevent_get_input(bev);
  int nread = evbuffer_remove(input, data, len);
  if(nread == -1) {
    return SPDYLAY_ERR_CALLBACK_FAILURE;
  } else if(nread == 0) {
    return SPDYLAY_ERR_WOULDBLOCK;
  } else {
    return nread;
  }
}
} // namespace

namespace {
void on_stream_close_callback
(spdylay_session *session, int32_t stream_id, spdylay_status_code status_code,
 void *user_data)
{
  int rv;
  SpdySession *spdy = reinterpret_cast<SpdySession*>(user_data);
  if(ENABLE_LOG) {
    SSLOG(INFO, spdy) << "Stream stream_id=" << stream_id
                      << " is being closed";
  }
  StreamData *sd;
  sd = reinterpret_cast<StreamData*>
    (spdylay_session_get_stream_user_data(session, stream_id));
  if(sd == 0) {
    // We might get this close callback when pushed streams are
    // closed.
    return;
  }
  SpdyDownstreamConnection* dconn = sd->dconn;
  if(dconn) {
    Downstream *downstream = dconn->get_downstream();
    if(downstream && downstream->get_downstream_stream_id() == stream_id) {
      Upstream *upstream = downstream->get_upstream();
      if(status_code == SPDYLAY_OK) {
        downstream->set_response_state(Downstream::MSG_COMPLETE);
        rv = upstream->on_downstream_body_complete(downstream);
        if(rv != 0) {
          downstream->set_response_state(Downstream::MSG_RESET);
        }
      } else {
        downstream->set_response_state(Downstream::MSG_RESET);
      }
      call_downstream_readcb(spdy, downstream);
      // dconn may be deleted
    }
  }
  // The life time of StreamData ends here
  spdy->remove_stream_data(sd);
}
} // namespace

namespace {
void on_ctrl_recv_callback
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 void *user_data)
{
  int rv;
  SpdySession *spdy = reinterpret_cast<SpdySession*>(user_data);
  StreamData *sd;
  Downstream *downstream;
  switch(type) {
  case SPDYLAY_SYN_STREAM:
    if(ENABLE_LOG) {
      SSLOG(INFO, spdy) << "Received upstream SYN_STREAM stream_id="
                        << frame->syn_stream.stream_id;
    }
    // We just respond pushed stream with RST_STREAM.
    spdylay_submit_rst_stream(session, frame->syn_stream.stream_id,
                              SPDYLAY_REFUSED_STREAM);
    break;
  case SPDYLAY_RST_STREAM:
    sd = reinterpret_cast<StreamData*>
      (spdylay_session_get_stream_user_data(session,
                                            frame->rst_stream.stream_id));
    if(sd && sd->dconn) {
      downstream = sd->dconn->get_downstream();
      if(downstream &&
         downstream->get_downstream_stream_id() ==
         frame->rst_stream.stream_id) {
        // If we got RST_STREAM, just flag MSG_RESET to indicate
        // upstream connection must be terminated.
        downstream->set_response_state(Downstream::MSG_RESET);
        call_downstream_readcb(spdy, downstream);
      }
    }
    break;
  case SPDYLAY_SYN_REPLY: {
    sd = reinterpret_cast<StreamData*>
      (spdylay_session_get_stream_user_data(session,
                                            frame->syn_reply.stream_id));
    if(!sd || !sd->dconn) {
      spdylay_submit_rst_stream(session, frame->syn_reply.stream_id,
                                SPDYLAY_INTERNAL_ERROR);
      break;
    }
    downstream = sd->dconn->get_downstream();
    if(!downstream ||
       downstream->get_downstream_stream_id() != frame->syn_reply.stream_id) {
      spdylay_submit_rst_stream(session, frame->syn_reply.stream_id,
                                SPDYLAY_INTERNAL_ERROR);
      break;
    }
    char **nv = frame->syn_reply.nv;
    const char *status = 0;
    const char *version = 0;
    const char *content_length = 0;
    for(size_t i = 0; nv[i]; i += 2) {
      if(strcmp(nv[i], ":status") == 0) {
        unsigned int code = strtoul(nv[i+1], 0, 10);
        downstream->set_response_http_status(code);
        status = nv[i+1];
      } else if(strcmp(nv[i], ":version") == 0) {
        // We assume for now that most version is HTTP/1.1 from
        // SPDY. So just check if it is HTTP/1.0 and then set response
        // minor as so.
        downstream->set_response_major(1);
        if(util::strieq(nv[i+1], "HTTP/1.0")) {
          downstream->set_response_minor(0);
        } else {
          downstream->set_response_minor(1);
        }
        version = nv[i+1];
      } else if(nv[i][0] != ':') {
        if(strcmp(nv[i], "content-length") == 0) {
          content_length = nv[i+1];
        }
        downstream->add_response_header(nv[i], nv[i+1]);
      }
    }
    if(!status || !version) {
      spdylay_submit_rst_stream(session, frame->syn_reply.stream_id,
                                SPDYLAY_PROTOCOL_ERROR);
      downstream->set_response_state(Downstream::MSG_RESET);
      call_downstream_readcb(spdy, downstream);
      return;
    }

    if(!content_length && downstream->get_request_method() != "HEAD" &&
       downstream->get_request_method() != "CONNECT") {
      unsigned int status;
      status = downstream->get_response_http_status();
      if(!((100 <= status && status <= 199) || status == 204 ||
           status == 304)) {
        // In SPDY, we are supporsed not to receive
        // transfer-encoding.
        downstream->add_response_header("transfer-encoding", "chunked");
      }
    }

    if(ENABLE_LOG) {
      std::stringstream ss;
      for(size_t i = 0; nv[i]; i += 2) {
        ss << TTY_HTTP_HD << nv[i] << TTY_RST << ": " << nv[i+1] << "\n";
      }
      SSLOG(INFO, spdy) << "HTTP response headers. stream_id="
                        << frame->syn_reply.stream_id
                        << "\n" << ss.str();
    }

    Upstream *upstream = downstream->get_upstream();
    downstream->set_response_state(Downstream::HEADER_COMPLETE);
    rv = upstream->on_downstream_header_complete(downstream);
    if(rv != 0) {
      spdylay_submit_rst_stream(session, frame->syn_reply.stream_id,
                                SPDYLAY_PROTOCOL_ERROR);
      downstream->set_response_state(Downstream::MSG_RESET);
    }
    call_downstream_readcb(spdy, downstream);
    break;
  }
  default:
    break;
  }
}
} // namespace

namespace {
void on_data_chunk_recv_callback(spdylay_session *session,
                                 uint8_t flags, int32_t stream_id,
                                 const uint8_t *data, size_t len,
                                 void *user_data)
{
  int rv;
  SpdySession *spdy = reinterpret_cast<SpdySession*>(user_data);
  StreamData *sd;
  sd = reinterpret_cast<StreamData*>
    (spdylay_session_get_stream_user_data(session, stream_id));
  if(!sd || !sd->dconn) {
    spdylay_submit_rst_stream(session, stream_id, SPDYLAY_INTERNAL_ERROR);
    return;
  }
  Downstream *downstream = sd->dconn->get_downstream();
  if(!downstream || downstream->get_downstream_stream_id() != stream_id) {
    spdylay_submit_rst_stream(session, stream_id, SPDYLAY_INTERNAL_ERROR);
    return;
  }

  if(spdy->get_flow_control()) {
    sd->dconn->inc_recv_window_size(len);
    if(sd->dconn->get_recv_window_size() > spdy->get_initial_window_size()) {
      if(ENABLE_LOG) {
        SSLOG(INFO, spdy) << "Flow control error: recv_window_size="
                          << sd->dconn->get_recv_window_size()
                          << ", initial_window_size="
                          << spdy->get_initial_window_size();
      }
      spdylay_submit_rst_stream(session, stream_id,
                                SPDYLAY_FLOW_CONTROL_ERROR);
      downstream->set_response_state(Downstream::MSG_RESET);
      call_downstream_readcb(spdy, downstream);
      return;
    }
  }

  Upstream *upstream = downstream->get_upstream();
  rv = upstream->on_downstream_body(downstream, data, len);
  if(rv != 0) {
    spdylay_submit_rst_stream(session, stream_id, SPDYLAY_INTERNAL_ERROR);
    downstream->set_response_state(Downstream::MSG_RESET);
  }
  call_downstream_readcb(spdy, downstream);
}
} // namespace

namespace {
void before_ctrl_send_callback(spdylay_session *session,
                               spdylay_frame_type type,
                               spdylay_frame *frame,
                               void *user_data)
{
  if(type == SPDYLAY_SYN_STREAM) {
    StreamData *sd;
    sd = reinterpret_cast<StreamData*>
      (spdylay_session_get_stream_user_data(session,
                                            frame->syn_stream.stream_id));
    if(!sd || !sd->dconn) {
      spdylay_submit_rst_stream(session, frame->syn_stream.stream_id,
                                SPDYLAY_CANCEL);
      return;
    }
    Downstream *downstream = sd->dconn->get_downstream();
    if(downstream) {
      downstream->set_downstream_stream_id(frame->syn_stream.stream_id);
    } else {
      spdylay_submit_rst_stream(session, frame->syn_stream.stream_id,
                                SPDYLAY_CANCEL);
    }
  }
}
} // namespace

namespace {
void on_ctrl_not_send_callback(spdylay_session *session,
                               spdylay_frame_type type,
                               spdylay_frame *frame,
                               int error_code, void *user_data)
{
  SpdySession *spdy = reinterpret_cast<SpdySession*>(user_data);
  SSLOG(WARNING, spdy) << "Failed to send control frame type=" << type << ", "
                       << "error_code=" << error_code << ":"
                       << spdylay_strerror(error_code);
  if(type == SPDYLAY_SYN_STREAM) {
    // To avoid stream hanging around, flag Downstream::MSG_RESET and
    // terminate the upstream and downstream connections.
    StreamData *sd;
    sd = reinterpret_cast<StreamData*>
      (spdylay_session_get_stream_user_data(session,
                                            frame->syn_stream.stream_id));
    if(!sd) {
      return;
    }
    if(sd->dconn) {
      Downstream *downstream = sd->dconn->get_downstream();
      if(!downstream ||
         downstream->get_downstream_stream_id() !=
         frame->syn_stream.stream_id) {
        return;
      }
      downstream->set_response_state(Downstream::MSG_RESET);
      call_downstream_readcb(spdy, downstream);
    }
    spdy->remove_stream_data(sd);
  }
}
} // namespace

namespace {
void on_ctrl_recv_parse_error_callback(spdylay_session *session,
                                       spdylay_frame_type type,
                                       const uint8_t *head, size_t headlen,
                                       const uint8_t *payload,
                                       size_t payloadlen, int error_code,
                                       void *user_data)
{
  SpdySession *spdy = reinterpret_cast<SpdySession*>(user_data);
  if(ENABLE_LOG) {
    SSLOG(INFO, spdy) << "Failed to parse received control frame. type="
                      << type
                      << ", error_code=" << error_code << ":"
                      << spdylay_strerror(error_code);
  }
}
} // namespace

namespace {
void on_unknown_ctrl_recv_callback(spdylay_session *session,
                                   const uint8_t *head, size_t headlen,
                                   const uint8_t *payload, size_t payloadlen,
                                   void *user_data)
{
  SpdySession *spdy = reinterpret_cast<SpdySession*>(user_data);
  if(ENABLE_LOG) {
    SSLOG(INFO, spdy) << "Received unknown control frame";
  }
}
} // namespace

int SpdySession::on_connect()
{
  int rv;
  const unsigned char *next_proto = 0;
  unsigned int next_proto_len;
  SSL_get0_next_proto_negotiated(ssl_, &next_proto, &next_proto_len);

  if(ENABLE_LOG) {
    std::string proto(next_proto, next_proto+next_proto_len);
    SSLOG(INFO, this) << "Negotiated next protocol: " << proto;
  }
  uint16_t version = spdylay_npn_get_version(next_proto, next_proto_len);
  if(!version) {
    return -1;
  }
  spdylay_session_callbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.send_callback = send_callback;
  callbacks.recv_callback = recv_callback;
  callbacks.on_stream_close_callback = on_stream_close_callback;
  callbacks.on_ctrl_recv_callback = on_ctrl_recv_callback;
  callbacks.on_data_chunk_recv_callback = on_data_chunk_recv_callback;
  callbacks.before_ctrl_send_callback = before_ctrl_send_callback;
  callbacks.on_ctrl_not_send_callback = on_ctrl_not_send_callback;
  callbacks.on_ctrl_recv_parse_error_callback =
    on_ctrl_recv_parse_error_callback;
  callbacks.on_unknown_ctrl_recv_callback = on_unknown_ctrl_recv_callback;

  rv = spdylay_session_client_new(&session_, version, &callbacks, this);
  if(rv != 0) {
    return -1;
  }

  if(version == SPDYLAY_PROTO_SPDY3) {
    int val = 1;
    flow_control_ = true;
    rv = spdylay_session_set_option(session_,
                                    SPDYLAY_OPT_NO_AUTO_WINDOW_UPDATE, &val,
                                    sizeof(val));
    assert(rv == 0);
  } else {
    flow_control_ = false;
  }

  spdylay_settings_entry entry[2];
  entry[0].settings_id = SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS;
  entry[0].value = get_config()->spdy_max_concurrent_streams;
  entry[0].flags = SPDYLAY_ID_FLAG_SETTINGS_NONE;

  entry[1].settings_id = SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE;
  entry[1].value = get_initial_window_size();
  entry[1].flags = SPDYLAY_ID_FLAG_SETTINGS_NONE;

  rv = spdylay_submit_settings
    (session_, SPDYLAY_FLAG_SETTINGS_NONE,
     entry, sizeof(entry)/sizeof(spdylay_settings_entry));
  if(rv != 0) {
    return -1;
  }
  rv = send();
  if(rv != 0) {
    return -1;
  }

  // submit pending request
  for(std::set<SpdyDownstreamConnection*>::iterator i = dconns_.begin(),
        eoi = dconns_.end(); i != eoi; ++i) {
    if((*i)->push_request_headers() != 0) {
      return -1;
    }
  }
  return 0;
}

int SpdySession::on_read()
{
  int rv = 0;
  if((rv = spdylay_session_recv(session_)) < 0) {
    if(rv != SPDYLAY_ERR_EOF) {
      SSLOG(ERROR, this) << "spdylay_session_recv() returned error: "
                         << spdylay_strerror(rv);
    }
  } else if((rv = spdylay_session_send(session_)) < 0) {
    SSLOG(ERROR, this) << "spdylay_session_send() returned error: "
                       << spdylay_strerror(rv);
  }
  if(rv == 0) {
    if(spdylay_session_want_read(session_) == 0 &&
       spdylay_session_want_write(session_) == 0) {
      if(ENABLE_LOG) {
        SSLOG(INFO, this) << "No more read/write for this session";
      }
      rv = -1;
    }
  }
  return rv;
}

int SpdySession::on_write()
{
  return send();
}

int SpdySession::send()
{
  int rv = 0;
  if((rv = spdylay_session_send(session_)) < 0) {
    SSLOG(ERROR, this) << "spdylay_session_send() returned error: "
                       << spdylay_strerror(rv);
  }
  if(rv == 0) {
    if(spdylay_session_want_read(session_) == 0 &&
       spdylay_session_want_write(session_) == 0) {
      if(ENABLE_LOG) {
        SSLOG(INFO, this) << "No more read/write for this session";
      }
      rv = -1;
    }
  }
  return rv;
}

void SpdySession::clear_notify()
{
  evbuffer *input = bufferevent_get_output(rdbev_);
  evbuffer_drain(input, evbuffer_get_length(input));
  notified_ = false;
}

void SpdySession::notify()
{
  if(!notified_) {
    bufferevent_write(wrbev_, "1", 1);
    notified_ = true;
  }
}

bufferevent* SpdySession::get_bev() const
{
  return bev_;
}

int SpdySession::get_state() const
{
  return state_;
}

} // namespace shrpx
