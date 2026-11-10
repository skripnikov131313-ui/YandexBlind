#pragma once
#include <windows.h>
#include <memory>
#include <string>
#include <vector>

#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_display_handler.h"

class SkAiClient final : public CefClient,
                         public CefLifeSpanHandler,
                         public CefDisplayHandler {
 public:
  SkAiClient(HWND host_window, int tab_id, bool ui_browser);
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame, CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame, const CefString& url) override;
  void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
  void OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
      const std::vector<CefString>& icon_urls) override;
  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
  bool is_closing() const { return closing_; }
  void RequestClose();
  int tab_id() const { return tab_id_; }
  bool is_ui_browser() const { return ui_browser_; }
 private:
  HWND host_window_ = nullptr;
  int tab_id_ = 0;
  bool ui_browser_ = false;
  bool closing_ = false;
  CefRefPtr<CefBrowser> browser_;
  IMPLEMENT_REFCOUNTING(SkAiClient);
};
