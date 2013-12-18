// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPAPI_WEBPLUGIN_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPAPI_WEBPLUGIN_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace WebKit {
struct WebPluginParams;
struct WebPrintParams;
}

namespace webkit {
namespace ppapi {

class PluginDelegate;
class PluginInstance;
class PluginModule;
class PPB_URLLoader_Impl;

class WebPluginImpl : public WebKit::WebPlugin {
 public:
  WEBKIT_PLUGINS_EXPORT WebPluginImpl(
      PluginModule* module,
      const WebKit::WebPluginParams& params,
      const base::WeakPtr<PluginDelegate>& plugin_delegate);

  PluginInstance* instance() { return instance_.get(); }

  // WebKit::WebPlugin implementation.
  virtual WebKit::WebPluginContainer* container() const;
  virtual bool initialize(WebKit::WebPluginContainer* container);
  virtual void destroy();
  virtual NPObject* scriptableObject();
  virtual bool getFormValue(WebKit::WebString& value);
  virtual void paint(WebKit::WebCanvas* canvas, const WebKit::WebRect& rect);
  virtual void updateGeometry(
      const WebKit::WebRect& frame_rect,
      const WebKit::WebRect& clip_rect,
      const WebKit::WebVector<WebKit::WebRect>& cut_outs_rects,
      bool is_visible);
  virtual void updateFocus(bool focused);
  virtual void updateVisibility(bool visible);
  virtual bool acceptsInputEvents();
  virtual bool handleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo& cursor_info);
  virtual void didReceiveResponse(const WebKit::WebURLResponse& response);
  virtual void didReceiveData(const char* data, int data_length);
  virtual void didFinishLoading();
  virtual void didFailLoading(const WebKit::WebURLError&);
  virtual void didFinishLoadingFrameRequest(const WebKit::WebURL& url,
                                            void* notify_data);
  virtual void didFailLoadingFrameRequest(const WebKit::WebURL& url,
                                          void* notify_data,
                                          const WebKit::WebURLError& error);
  virtual bool hasSelection() const;
  virtual WebKit::WebString selectionAsText() const;
  virtual WebKit::WebString selectionAsMarkup() const;
  virtual WebKit::WebURL linkAtPosition(const WebKit::WebPoint& position) const;
  virtual void setZoomLevel(double level, bool text_only);
  virtual bool startFind(const WebKit::WebString& search_text,
                         bool case_sensitive,
                         int identifier);
  virtual void selectFindResult(bool forward);
  virtual void stopFind();
  virtual bool supportsPaginatedPrint() OVERRIDE;
  virtual bool isPrintScalingDisabled() OVERRIDE;

  virtual int printBegin(const WebKit::WebPrintParams& print_params) OVERRIDE;
  virtual bool printPage(int page_number, WebKit::WebCanvas* canvas) OVERRIDE;
  virtual void printEnd() OVERRIDE;

  virtual bool canRotateView() OVERRIDE;
  virtual void rotateView(RotationType type) OVERRIDE;
  virtual bool isPlaceholder() OVERRIDE;

 private:
  friend class base::DeleteHelper<WebPluginImpl>;

  WEBKIT_PLUGINS_EXPORT virtual ~WebPluginImpl();
  struct InitData;

  scoped_ptr<InitData> init_data_;  // Cleared upon successful initialization.
  // True if the instance represents the entire document in a frame instead of
  // being an embedded resource.
  bool full_frame_;
  scoped_refptr<PluginInstance> instance_;
  scoped_refptr<PPB_URLLoader_Impl> document_loader_;
  gfx::Rect plugin_rect_;
  PP_Var instance_object_;
  WebKit::WebPluginContainer* container_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginImpl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPAPI_WEBPLUGIN_IMPL_H_
