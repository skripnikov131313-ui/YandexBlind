#pragma once

#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"

class SkAiApp final : public CefApp,
                      public CefBrowserProcessHandler,
                      public CefRenderProcessHandler {
 public:
  SkAiApp() = default;

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;

 private:
  IMPLEMENT_REFCOUNTING(SkAiApp);
};
