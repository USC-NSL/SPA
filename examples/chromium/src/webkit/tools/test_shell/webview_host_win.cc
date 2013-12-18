// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/webview_host.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"

using namespace WebKit;

static const wchar_t kWindowClassName[] = L"WebViewHost";

/*static*/
WebViewHost* WebViewHost::Create(HWND parent_view,
                                 TestWebViewDelegate* delegate,
                                 WebDevToolsAgentClient* dev_tools_client,
                                 const webkit_glue::WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost();

  static bool registered_class = false;
  if (!registered_class) {
    WNDCLASSEX wcex = {0};
    wcex.cbSize        = sizeof(wcex);
    wcex.style         = CS_DBLCLKS;
    wcex.lpfnWndProc   = WebWidgetHost::WndProc;
    wcex.hInstance     = GetModuleHandle(NULL);
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = kWindowClassName;
    RegisterClassEx(&wcex);
    registered_class = true;
  }

  host->view_ = CreateWindow(kWindowClassName, NULL,
                             WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0,
                             0, 0, parent_view, NULL,
                             GetModuleHandle(NULL), NULL);
  ui::SetWindowUserData(host->view_, host);

  host->webwidget_ = WebView::create(delegate);
  host->webview()->setDevToolsAgentClient(dev_tools_client);
  prefs.Apply(host->webview());
  host->webview()->settings()->setExperimentalCSSGridLayoutEnabled(true);
  host->webview()->initializeMainFrame(delegate);

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}
