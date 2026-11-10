#include "skai_client.h"

bool SkAiClient::OnProcessMessageReceived(CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame>, CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  if (source_process != PID_RENDERER || message->GetName() != "skai.windowAction")
    return false;

  const auto args = message->GetArgumentList();
  const auto action = args->GetString(0);
  if (!IsWindow(host_window_)) return true;

  if (action == "minimize") {
    ShowWindow(host_window_, SW_MINIMIZE);
  } else if (action == "toggleMaximize") {
    ShowWindow(host_window_, IsZoomed(host_window_) ? SW_RESTORE : SW_MAXIMIZE);
  } else if (action == "close") {
    PostMessageW(host_window_, WM_CLOSE, 0, 0);
  }
  return true;
}

void SkAiClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  browser_ = browser;
}
bool SkAiClient::DoClose(CefRefPtr<CefBrowser>) {
  return false;
}
void SkAiClient::OnBeforeClose(CefRefPtr<CefBrowser>) {
  browser_ = nullptr;
  if (IsWindow(host_window_)) PostMessageW(host_window_, WM_APP + 11, 0, 0);
}
