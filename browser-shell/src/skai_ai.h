#pragma once

#include "include/cef_browser.h"

#include <string>

// All operations are asynchronous. UI callbacks are delivered to the supplied
// CEF browser on its UI thread.
void SkAiAiQueryStatus(CefRefPtr<CefBrowser> browser);
void SkAiAiInstallModel(CefRefPtr<CefBrowser> browser);
void SkAiAiSendChat(CefRefPtr<CefBrowser> browser,
                    const std::wstring& request_id,
                    const std::wstring& openai_request_json);
void SkAiAiShutdown();
