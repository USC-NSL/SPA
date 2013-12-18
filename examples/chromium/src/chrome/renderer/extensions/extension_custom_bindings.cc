// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/view_type.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

}  // namespace

ExtensionCustomBindings::ExtensionCustomBindings(
    Dispatcher* dispatcher,
    v8::Handle<v8::Context> context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetExtensionViews",
      base::Bind(&ExtensionCustomBindings::GetExtensionViews,
                 base::Unretained(this)));
}

v8::Handle<v8::Value> ExtensionCustomBindings::GetExtensionViews(
    const v8::Arguments& args) {
  if (args.Length() != 2)
    return v8::Undefined();

  if (!args[0]->IsInt32() || !args[1]->IsString())
    return v8::Undefined();

  // |browser_window_id| == extension_misc::kUnknownWindowId means getting
  // views attached to any browser window.
  int browser_window_id = args[0]->Int32Value();

  std::string view_type_string = *v8::String::Utf8Value(args[1]->ToString());
  StringToUpperASCII(&view_type_string);
  // |view_type| == VIEW_TYPE_INVALID means getting any type of
  // views.
  ViewType view_type = VIEW_TYPE_INVALID;
  if (view_type_string == kViewTypeBackgroundPage) {
    view_type = VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
  } else if (view_type_string == kViewTypeInfobar) {
    view_type = VIEW_TYPE_EXTENSION_INFOBAR;
  } else if (view_type_string == kViewTypeNotification) {
    view_type = VIEW_TYPE_NOTIFICATION;
  } else if (view_type_string == kViewTypeTabContents) {
    view_type = VIEW_TYPE_TAB_CONTENTS;
  } else if (view_type_string == kViewTypePopup) {
    view_type = VIEW_TYPE_EXTENSION_POPUP;
  } else if (view_type_string == kViewTypeExtensionDialog) {
    view_type = VIEW_TYPE_EXTENSION_DIALOG;
  } else if (view_type_string == kViewTypeAppShell) {
    view_type = VIEW_TYPE_APP_SHELL;
  } else if (view_type_string == kViewTypePanel) {
    view_type = VIEW_TYPE_PANEL;
  } else if (view_type_string != kViewTypeAll) {
    return v8::Undefined();
  }

  const Extension* extension = GetExtensionForRenderView();
  if (!extension)
    return v8::Undefined();

  std::vector<content::RenderView*> views = ExtensionHelper::GetExtensionViews(
      extension->id(), browser_window_id, view_type);
  v8::Local<v8::Array> v8_views = v8::Array::New();
  int v8_index = 0;
  for (size_t i = 0; i < views.size(); ++i) {
    v8::Local<v8::Context> context =
        views[i]->GetWebView()->mainFrame()->mainWorldScriptContext();
    if (!context.IsEmpty()) {
      v8::Local<v8::Value> window = context->Global();
      DCHECK(!window.IsEmpty());
      v8_views->Set(v8::Integer::New(v8_index++), window);
    }
  }

  return v8_views;
}

}  // namespace extensions
