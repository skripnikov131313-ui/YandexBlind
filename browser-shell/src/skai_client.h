#pragma once
#include <windows.h>
#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_life_span_handler.h"

class SkAiClient final : public CefClient, public CefLifeSpanHandler {
 public:
  explicit SkAiClient(HWND host_window) : host_window_(host_window) {}
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame, CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
 private:
  HWND host_window_ = nullptr;
  CefRefPtr<CefBrowser> browser_;
  IMPLEMENT_REFCOUNTING(SkAiClient);
};
