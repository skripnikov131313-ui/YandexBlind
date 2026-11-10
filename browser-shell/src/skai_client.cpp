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
    CefString url = action;
    if (url.find(L"://") == std::wstring::npos &&
        url.find(L"localhost") != 0 &&
        url.find(L"127.0.0.1") != 0 &&
        url.find(L".") == std::wstring::npos) {
      url = L"https://www.google.com/search?q=" + CefURIEncode(url, false).ToWString();
    } else if (url.find(L"://") == std::wstring::npos) {
      url = L"https://" + url.ToWString();
    }
    browser->GetMainFrame()->LoadURL(url);
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
void SkAiClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame, const CefString& url) {
  if (!frame->IsMain()) return;
  const auto script = "window.skaiNative && window.skaiNative.addressChanged(" +
      CefURIEncode(url, true).ToString() + ");";
  // The renderer bridge receives the current URL through a safely quoted JS string.
  const auto encoded = CefURIDecode(CefURIEncode(url, true), false, URIDECODE_NONE);
  const auto value = encoded.ToString();
  std::wstring escaped;
  for (wchar_t ch : value) {
    if (ch == L'\\' || ch == L'\"') escaped.push_back(L'\\');
    if (ch == L'\r' || ch == L'\n') continue;
    escaped.push_back(ch);
  }
  const auto js = L"window.skaiNative && window.skaiNative.addressChanged(\"" + escaped + L"\");";
  frame->ExecuteJavaScript(js, frame->GetURL(), 0);
}
