// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DB_MESSAGE_FILTER_H_
#define CONTENT_COMMON_DB_MESSAGE_FILTER_H_

#include "ipc/ipc_channel_proxy.h"

namespace content {

// Receives database messages from the browser process and processes them on the
// IO thread.
class DBMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  DBMessageFilter();

  // IPC::ChannelProxy::MessageFilter
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  virtual ~DBMessageFilter() {}

 private:
  void OnDatabaseUpdateSize(const string16& origin_identifier,
                            const string16& database_name,
                            int64 database_size);
  void OnDatabaseUpdateSpaceAvailable(const string16& origin_identifier,
                                      int64 space_available);
  void OnDatabaseResetSpaceAvailable(const string16& origin_identifier);
  void OnDatabaseCloseImmediately(const string16& origin_identifier,
                                  const string16& database_name);
};

}  // namespace content

#endif  // CONTENT_COMMON_DB_MESSAGE_FILTER_H_
