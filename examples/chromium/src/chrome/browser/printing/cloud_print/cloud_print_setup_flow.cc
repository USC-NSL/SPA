// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"

#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_message_handler.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_source.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/chromium_strings.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/gfx/font.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

string16 SetupIframeXPath() {
  return ASCIIToUTF16("//iframe[@id='cloudprintsetup']");
}

string16 DoneIframeXPath() {
  return ASCIIToUTF16("//iframe[@id='setupdone']");
}

}  // end namespace

////////////////////////////////////////////////////////////////////////////////
// CloudPrintSetupFlow implementation.

// static
CloudPrintSetupFlow* CloudPrintSetupFlow::OpenDialog(
    Profile* profile,
    const base::WeakPtr<Delegate>& delegate,
    gfx::NativeWindow parent_window) {
  DCHECK(profile);
  Browser* browser = chrome::FindLastActiveWithProfile(profile,
      chrome::GetActiveDesktop());
  // Set the arguments for showing the gaia login page.
  DictionaryValue args;
  args.SetString("user", std::string());
  args.SetInteger("error", 0);
  args.SetBoolean("editable_user", true);

  bool setup_done = false;
  if (profile->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      !profile->GetPrefs()->GetString(prefs::kCloudPrintEmail).empty())
    setup_done = true;
  args.SetString("pageToShow", setup_done ? "setupdone" : "cloudprintsetup");

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  CloudPrintSetupFlow* flow = new CloudPrintSetupFlow(json_args, profile,
                                                      delegate, setup_done);
  // We may not always have a browser. This can happen when we are being
  // invoked in the context of a "token expired" notfication. If we don't have
  // a brower, use the underlying dialog system to show the dialog without
  // using a browser.
  if (!parent_window && browser && browser->window())
    parent_window = browser->window()->GetNativeWindow();
  chrome::ShowWebDialog(parent_window, profile, flow);
  return flow;
}

CloudPrintSetupFlow::CloudPrintSetupFlow(
    const std::string& args,
    Profile* profile,
    const base::WeakPtr<Delegate>& delegate,
    bool setup_done)
    : web_ui_(NULL),
      dialog_start_args_(args),
      last_auth_error_(GoogleServiceAuthError::AuthErrorNone()),
      setup_done_(setup_done),
      process_control_(NULL),
      delegate_(delegate) {
  // TODO(hclam): The data source should be added once.
  profile_ = profile;
  content::URLDataSource::Add(profile, new CloudPrintSetupSource());
}

CloudPrintSetupFlow::~CloudPrintSetupFlow() {
}

void CloudPrintSetupFlow::Focus() {
  // TODO(pranavk): implement this method.
  NOTIMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////
// ui::WebDialogDelegate implementation.
GURL CloudPrintSetupFlow::GetDialogContentURL() const {
  return GURL("chrome://cloudprintsetup/setupflow");
}

void CloudPrintSetupFlow::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  // Create the message handler only after we are asked, the caller is
  // responsible for deleting the objects.
  handlers->push_back(
      new CloudPrintSetupMessageHandler(
          const_cast<CloudPrintSetupFlow*>(this)));
}

void CloudPrintSetupFlow::GetDialogSize(gfx::Size* size) const {
  PrefService* prefs = profile_->GetPrefs();
  gfx::Font approximate_web_font(
      prefs->GetString(prefs::kWebKitSansSerifFontFamily),
      prefs->GetInteger(prefs::kWebKitDefaultFontSize));

  if (setup_done_) {
    *size = ui::GetLocalizedContentsSizeForFont(
        IDS_CLOUD_PRINT_SETUP_WIZARD_DONE_WIDTH_CHARS,
        IDS_CLOUD_PRINT_SETUP_WIZARD_DONE_HEIGHT_LINES,
        approximate_web_font);
  } else {
    *size = ui::GetLocalizedContentsSizeForFont(
        IDS_CLOUD_PRINT_SETUP_WIZARD_WIDTH_CHARS,
        IDS_CLOUD_PRINT_SETUP_WIZARD_HEIGHT_LINES,
        approximate_web_font);
  }

#if !defined(OS_WIN)
  // NOTE(scottbyer):The following comment comes from
  // SyncSetupFlow::GetDialogSize, where this hack was copied from. By starting
  // off development of the UI on Windows, the hack is seemingly backwards.

  // NOTE(akalin): This is a hack to work around a problem with font height on
  // windows.  Basically font metrics are incorrectly returned in logical units
  // instead of pixels on Windows.  Logical units are very commonly 96 DPI
  // so our localized char/line counts are too small by a factor of 96/72.
  // So we compensate for this on non-windows platform.

  // TODO(scottbyer): Fix the root cause, kill the hacks.
  float scale_hack = 96.f/72.f;
  size->set_width(size->width() * scale_hack);
  size->set_height(size->height() * scale_hack);
#endif
}

// A callback to notify the delegate that the dialog closed.
void CloudPrintSetupFlow::OnDialogClosed(const std::string& json_retval) {
  // If we are fetching the token then cancel the request.
  if (authenticator_.get())
    authenticator_->CancelRequest();

  if (delegate_)
    delegate_->OnDialogClosed();
  delete this;
}

std::string CloudPrintSetupFlow::GetDialogArgs() const {
  return dialog_start_args_;
}

void CloudPrintSetupFlow::OnCloseContents(WebContents* source,
                                          bool* out_close_dialog) {
}

string16 CloudPrintSetupFlow::GetDialogTitle() const {
  return l10n_util::GetStringFUTF16(IDS_CLOUD_PRINT_SETUP_DIALOG_TITLE,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT));
}

ui::ModalType CloudPrintSetupFlow::GetDialogModalType() const {
  // We are always modeless.
  return ui::MODAL_TYPE_NONE;
}

bool CloudPrintSetupFlow::ShouldShowDialogTitle() const {
  return true;
}

bool CloudPrintSetupFlow::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// GaiaAuthConsumer implementation.
void CloudPrintSetupFlow::OnClientLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Save the token for the cloud print proxy.
  lsid_ = credentials.lsid;

  // Show that Gaia login has succeeded.
  ShowGaiaSuccessAndSettingUp();
  authenticator_.reset();

  CloudPrintProxyServiceFactory::GetForProfile(profile_)->EnableForUser(
      credentials.lsid, login_);
  // TODO(sanjeevr): Should we wait and verify that the enable succeeded?
  ShowSetupDone();
}

void CloudPrintSetupFlow::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  last_auth_error_ = error;
  ShowGaiaFailed(error);
  authenticator_.reset();
}

///////////////////////////////////////////////////////////////////////////////
// Methods called by CloudPrintSetupMessageHandler
void CloudPrintSetupFlow::Attach(content::WebUI* web_ui) {
  web_ui_ = web_ui;
}

void CloudPrintSetupFlow::OnUserSubmittedAuth(const std::string& user,
                                              const std::string& password,
                                              const std::string& captcha,
                                              const std::string& access_code) {
  // Save the login name only.
  login_ = user;

  // Start the authenticator.
  authenticator_.reset(
      new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                          profile_->GetRequestContext()));

  if (!access_code.empty()) {
    authenticator_->StartClientLogin(user, access_code,
                                     GaiaConstants::kCloudPrintService,
                                     std::string(), std::string(),
                                     GaiaAuthFetcher::HostedAccountsAllowed);
  } else {
    authenticator_->StartClientLogin(user, password,
                                     GaiaConstants::kCloudPrintService,
                                     last_auth_error_.captcha().token, captcha,
                                     GaiaAuthFetcher::HostedAccountsAllowed);
  }
}

void CloudPrintSetupFlow::OnUserClickedLearnMore() {
  OpenURLParams params(CloudPrintURL::GetCloudPrintLearnMoreURL(),
                       Referrer(), NEW_FOREGROUND_TAB,
                       content::PAGE_TRANSITION_LINK, false);
  web_ui_->GetWebContents()->OpenURL(params);
}

void CloudPrintSetupFlow::OnUserClickedPrintTestPage() {
  OpenURLParams params(CloudPrintURL::GetCloudPrintTestPageURL(),
                       Referrer(), NEW_FOREGROUND_TAB,
                       content::PAGE_TRANSITION_LINK, false);
  web_ui_->GetWebContents()->OpenURL(params);
}

///////////////////////////////////////////////////////////////////////////////
// Helper methods for showing contents of the Web UI
void CloudPrintSetupFlow::ShowGaiaLogin(const DictionaryValue& args) {
  if (web_ui_)
    web_ui_->CallJavascriptFunction("cloudprint.showSetupLogin");

  std::string json;
  base::JSONWriter::Write(&args, &json);
  string16 javascript = UTF8ToUTF16("cloudprint.showGaiaLogin(" + json + ");");

  ExecuteJavascriptInIFrame(SetupIframeXPath(), javascript);
}

void CloudPrintSetupFlow::ShowGaiaSuccessAndSettingUp() {
  ExecuteJavascriptInIFrame(
      SetupIframeXPath(),
      ASCIIToUTF16("cloudprint.showGaiaSuccessAndSettingUp();"));
}

void CloudPrintSetupFlow::ShowGaiaFailed(const GoogleServiceAuthError& error) {
  DictionaryValue args;
  args.SetString("pageToShow", "cloudprintsetup");
  args.SetString("user", login_);
  args.SetInteger("error", error.state());
  args.SetBoolean("editable_user", true);
  args.SetString("captchaUrl", error.captcha().image_url.spec());
  ShowGaiaLogin(args);
}

void CloudPrintSetupFlow::ShowSetupDone() {
  setup_done_ = true;
  string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  string16 message = l10n_util::GetStringFUTF16(IDS_CLOUD_PRINT_SETUP_DONE,
                                                product_name,
                                                UTF8ToUTF16(login_));
  string16 javascript(ASCIIToUTF16("cloudprint.setMessage('") + message +
                      ASCIIToUTF16("');"));
  ExecuteJavascriptInIFrame(DoneIframeXPath(), javascript);

  if (web_ui_) {
    PrefService* prefs = profile_->GetPrefs();
    gfx::Font approximate_web_font(
        prefs->GetString(prefs::kWebKitSansSerifFontFamily),
        prefs->GetInteger(prefs::kWebKitDefaultFontSize));
    gfx::Size done_size = ui::GetLocalizedContentsSizeForFont(
        IDS_CLOUD_PRINT_SETUP_WIZARD_DONE_WIDTH_CHARS,
        IDS_CLOUD_PRINT_SETUP_WIZARD_DONE_HEIGHT_LINES,
        approximate_web_font);

    base::FundamentalValue new_width(done_size.width());
    base::FundamentalValue new_height(done_size.height());
    web_ui_->CallJavascriptFunction("cloudprint.showSetupDone",
                                    new_width, new_height);
  }

  ExecuteJavascriptInIFrame(DoneIframeXPath(),
                            ASCIIToUTF16("cloudprint.onPageShown();"));
}

void CloudPrintSetupFlow::ExecuteJavascriptInIFrame(
    const string16& iframe_xpath,
    const string16& js) {
  if (web_ui_) {
    content::RenderViewHost* rvh =
        web_ui_->GetWebContents()->GetRenderViewHost();
    rvh->ExecuteJavascriptInWebFrame(iframe_xpath, js);
  }
}
