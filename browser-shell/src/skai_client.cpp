#include "skai_client.h"
#include "skai_window.h"
#include "include/cef_parser.h"
#include "include/cef_values.h"

#include <string>

SkAiClient::SkAiClient(HWND host_window, int tab_id, bool ui_browser)
    : host_window_(host_window), tab_id_(tab_id), ui_browser_(ui_browser) {}

bool SkAiClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame>, CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  if (!ui_browser_ || source_process != PID_RENDERER || message->GetName() != "skai.command")
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
  if (category == "tab") {
    if (action == "new") {
      const std::wstring url = args->GetString(2).ToWString();
      SkAiCreateTab(url.empty() ? L"about:blank" : url, true);
    } else if (action == "activate") {
      SkAiActivateTab(args->GetInt(2));
    } else if (action == "close") {
      SkAiCloseTab(args->GetInt(2));
    }
    return true;
  }
  if (category == "layout") {
    SkAiSetContentBounds(args->GetInt(2), args->GetInt(3),
                         args->GetInt(4), args->GetInt(5));
    return true;
  }
  if (!browser) return true;
  if (category == "navigate") {
    std::wstring target = action.ToWString();
    if (target.find(L"://") == std::wstring::npos)
      target = L"https://" + target;
    SkAiNavigateActive(target);
    return true;
  }
  if (category == "browser") {
    SkAiBrowserAction(action.ToWString());
    return true;
  }
  return false;
}
void SkAiClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  browser_ = browser;
  if (!ui_browser_) SkAiNotifyTabState(tab_id_, L"Новая вкладка", browser->GetMainFrame()->GetURL(), false);
}
bool SkAiClient::DoClose(CefRefPtr<CefBrowser>) { return false; }
void SkAiClient::OnBeforeClose(CefRefPtr<CefBrowser>) {
  browser_ = nullptr;
  if (!ui_browser_) SkAiOnTabClosed(tab_id_);
  else if (IsWindow(host_window_)) PostMessageW(host_window_, WM_APP + 11, 0, 0);
}
void SkAiClient::OnAddressChange(CefRefPtr<CefBrowser>,
    CefRefPtr<CefFrame> frame, const CefString& url) {
  if (!frame->IsMain()) return;
  if (ui_browser_) {
    auto value = CefValue::Create(); value->SetString(url);
    const CefString json = CefWriteJSON(value, JSON_WRITER_DEFAULT);
    const std::wstring script = L"window.skaiNative&&window.skaiNative.addressChanged(" +
        json.ToWString() + L");";
    frame->ExecuteJavaScript(script, frame->GetURL(), 0);
  } else {
    SkAiNotifyTabState(tab_id_, L"Загрузка…", url.ToWString(), true);
  }
}
void SkAiClient::OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) {
  if (!ui_browser_) SkAiNotifyTabState(tab_id_, title.ToWString(), browser_ ? browser_->GetMainFrame()->GetURL().ToWString() : L"", false);
}
void SkAiClient::OnFaviconURLChange(CefRefPtr<CefBrowser>, const std::vector<CefString>&) {}
