#include "skai_client.h"

#include "skai_window.h"
#include "include/cef_parser.h"
#include "include/cef_values.h"

#include <exception>
#include <sstream>
#include <string>

SkAiClient::SkAiClient(HWND host_window, int tab_id, bool ui_browser)
    : host_window_(host_window), tab_id_(tab_id), ui_browser_(ui_browser) {}

bool SkAiClient::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> /*frame*/,
    CefProcessId source,
    CefRefPtr<CefProcessMessage> message) {
  if (!ui_browser_ || source != PID_RENDERER ||
      message->GetName() != "skai.command") {
    return false;
  }

  const auto args = message->GetArgumentList();
  const auto category = args->GetString(0);
  const auto action = args->GetString(1);

  if (category == "window") {
    if (!IsWindow(host_window_)) {
      return true;
    }

    if (action == "minimize") {
      ShowWindow(host_window_, SW_MINIMIZE);
    } else if (action == "toggleMaximize") {
      ShowWindow(host_window_,
                 IsZoomed(host_window_) ? SW_RESTORE : SW_MAXIMIZE);
    } else if (action == "close") {
      PostMessageW(host_window_, WM_CLOSE, 0, 0);
    } else if (action == "drag") {
      ReleaseCapture();
      SendMessageW(host_window_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
    return true;
  }

  if (category == "assistant") {
    SkAiToggleFloatingAssistant();
    return true;
  }

  if (category == "tab") {
    if (action == "new") {
      SkAiCreateTab(args->GetString(2).ToWString(), true);
    } else if (action == "activate") {
      try {
        SkAiActivateTab(std::stoi(args->GetString(2).ToString()));
      } catch (const std::exception&) {
        // Ignore malformed renderer input.
      }
    } else if (action == "close") {
      try {
        SkAiCloseTab(std::stoi(args->GetString(2).ToString()));
      } catch (const std::exception&) {
        // Ignore malformed renderer input.
      }
    }
    return true;
  }

  if (category == "layout") {
    std::wistringstream stream(args->GetString(1).ToWString());
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    wchar_t first = 0;
    wchar_t second = 0;
    wchar_t third = 0;

    if (stream >> x >> first >> y >> second >> width >> third >> height &&
        first == L',' && second == L',' && third == L',') {
      SkAiSetContentBounds(x, y, width, height);
    }
    return true;
  }

  if (!browser) {
    return true;
  }

  if (category == "navigate") {
    std::wstring url = action.ToWString();
    if (url.find(L"://") == std::wstring::npos) {
      url = L"https://" + url;
    }
    SkAiNavigateActive(url);
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

  // A browser may finish its asynchronous creation after the user has
  // requested application shutdown. Register and close it before touching UI.
  if (SkAiOnBrowserCreated()) {
    browser_->GetHost()->CloseBrowser(false);
    return;
  }

  if (ui_browser_ && tab_id_ == -1) {
    const HWND hwnd = browser_->GetHost()->GetWindowHandle();
    host_window_ = hwnd;
    SkAiSetFloatingAssistantHandle(hwnd);
    return;
  }

  if (!ui_browser_) {
    SkAiNotifyTabState(tab_id_, L"Новая вкладка",
                       browser_->GetMainFrame()->GetURL(), false);
    SkAiActivateTab(tab_id_);
  }
}

bool SkAiClient::DoClose(CefRefPtr<CefBrowser> /*browser*/) {
  return false;
}

void SkAiClient::OnBeforeClose(CefRefPtr<CefBrowser> /*browser*/) {
  browser_ = nullptr;

  if (!ui_browser_) {
    SkAiOnTabClosed(tab_id_);
  } else if (tab_id_ == -1) {
    SkAiOnAssistantClosed();
  } else {
    SkAiOnUiClosed();
  }
}

void SkAiClient::OnAddressChange(CefRefPtr<CefBrowser> /*browser*/,
                                 CefRefPtr<CefFrame> frame,
                                 const CefString& url) {
  if (!frame->IsMain()) {
    return;
  }

  if (ui_browser_) {
    auto value = CefValue::Create();
    value->SetString(url);
    const auto json = CefWriteJSON(value, JSON_WRITER_DEFAULT);
    frame->ExecuteJavaScript(
        L"window.skaiNative&&window.skaiNative.addressChanged(" +
            json.ToWString() + L");",
        frame->GetURL(), 0);
  } else {
    SkAiNotifyTabState(tab_id_, L"Загрузка…", url.ToWString(), true);
  }
}

void SkAiClient::OnTitleChange(CefRefPtr<CefBrowser> /*browser*/,
                               const CefString& title) {
  if (ui_browser_ || !browser_) {
    return;
  }

  SkAiNotifyTabState(tab_id_, title.ToWString(),
                     browser_->GetMainFrame()->GetURL().ToWString(), false);
}

void SkAiClient::OnFaviconURLChange(
    CefRefPtr<CefBrowser> /*browser*/,
    const std::vector<CefString>& icon_urls) {
  if (!ui_browser_ && !icon_urls.empty()) {
    SkAiNotifyTabFavicon(tab_id_, icon_urls.front().ToWString());
  }
}
