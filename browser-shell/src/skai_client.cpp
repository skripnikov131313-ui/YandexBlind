#include "skai_client.h"
#include "include/cef_parser.h"

#include <string>

bool SkAiClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame>, CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  if (source_process != PID_RENDERER || message->GetName() != "skai.command")
    return false;

  const auto args = message->GetArgumentList();
  const auto category = args->GetString(0);
  const auto action = args->GetString(1);
  if (category == "window") {
    if (!IsWindow(host_window_)) return true;
    if (action == "minimize") ShowWindow(host_window_, SW_MINIMIZE);
    else if (action == "toggleMaximize")
      ShowWindow(host_window_, IsZoomed(host_window_) ? SW_RESTORE : SW_MAXIMIZE);
    else if (action == "close") PostMessageW(host_window_, WM_CLOSE, 0, 0);
    return true;
  }
  if (!browser) return true;
  if (category == "navigate") {
    std::wstring target = action.ToWString();
    if (target.find(L"://") == std::wstring::npos)
      target = L"https://" + target;
    browser->GetMainFrame()->LoadURL(target);
    return true;
  }
  if (category == "browser") {
    if (action == "back" && browser->CanGoBack()) browser->GoBack();
    else if (action == "forward" && browser->CanGoForward()) browser->GoForward();
    else if (action == "reload") browser->Reload();
    return true;
  }
  return false;
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
void SkAiClient::OnAddressChange(CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame> frame, const CefString& url) {
  if (!frame->IsMain()) return;
  const CefString json = CefWriteJSON(CefV8Value::CreateString(url), JSON_WRITER_DEFAULT);
  const std::wstring script = L"window.skaiNative && window.skaiNative.addressChanged(" +
      json.ToWString() + L");";
  frame->ExecuteJavaScript(script, frame->GetURL(), 0);
}
