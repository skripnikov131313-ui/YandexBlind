#include "skai_client.h"

#include "skai_window.h"
#include "include/cef_parser.h"
#include "include/cef_values.h"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cwchar>
#include <sstream>
#include <string>
#include <filesystem>
#include <system_error>
#include <shlobj.h>
#include <knownfolders.h>

namespace {
bool TryParseDownloadId(const CefString& input, uint32_t& download_id) {
  const std::wstring value = input.ToWString();
  if (value.empty()) return false;
  wchar_t* end = nullptr;
  errno = 0;
  const unsigned long parsed = std::wcstoul(value.c_str(), &end, 10);
  if (errno == ERANGE || end == value.c_str() || *end != L'\\0' ||
      parsed > UINT_MAX) {
    return false;
  }
  download_id = static_cast<uint32_t>(parsed);
  return true;
}

bool TryParseTabId(const CefString& input, int& tab_id) {
  const std::wstring value = input.ToWString();
  if (value.empty()) {
    return false;
  }

  wchar_t* end = nullptr;
  errno = 0;
  const long parsed = std::wcstol(value.c_str(), &end, 10);
  if (errno == ERANGE || end == value.c_str() || *end != L'\0' ||
      parsed < 0 || parsed > INT_MAX) {
    return false;
  }

  tab_id = static_cast<int>(parsed);
  return true;
}
}  // namespace

namespace {
std::filesystem::path GetSkAiDownloadsDirectory() {
  PWSTR downloads = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, KF_FLAG_CREATE,
                                    nullptr, &downloads)) && downloads) {
    std::filesystem::path directory =
        std::filesystem::path(downloads) / L"SkAi";
    CoTaskMemFree(downloads);

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (!error) {
      return directory;
    }
  }
  return {};
}

std::wstring SafeDownloadName(std::wstring name) {
  for (auto& ch : name) {
    if (ch < 32 || ch == L'<' || ch == L'>' || ch == L':' ||
        ch == L'"' || ch == L'/' || ch == L'\\' ||
        ch == L'|' || ch == L'?' || ch == L'*') {
      ch = L'_';
    }
  }
  while (!name.empty() && (name.back() == L'.' || name.back() == L' ')) {
    name.pop_back();
  }
  if (name.empty() || name == L"." || name == L"..") {
    return L"download";
  }
  return name;
}

std::filesystem::path MakeUniqueDownloadPath(
    const std::filesystem::path& directory, const std::wstring& name) {
  const auto safe_name = SafeDownloadName(name);
  auto candidate = directory / safe_name;
  std::error_code error;
  if (!std::filesystem::exists(candidate, error) || error) {
    return candidate;
  }

  const std::filesystem::path file_name(safe_name);
  const auto stem = file_name.stem().wstring();
  const auto extension = file_name.extension().wstring();
  for (unsigned int suffix = 2; suffix < 10000; ++suffix) {
    candidate = directory /
        (stem + L" (" + std::to_wstring(suffix) + L")" + extension);
    error.clear();
    if (!std::filesystem::exists(candidate, error) || error) {
      return candidate;
    }
  }
  return directory /
      (stem + L" (" + std::to_wstring(GetTickCount64()) + L")" + extension);
}
}  // namespace

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
      int tab_id = 0;
      if (TryParseTabId(args->GetString(2), tab_id)) {
        SkAiActivateTab(tab_id);
      }
    } else if (action == "close") {
      int tab_id = 0;
      if (TryParseTabId(args->GetString(2), tab_id)) {
        SkAiCloseTab(tab_id);
      }
    }
    return true;
  }

  if (category == "download") {
    uint32_t download_id = 0;
    if (TryParseDownloadId(args->GetString(2), download_id)) {
      SkAiDownloadAction(download_id, action.ToWString());
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
    RequestClose();
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

void SkAiClient::RequestClose() {
  if (closing_ || !browser_) {
    return;
  }
  closing_ = true;
  browser_->GetHost()->CloseBrowser(false);
}

bool SkAiClient::DoClose(CefRefPtr<CefBrowser> /*browser*/) {
  closing_ = true;
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

void SkAiClient::OnLoadingStateChange(
    CefRefPtr<CefBrowser> browser, bool is_loading, bool can_go_back,
    bool can_go_forward) {
  if (!ui_browser_) {
    if (!is_loading && browser && browser->GetMainFrame()) {
      SkAiNotifyTabState(tab_id_, L"",
                         browser->GetMainFrame()->GetURL().ToWString(), false);
    }
    SkAiNotifyNavigationState(tab_id_, can_go_back, can_go_forward, is_loading);
  }
}

bool SkAiClient::OnBeforeDownload(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefDownloadItem> download_item,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  if (!callback || !download_item || !download_item->IsValid()) {
    return false;
  }

  const auto directory = GetSkAiDownloadsDirectory();
  if (directory.empty()) {
    // Preserve user choice if the standard Downloads folder is unavailable.
    callback->Continue(SafeDownloadName(suggested_name.ToWString()), true);
    return true;
  }

  const auto path = MakeUniqueDownloadPath(
      directory, suggested_name.ToWString());
  callback->Continue(path.wstring(), false);
  return true;
}

void SkAiClient::OnDownloadUpdated(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefDownloadItem> download_item,
    CefRefPtr<CefDownloadItemCallback> callback) {
  if (!download_item || !download_item->IsValid()) {
    return;
  }
  SkAiNotifyDownloadState(download_item, callback);
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
