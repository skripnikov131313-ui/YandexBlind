#pragma once
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"

class SkAiClient final : public CefClient, public CefLifeSpanHandler {
 public:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
 private:
  CefRefPtr<CefBrowser> browser_;
  IMPLEMENT_REFCOUNTING(SkAiClient);
};
