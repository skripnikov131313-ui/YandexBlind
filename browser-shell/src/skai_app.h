#pragma once
#include "include/cef_app.h"

class SkAiApp final : public CefApp, public CefBrowserProcessHandler {
 public:
  SkAiApp() = default;
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
 private:
  IMPLEMENT_REFCOUNTING(SkAiApp);
};
