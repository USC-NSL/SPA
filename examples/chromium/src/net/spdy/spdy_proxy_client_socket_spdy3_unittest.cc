// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_proxy_client_socket.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "net/base/address_list.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/spdy/spdy_test_util_spdy3.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using namespace net::test_spdy3;

//-----------------------------------------------------------------------------

namespace {

static const char kRequestUrl[] = "https://www.google.com/";
static const char kOriginHost[] = "www.google.com";
static const int kOriginPort = 443;
static const char kOriginHostPort[] = "www.google.com:443";
static const char kProxyUrl[] = "https://myproxy:6121/";
static const char kProxyHost[] = "myproxy";
static const int kProxyPort = 6121;
static const char kUserAgent[] = "Mozilla/1.0";

static const int kStreamId = 1;

static const char kMsg1[] = "\0hello!\xff";
static const int kLen1 = 8;
static const char kMsg2[] = "\00012345678\0";
static const int kLen2 = 10;
static const char kMsg3[] = "bye!";
static const int kLen3 = 4;
static const char kMsg33[] = "bye!bye!";
static const int kLen33 = kLen3 + kLen3;
static const char kMsg333[] = "bye!bye!bye!";
static const int kLen333 = kLen3 + kLen3 + kLen3;

static const char kRedirectUrl[] = "https://example.com/";

}  // anonymous namespace

namespace net {

class SpdyProxyClientSocketSpdy3Test : public PlatformTest {
 public:
  SpdyProxyClientSocketSpdy3Test();

  virtual void TearDown();

 protected:
  void Initialize(MockRead* reads, size_t reads_count, MockWrite* writes,
                  size_t writes_count);
  SpdyFrame* ConstructConnectRequestFrame();
  SpdyFrame* ConstructConnectAuthRequestFrame();
  SpdyFrame* ConstructConnectReplyFrame();
  SpdyFrame* ConstructConnectAuthReplyFrame();
  SpdyFrame* ConstructConnectRedirectReplyFrame();
  SpdyFrame* ConstructConnectErrorReplyFrame();
  SpdyFrame* ConstructBodyFrame(const char* data, int length);
  scoped_refptr<IOBufferWithSize> CreateBuffer(const char* data, int size);
  void AssertConnectSucceeds();
  void AssertConnectFails(int result);
  void AssertConnectionEstablished();
  void AssertSyncReadEquals(const char* data, int len);
  void AssertAsyncReadEquals(const char* data, int len);
  void AssertReadStarts(const char* data, int len);
  void AssertReadReturns(const char* data, int len);
  void AssertAsyncWriteSucceeds(const char* data, int len);
  void AssertWriteReturns(const char* data, int len, int rv);
  void AssertWriteLength(int len);
  void AssertAsyncWriteWithReadsSucceeds(const char* data, int len,
                                        int num_reads);

  void AddAuthToCache() {
    const base::string16 kFoo(ASCIIToUTF16("foo"));
    const base::string16 kBar(ASCIIToUTF16("bar"));
    session_->http_auth_cache()->Add(GURL(kProxyUrl),
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  void Run(int steps) {
    data_->StopAfter(steps);
    data_->Run();
  }

  scoped_ptr<SpdyProxyClientSocket> sock_;
  TestCompletionCallback read_callback_;
  TestCompletionCallback write_callback_;
  scoped_ptr<DeterministicSocketData> data_;
  CapturingBoundNetLog net_log_;

 private:
  scoped_refptr<HttpNetworkSession> session_;
  scoped_refptr<IOBuffer> read_buf_;
  SpdySessionDependencies session_deps_;
  MockConnect connect_data_;
  scoped_refptr<SpdySession> spdy_session_;
  scoped_refptr<SpdyStream> spdy_stream_;
  BufferedSpdyFramer framer_;

  std::string user_agent_;
  GURL url_;
  HostPortPair proxy_host_port_;
  HostPortPair endpoint_host_port_pair_;
  ProxyServer proxy_;
  HostPortProxyPair endpoint_host_port_proxy_pair_;
  scoped_refptr<TransportSocketParams> transport_params_;

  DISALLOW_COPY_AND_ASSIGN(SpdyProxyClientSocketSpdy3Test);
};

SpdyProxyClientSocketSpdy3Test::SpdyProxyClientSocketSpdy3Test()
    : sock_(NULL),
      data_(NULL),
      session_(NULL),
      read_buf_(NULL),
      session_deps_(),
      connect_data_(SYNCHRONOUS, OK),
      spdy_session_(NULL),
      spdy_stream_(NULL),
      framer_(3, false),
      user_agent_(kUserAgent),
      url_(kRequestUrl),
      proxy_host_port_(kProxyHost, kProxyPort),
      endpoint_host_port_pair_(kOriginHost, kOriginPort),
      proxy_(ProxyServer::SCHEME_HTTPS, proxy_host_port_),
      endpoint_host_port_proxy_pair_(endpoint_host_port_pair_, proxy_),
      transport_params_(new TransportSocketParams(proxy_host_port_,
                                                  LOWEST,
                                                  false,
                                                  false,
                                                  OnHostResolutionCallback())) {
  session_deps_.net_log = net_log_.bound().net_log();
}

void SpdyProxyClientSocketSpdy3Test::TearDown() {
  sock_.reset(NULL);
  if (session_ != NULL)
    session_->spdy_session_pool()->CloseAllSessions();

  // Empty the current queue.
  MessageLoop::current()->RunUntilIdle();
  PlatformTest::TearDown();
}

void SpdyProxyClientSocketSpdy3Test::Initialize(MockRead* reads,
                                                size_t reads_count,
                                                MockWrite* writes,
                                                size_t writes_count) {
  data_.reset(new DeterministicSocketData(reads, reads_count,
                                          writes, writes_count));
  data_->set_connect_data(connect_data_);
  data_->SetStop(2);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(
      data_.get());
  session_deps_.host_resolver->set_synchronous_mode(true);

  session_ = SpdySessionDependencies::SpdyCreateSessionDeterministic(
      &session_deps_);

  // Creates a new spdy session.
  spdy_session_ =
      session_->spdy_session_pool()->Get(endpoint_host_port_proxy_pair_,
                                         net_log_.bound());

  // Perform the TCP connect.
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK,
            connection->Init(endpoint_host_port_pair_.ToString(),
                             transport_params_, LOWEST, CompletionCallback(),
                             session_->GetTransportSocketPool(
                                 HttpNetworkSession::NORMAL_SOCKET_POOL),
                             net_log_.bound()));
  spdy_session_->InitializeWithSocket(connection.release(), false, OK);

  // Create the SPDY Stream.
  spdy_stream_ =
      CreateStreamSynchronously(spdy_session_, url_, LOWEST, net_log_.bound());
  ASSERT_TRUE(spdy_stream_.get() != NULL);

  // Create the SpdyProxyClientSocket.
  sock_.reset(
      new SpdyProxyClientSocket(spdy_stream_, user_agent_,
                                endpoint_host_port_pair_, url_,
                                proxy_host_port_,  net_log_.bound(),
                                session_->http_auth_cache(),
                                session_->http_auth_handler_factory()));
}

scoped_refptr<IOBufferWithSize> SpdyProxyClientSocketSpdy3Test::CreateBuffer(
    const char* data, int size) {
  scoped_refptr<IOBufferWithSize> buf(new IOBufferWithSize(size));
  memcpy(buf->data(), data, size);
  return buf;
}

void SpdyProxyClientSocketSpdy3Test::AssertConnectSucceeds() {
  ASSERT_EQ(ERR_IO_PENDING, sock_->Connect(read_callback_.callback()));
  data_->Run();
  ASSERT_EQ(OK, read_callback_.WaitForResult());
}

void SpdyProxyClientSocketSpdy3Test::AssertConnectFails(int result) {
  ASSERT_EQ(ERR_IO_PENDING, sock_->Connect(read_callback_.callback()));
  data_->Run();
  ASSERT_EQ(result, read_callback_.WaitForResult());
}

void SpdyProxyClientSocketSpdy3Test::AssertConnectionEstablished() {
  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_EQ(200, response->headers->response_code());
  ASSERT_EQ("Connection Established", response->headers->GetStatusText());
}

void SpdyProxyClientSocketSpdy3Test::AssertSyncReadEquals(const char* data,
                                                          int len) {
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  ASSERT_EQ(len, sock_->Read(buf, len, CompletionCallback()));
  ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
  ASSERT_TRUE(sock_->IsConnected());
}

void SpdyProxyClientSocketSpdy3Test::AssertAsyncReadEquals(const char* data,
                                                           int len) {
  data_->StopAfter(1);
  // Issue the read, which will be completed asynchronously
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  ASSERT_EQ(ERR_IO_PENDING, sock_->Read(buf, len, read_callback_.callback()));
  EXPECT_TRUE(sock_->IsConnected());
  data_->Run();

  EXPECT_TRUE(sock_->IsConnected());

  // Now the read will return
  EXPECT_EQ(len, read_callback_.WaitForResult());
  ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
}

void SpdyProxyClientSocketSpdy3Test::AssertReadStarts(const char* data,
                                                      int len) {
  data_->StopAfter(1);
  // Issue the read, which will be completed asynchronously
  read_buf_ = new IOBuffer(len);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf_, len, read_callback_.callback()));
  EXPECT_TRUE(sock_->IsConnected());
}

void SpdyProxyClientSocketSpdy3Test::AssertReadReturns(const char* data,
                                                       int len) {
  EXPECT_TRUE(sock_->IsConnected());

  // Now the read will return
  EXPECT_EQ(len, read_callback_.WaitForResult());
  ASSERT_EQ(std::string(data, len), std::string(read_buf_->data(), len));
}

void SpdyProxyClientSocketSpdy3Test::AssertAsyncWriteSucceeds(const char* data,
                                                              int len) {
  AssertWriteReturns(data, len, ERR_IO_PENDING);
  data_->RunFor(1);
  AssertWriteLength(len);
}

void SpdyProxyClientSocketSpdy3Test::AssertWriteReturns(const char* data,
                                                        int len,
                                                        int rv) {
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(data, len));
  EXPECT_EQ(rv, sock_->Write(buf, buf->size(), write_callback_.callback()));
}

void SpdyProxyClientSocketSpdy3Test::AssertWriteLength(int len) {
  EXPECT_EQ(len, write_callback_.WaitForResult());
}

void SpdyProxyClientSocketSpdy3Test::AssertAsyncWriteWithReadsSucceeds(
    const char* data, int len, int num_reads) {
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(data, len));

  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(buf, buf->size(),
                                         write_callback_.callback()));

  for (int i = 0; i < num_reads; i++) {
    Run(1);
    AssertSyncReadEquals(kMsg2, kLen2);
  }

  write_callback_.WaitForResult();
}

// Constructs a standard SPDY SYN_STREAM frame for a CONNECT request.
SpdyFrame*
SpdyProxyClientSocketSpdy3Test::ConstructConnectRequestFrame() {
  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,
    kStreamId,
    0,
    net::ConvertRequestPriorityToSpdyPriority(LOWEST, 3),
    0,
    CONTROL_FLAG_NONE,
    false,
    RST_STREAM_INVALID,
    NULL,
    0,
    DATA_FLAG_NONE
  };
  const char* const kConnectHeaders[] = {
    ":method", "CONNECT",
    ":path", kOriginHostPort,
    ":host", kOriginHost,
    "user-agent", kUserAgent,
    ":version", "HTTP/1.1",
  };
  return ConstructSpdyFrame(
      kSynStartHeader, NULL, 0, kConnectHeaders, arraysize(kConnectHeaders)/2);
}

// Constructs a SPDY SYN_STREAM frame for a CONNECT request which includes
// Proxy-Authorization headers.
SpdyFrame*
SpdyProxyClientSocketSpdy3Test::ConstructConnectAuthRequestFrame() {
  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,
    kStreamId,
    0,
    net::ConvertRequestPriorityToSpdyPriority(LOWEST, 3),
    0,
    CONTROL_FLAG_NONE,
    false,
    RST_STREAM_INVALID,
    NULL,
    0,
    DATA_FLAG_NONE
  };
  const char* const kConnectHeaders[] = {
    ":method", "CONNECT",
    ":path", kOriginHostPort,
    ":host", kOriginHost,
    "user-agent", kUserAgent,
    ":version", "HTTP/1.1",
    "proxy-authorization", "Basic Zm9vOmJhcg==",
  };
  return ConstructSpdyFrame(
      kSynStartHeader, NULL, 0, kConnectHeaders, arraysize(kConnectHeaders)/2);
}

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY CONNECT.
SpdyFrame* SpdyProxyClientSocketSpdy3Test::ConstructConnectReplyFrame() {
  const char* const kStandardReplyHeaders[] = {
      ":status", "200 Connection Established",
      ":version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   kStreamId,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardReplyHeaders,
                                   arraysize(kStandardReplyHeaders));
}

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY CONNECT.
SpdyFrame*
SpdyProxyClientSocketSpdy3Test::ConstructConnectAuthReplyFrame() {
  const char* const kStandardReplyHeaders[] = {
      ":status", "407 Proxy Authentication Required",
      ":version", "HTTP/1.1",
      "proxy-authenticate", "Basic realm=\"MyRealm1\"",
  };

  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   kStreamId,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardReplyHeaders,
                                   arraysize(kStandardReplyHeaders));
}

// Constructs a SPDY SYN_REPLY frame with an HTTP 302 redirect.
SpdyFrame*
SpdyProxyClientSocketSpdy3Test::ConstructConnectRedirectReplyFrame() {
  const char* const kStandardReplyHeaders[] = {
      ":status", "302 Found",
      ":version", "HTTP/1.1",
      "location", kRedirectUrl,
      "set-cookie", "foo=bar"
  };

  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   kStreamId,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardReplyHeaders,
                                   arraysize(kStandardReplyHeaders));
}

// Constructs a SPDY SYN_REPLY frame with an HTTP 500 error.
SpdyFrame*
SpdyProxyClientSocketSpdy3Test::ConstructConnectErrorReplyFrame() {
  const char* const kStandardReplyHeaders[] = {
      ":status", "500 Internal Server Error",
      ":version", "HTTP/1.1",
  };

  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   kStreamId,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardReplyHeaders,
                                   arraysize(kStandardReplyHeaders));
}

SpdyFrame* SpdyProxyClientSocketSpdy3Test::ConstructBodyFrame(
    const char* data,
    int length) {
  return framer_.CreateDataFrame(kStreamId, data, length, DATA_FLAG_NONE);
}

// ----------- Connect

TEST_F(SpdyProxyClientSocketSpdy3Test, ConnectSendsCorrectRequest) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  AssertConnectionEstablished();
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ConnectWithAuthRequested) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectAuthReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_EQ(407, response->headers->response_code());
  ASSERT_EQ("Proxy Authentication Required",
            response->headers->GetStatusText());
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ConnectWithAuthCredentials) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectAuthRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));
  AddAuthToCache();

  AssertConnectSucceeds();

  AssertConnectionEstablished();
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ConnectRedirects) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectRedirectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_HTTPS_PROXY_TUNNEL_RESPONSE);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);

  const HttpResponseHeaders* headers = response->headers;
  ASSERT_EQ(302, headers->response_code());
  ASSERT_FALSE(headers->HasHeader("set-cookie"));
  ASSERT_TRUE(headers->HasHeaderValue("content-length", "0"));

  std::string location;
  ASSERT_TRUE(headers->IsRedirect(&location));
  ASSERT_EQ(location, kRedirectUrl);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ConnectFails) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    MockRead(ASYNC, 0, 1),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_CONNECTION_CLOSED);

  ASSERT_FALSE(sock_->IsConnected());
}

// ----------- WasEverUsed

TEST_F(SpdyProxyClientSocketSpdy3Test, WasEverUsedReturnsCorrectValues) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_FALSE(sock_->WasEverUsed());
  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->WasEverUsed());
  sock_->Disconnect();
  EXPECT_TRUE(sock_->WasEverUsed());
}

// ----------- GetPeerAddress

TEST_F(SpdyProxyClientSocketSpdy3Test, GetPeerAddressReturnsCorrectValues) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  net::IPEndPoint addr;
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->GetPeerAddress(&addr));

  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(OK, sock_->GetPeerAddress(&addr));

  Run(1);

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->GetPeerAddress(&addr));

  sock_->Disconnect();

  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->GetPeerAddress(&addr));
}

// ----------- Write

TEST_F(SpdyProxyClientSocketSpdy3Test, WriteSendsDataInDataFrame) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
    CreateMockWrite(*msg1, 2, SYNCHRONOUS),
    CreateMockWrite(*msg2, 3, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  AssertAsyncWriteSucceeds(kMsg1, kLen1);
  AssertAsyncWriteSucceeds(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, WriteSplitsLargeDataIntoMultipleFrames) {
  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<SpdyFrame> chunk(ConstructBodyFrame(chunk_data.data(),
                                                       chunk_data.length()));
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
    CreateMockWrite(*chunk, 2, SYNCHRONOUS),
    CreateMockWrite(*chunk, 3, SYNCHRONOUS),
    CreateMockWrite(*chunk, 4, SYNCHRONOUS)
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 5),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  std::string big_data(kMaxSpdyFrameChunkSize * 3, 'x');
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(big_data.data(),
                                                   big_data.length()));

  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(buf, buf->size(),
                                         write_callback_.callback()));
  data_->RunFor(3);

  EXPECT_EQ(buf->size(), write_callback_.WaitForResult());
}

// ----------- Read

TEST_F(SpdyProxyClientSocketSpdy3Test, ReadReadsDataInDataFrame) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ReadDataFromBufferedFrames) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    CreateMockRead(*msg2, 3, ASYNC),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ReadDataMultipleBufferedFrames) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    CreateMockRead(*msg2, 3, ASYNC),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketSpdy3Test,
       LargeReadWillMergeDataFromDifferentFrames) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg3, 2, ASYNC),
    CreateMockRead(*msg3, 3, ASYNC),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, MultipleShortReadsThenMoreRead) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    CreateMockRead(*msg3, 3, ASYNC),
    CreateMockRead(*msg3, 4, ASYNC),
    CreateMockRead(*msg2, 5, ASYNC),
    MockRead(ASYNC, 0, 6),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(4);  // SpdySession consumes the next four reads and sends then to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ReadWillSplitDataFromLargeFrame) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg33(ConstructBodyFrame(kMsg33, kLen33));
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    CreateMockRead(*msg33, 3, ASYNC),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg3, kLen3);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, MultipleReadsFromSameLargeFrame) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg333(ConstructBodyFrame(kMsg333, kLen333));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg333, 2, ASYNC),
    MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg33, kLen33);

  // Now attempt to do a read of more data than remains buffered
  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen33));
  ASSERT_EQ(kLen3, sock_->Read(buf, kLen33, read_callback_.callback()));
  ASSERT_EQ(std::string(kMsg3, kLen3), std::string(buf->data(), kLen3));
  ASSERT_TRUE(sock_->IsConnected());
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ReadAuthResponseBody) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectAuthReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    CreateMockRead(*msg2, 3, ASYNC),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, ReadErrorResponseBody) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectErrorReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    CreateMockRead(*msg2, 3, ASYNC),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);
}

// ----------- Reads and Writes

TEST_F(SpdyProxyClientSocketSpdy3Test, AsyncReadAroundWrite) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
    CreateMockWrite(*msg2, 3, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),  // sync read
    CreateMockRead(*msg3, 4, ASYNC),  // async read
    MockRead(ASYNC, 0, 5),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);
  AssertSyncReadEquals(kMsg1, kLen1);

  AssertReadStarts(kMsg3, kLen3);
  // Read should block until after the write succeeds

  AssertAsyncWriteSucceeds(kMsg2, kLen2);  // Runs 1 step

  ASSERT_FALSE(read_callback_.have_result());
  Run(1);
  // Now the read will return
  AssertReadReturns(kMsg3, kLen3);
}

TEST_F(SpdyProxyClientSocketSpdy3Test, AsyncWriteAroundReads) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
    CreateMockWrite(*msg2, 4, ASYNC),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<SpdyFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    CreateMockRead(*msg3, 3, ASYNC),
    MockRead(ASYNC, 0, 5),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);
  AssertSyncReadEquals(kMsg1, kLen1);
  // Write should block until the read completes
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  AssertAsyncReadEquals(kMsg3, kLen3);

  ASSERT_FALSE(write_callback_.have_result());

  // Now the write will complete
  Run(1);
  AssertWriteLength(kLen2);
}

// ----------- Reading/Writing on Closed socket

// Reading from an already closed socket should return 0
TEST_F(SpdyProxyClientSocketSpdy3Test, ReadOnClosedSocketReturnsZero) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);

  ASSERT_FALSE(sock_->IsConnected());
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  ASSERT_FALSE(sock_->IsConnectedAndIdle());
}

// Read pending when socket is closed should return 0
TEST_F(SpdyProxyClientSocketSpdy3Test, PendingReadOnCloseReturnsZero) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  AssertReadStarts(kMsg1, kLen1);

  Run(1);

  ASSERT_EQ(0, read_callback_.WaitForResult());
}

// Reading from a disconnected socket is an error
TEST_F(SpdyProxyClientSocketSpdy3Test,
       ReadOnDisconnectSocketReturnsNotConnected) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  sock_->Disconnect();

  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(NULL, 1, CompletionCallback()));
}

// Reading buffered data from an already closed socket should return
// buffered data, then 0.
TEST_F(SpdyProxyClientSocketSpdy3Test, ReadOnClosedSocketReturnsBufferedData) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(2);

  ASSERT_FALSE(sock_->IsConnected());
  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen1));
  ASSERT_EQ(kLen1, sock_->Read(buf, kLen1, CompletionCallback()));
  ASSERT_EQ(std::string(kMsg1, kLen1), std::string(buf->data(), kLen1));

  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  sock_->Disconnect();
  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(NULL, 1, CompletionCallback()));
}

// Calling Write() on a closed socket is an error
TEST_F(SpdyProxyClientSocketSpdy3Test, WriteOnClosedStream) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // Read EOF which will close the stream
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Write(buf, buf->size(), CompletionCallback()));
}

// Calling Write() on a disconnected socket is an error
TEST_F(SpdyProxyClientSocketSpdy3Test, WriteOnDisconnectedSocket) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  sock_->Disconnect();

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Write(buf, buf->size(), CompletionCallback()));
}

// If the socket is closed with a pending Write(), the callback
// should be called with ERR_CONNECTION_CLOSED.
TEST_F(SpdyProxyClientSocketSpdy3Test, WritePendingOnClose) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
    MockWrite(ASYNC, ERR_IO_PENDING, 2),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(buf, buf->size(), write_callback_.callback()));

  Run(1);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, write_callback_.WaitForResult());
}

// If the socket is Disconnected with a pending Write(), the callback
// should not be called.
TEST_F(SpdyProxyClientSocketSpdy3Test, DisconnectWithWritePending) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
    MockWrite(SYNCHRONOUS, 0, 2),  // EOF
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(buf, buf->size(), write_callback_.callback()));

  sock_->Disconnect();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(write_callback_.have_result());
}

// If the socket is Disconnected with a pending Read(), the callback
// should not be called.
TEST_F(SpdyProxyClientSocketSpdy3Test, DisconnectWithReadPending) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(buf, kLen1, read_callback_.callback()));

  sock_->Disconnect();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(read_callback_.have_result());
}

// If the socket is Reset when both a read and write are pending,
// both should be called back.
TEST_F(SpdyProxyClientSocketSpdy3Test, RstWithReadAndWritePending) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
    MockWrite(ASYNC, ERR_IO_PENDING, 2),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> rst(ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*rst, 3, ASYNC),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBuffer> read_buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf, kLen1, read_callback_.callback()));

  scoped_refptr<IOBufferWithSize> write_buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(write_buf, write_buf->size(),
                         write_callback_.callback()));

  Run(2);

  EXPECT_TRUE(sock_.get());
  EXPECT_TRUE(read_callback_.have_result());
  EXPECT_TRUE(write_callback_.have_result());
}

// Makes sure the proxy client socket's source gets the expected NetLog events
// and only the expected NetLog events (No SpdySession events).
TEST_F(SpdyProxyClientSocketSpdy3Test, NetLog) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);

  NetLog::Source sock_source = sock_->NetLog().source();
  sock_.reset();

  CapturingNetLog::CapturedEntryList entry_list;
  net_log_.GetEntriesForSource(sock_source, &entry_list);

  ASSERT_EQ(entry_list.size(), 10u);
  EXPECT_TRUE(LogContainsBeginEvent(entry_list, 0, NetLog::TYPE_SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(entry_list, 1,
                  NetLog::TYPE_SPDY_PROXY_CLIENT_SESSION,
                  NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsBeginEvent(entry_list, 2,
                  NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsEvent(entry_list, 3,
                  NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
                  NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(entry_list, 4,
                  NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsBeginEvent(entry_list, 5,
                  NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(entry_list, 6,
                  NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
                  NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(entry_list, 7,
                  NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(entry_list, 8,
                  NetLog::TYPE_SOCKET_BYTES_RECEIVED,
                  NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(entry_list, 9, NetLog::TYPE_SOCKET_ALIVE));
}

// CompletionCallback that causes the SpdyProxyClientSocket to be
// deleted when Run is invoked.
class DeleteSockCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteSockCallback(scoped_ptr<SpdyProxyClientSocket>* sock)
      : sock_(sock),
        callback_(base::Bind(&DeleteSockCallback::OnComplete,
                             base::Unretained(this))) {
  }

  virtual ~DeleteSockCallback() {
  }

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    sock_->reset(NULL);
    SetResult(result);
  }

  scoped_ptr<SpdyProxyClientSocket>* sock_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteSockCallback);
};

// If the socket is Reset when both a read and write are pending, and the
// read callback causes the socket to be deleted, the write callback should
// not be called.
TEST_F(SpdyProxyClientSocketSpdy3Test, RstWithReadAndWritePendingDelete) {
  scoped_ptr<SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
    MockWrite(ASYNC, ERR_IO_PENDING, 2),
  };

  scoped_ptr<SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<SpdyFrame> rst(ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*rst, 3, ASYNC),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  DeleteSockCallback read_callback(&sock_);

  scoped_refptr<IOBuffer> read_buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf, kLen1, read_callback.callback()));

  scoped_refptr<IOBufferWithSize> write_buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(write_buf, write_buf->size(),
                                         write_callback_.callback()));

  Run(2);

  EXPECT_FALSE(sock_.get());
  EXPECT_TRUE(read_callback.have_result());
  EXPECT_FALSE(write_callback_.have_result());
}

}  // namespace net
