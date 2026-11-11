#include "skai_client.h"

#include "skai_window.h"
#include "skai_ai.h"
#include "skai_tracking_protection.h"
#include "skai_security.h"
#include "include/cef_request_handler.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_request.h"
#include "include/cef_cookie.h"
#include "include/cef_parser.h"
#include "include/cef_values.h"

#include <cerrno>
#include <climits>
#include <stdint.h>
#include <cwchar>
#include <sstream>
#include <string>
#include <filesystem>
#include <system_error>
#include <shlobj.h>
#include <knownfolders.h>

namespace {
std::wstring SecurityOrigin(const std::wstring& raw_url) {
  CefURLParts parts;
  if (!CefParseURL(raw_url, parts)) return L"";
  const auto scheme = CefString(&parts.scheme).ToWString();
  const auto host = CefString(&parts.host).ToWString();
  if ((scheme != L"http" && scheme != L"https") || host.empty()) return L"";
  return scheme + L"://" + host;
}

class SkAiPrivacyCookieFilter final : public CefCookieAccessFilter {
 public:
  bool CanSendCookie(CefRefPtr<CefBrowser> /*browser*/,
                     CefRefPtr<CefFrame> /*frame*/,
                     CefRefPtr<CefRequest> /*request*/,
                     const CefCookie& /*cookie*/) override {
    return false;  // Never attach real site cookies to HTTP requests.
  }

  bool CanSaveCookie(CefRefPtr<CefBrowser> /*browser*/,
                     CefRefPtr<CefFrame> /*frame*/,
                     CefRefPtr<CefRequest> /*request*/,
                     CefRefPtr<CefResponse> /*response*/,
                     const CefCookie& cookie) override {
    return false;  // Never persist cookies returned by site servers.
  }

 private:
  IMPLEMENT_REFCOUNTING(SkAiPrivacyCookieFilter);
};

class SkAiTrackingResourceHandler final : public CefResourceRequestHandler {
 public:
  explicit SkAiTrackingResourceHandler(bool block_tracking_requests = true)
      : block_tracking_requests_(block_tracking_requests) {}
  CefRefPtr<CefCookieAccessFilter> GetCookieAccessFilter(
      CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/,
      CefRefPtr<CefRequest> /*request*/) override {
    return new SkAiPrivacyCookieFilter();
  }

  ReturnValue OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> /*callback*/) override {
    if (block_tracking_requests_ && request &&
        SkAiShouldBlockTrackingRequest(request->GetURL().ToWString())) {
      SkAiRecordSecurityEvent(
          SkAiSecurityEventType::tracker_blocked, SkAiSecuritySeverity::low,
          SecurityOrigin(request->GetURL().ToWString()),
          L"Заблокирован запрос известного трекера");
      return RV_CANCEL;
    }
    return RV_CONTINUE;
  }

 private:
  bool block_tracking_requests_ = true;
  IMPLEMENT_REFCOUNTING(SkAiTrackingResourceHandler);
};

void RecordNavigationRisk(const std::wstring& from_url,
                            const std::wstring& to_url,
                            bool is_redirect) {
  CefURLParts target_parts;
  if (!CefParseURL(to_url, target_parts)) return;
  const auto target_scheme = CefString(&target_parts.scheme).ToWString();
  const auto target_host = CefString(&target_parts.host).ToWString();
  const auto username = CefString(&target_parts.username).ToWString();
  const auto password = CefString(&target_parts.password).ToWString();
  if ((target_scheme != L"http" && target_scheme != L"https") ||
      target_host.empty()) return;

  std::wstring source_scheme;
  if (is_redirect) {
    CefURLParts source_parts;
    if (CefParseURL(from_url, source_parts)) {
      source_scheme = CefString(&source_parts.scheme).ToWString();
    }
  }
  const auto risk = SkAiClassifyNavigationRisk(
      source_scheme, target_scheme, target_host,
      !username.empty() || !password.empty(), is_redirect);
  if (risk.detected) {
    SkAiRecordSecurityEvent(
        SkAiSecurityEventType::suspicious_navigation, risk.severity,
        SecurityOrigin(to_url), L"Обнаружен признак риска при навигации",
        risk.evidence);
  }
}

bool TryParseDownloadId(const CefString& input, uint32_t& download_id) {
  const std::wstring value = input.ToWString();
  if (value.empty()) return false;
  wchar_t* end = nullptr;
  errno = 0;
  const unsigned long parsed = std::wcstoul(value.c_str(), &end, 10);
  if (errno == ERANGE || end == value.c_str() || *end != L'\0' ||
      parsed > UINT_MAX) {
    return false;
  }
  download_id = static_cast<uint32_t>(parsed);
  return true;
}

bool TryParsePermissionId(const CefString& input, uint64_t& permission_id) {
  const std::wstring value = input.ToWString();
  if (value.empty()) return false;
  wchar_t* end = nullptr;
  errno = 0;
  const unsigned long long parsed = std::wcstoull(value.c_str(), &end, 10);
  if (errno == ERANGE || end == value.c_str() || *end != L'\0') {
    return false;
  }
  permission_id = static_cast<uint64_t>(parsed);
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

CefRefPtr<CefResourceRequestHandler> SkAiCreateTrackingResourceHandler() {
  return new SkAiTrackingResourceHandler();
}

CefRefPtr<CefResourceRequestHandler> SkAiClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> /*request*/, bool is_navigation, bool is_download,
    const CefString& /*request_initiator*/,
    bool& /*disable_default_handling*/) {
  // Downloads are preserved. Main-frame navigations keep working, but still
  // receive the cookie filter; tracker URL cancellation applies to subresources.
  if (is_download) return new SkAiTrackingResourceHandler(false);
  if (is_navigation && frame && frame->IsMain()) {
    return new SkAiTrackingResourceHandler(false);
  }
  return SkAiCreateTrackingResourceHandler();
}

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

  if (category == "pageContext") {
    if (action == "share") {
      SkAiShareCurrentPage(browser);
    }
    return true;
  }

  if (category == "ai") {
    if (action == "status") {
      SkAiAiQueryStatus(browser);
    } else if (action == "install") {
      SkAiAiInstallModel(browser);
    } else if (action == "send") {
      if (args->GetSize() >= 4) {
        SkAiAiSendChat(browser, args->GetString(2).ToWString(),
                       args->GetString(3).ToWString());
      }
    }
    return true;
  }

  if (category == "tab") {
    if (args->GetSize() < 3) {
      return true;
    }
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
    if (args->GetSize() < 3) return true;
    uint32_t download_id = 0;
    if (TryParseDownloadId(args->GetString(2), download_id)) {
      SkAiDownloadAction(download_id, action.ToWString());
    }
    return true;
  }

  if (category == "downloadPath") {
    if (args->GetSize() < 3) return true;
    SkAiOpenDownloadedFile(args->GetString(2).ToWString(),
                           action.ToWString());
    return true;
  }

  if (category == "profilesList") {
    SkAiSendProfileList();
    return true;
  }

  if (category == "profileCreate") {
    if (args->GetSize() < 3) return true;
    SkAiCreateProfile(args->GetString(2).ToWString());
    return true;
  }

  if (category == "profileSwitch") {
    if (args->GetSize() < 3) return true;
    SkAiSwitchProfile(args->GetString(2).ToWString());
    return true;
  }

  if (category == "securityEvents") {
    if (action == "list") SkAiSendSecurityEvents();
    else if (action == "clear") {
      SkAiClearSecurityEvents();
      SkAiSendSecurityEvents();
    }
    return true;
  }

  if (category == "permissionsList") {
    SkAiSendSitePermissionList();
    return true;
  }

  if (category == "permissionRevoke") {
    if (args->GetSize() < 3) return true;
    SkAiRevokeSitePermission(args->GetString(2).ToWString());
    return true;
  }

  if (category == "permission") {
    if (args->GetSize() < 4) return true;
    uint64_t request_id = 0;
    if (TryParsePermissionId(args->GetString(2), request_id)) {
      SkAiResolvePermission(request_id, action == "allow", args->GetBool(3));
    }
    return true;
  }


  if (category == "layout") {
    if (args->GetSize() < 2) return true;
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
    const std::wstring parameter = args->GetSize() >= 3
        ? args->GetString(2).ToWString() : L"";
    SkAiBrowserAction(action.ToWString(), parameter);
    return true;
  }

  return false;
}

void SkAiClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  browser_ = browser;

  // A browser may finish its asynchronous creation after the user has
  // requested application shutdown. Register and close it before touching UI.
  if (SkAiOnBrowserCreated() || closing_) {
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
  closing_ = true;
  if (!browser_ || close_sent_) {
    return;
  }
  close_sent_ = true;
  browser_->GetHost()->CloseBrowser(false);
}

bool SkAiClient::DoClose(CefRefPtr<CefBrowser> /*browser*/) {
  closing_ = true;
  return false;
}

void SkAiClient::OnBeforeClose(CefRefPtr<CefBrowser> /*browser*/) {
  browser_ = nullptr;

  if (!ui_browser_) {
    SkAiCancelPermissionsForTab(tab_id_);
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

void SkAiClient::OnFindResult(
    CefRefPtr<CefBrowser> /*browser*/, int /*identifier*/, int count,
    const CefRect& /*selection_rect*/, int active_match_ordinal,
    bool final_update) {
  if (!ui_browser_) {
    SkAiNotifyFindResult(tab_id_, count, active_match_ordinal, final_update);
  }
}

void SkAiClient::OnLoadingStateChange(
    CefRefPtr<CefBrowser> browser, bool is_loading, bool can_go_back,
    bool can_go_forward) {
  if (!ui_browser_) {
    if (!is_loading) {
      main_frame_redirect_count_ = 0;
      redirect_chain_warning_recorded_ = false;
    }
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

  SkAiRecordSecurityEvent(
      SkAiSecurityEventType::download_started, SkAiSecuritySeverity::info,
      SecurityOrigin(download_item->GetURL().ToWString()),
      L"Начата загрузка файла");
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
  if (download_item->IsComplete()) {
    SkAiRecordSecurityEvent(
        SkAiSecurityEventType::download_completed, SkAiSecuritySeverity::info,
        SecurityOrigin(download_item->GetURL().ToWString()),
        L"Загрузка завершена");
  } else if (download_item->IsCanceled()) {
    SkAiRecordSecurityEvent(
        SkAiSecurityEventType::download_interrupted, SkAiSecuritySeverity::low,
        SecurityOrigin(download_item->GetURL().ToWString()),
        L"Загрузка отменена или прервана");
  }
  SkAiNotifyDownloadState(download_item, callback);
}

bool SkAiClient::OnBeforeBrowse(
    CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request, bool /*user_gesture*/,
    bool is_redirect) {
  if (!frame || !frame->IsMain() || !request ||
      request->GetMethod() != "GET") {
    return false;
  }
  const auto original_url = request->GetURL().ToWString();
  if (!is_redirect) {
    main_frame_redirect_count_ = 0;
    redirect_chain_warning_recorded_ = false;
  } else {
    ++main_frame_redirect_count_;
    const auto chain_risk =
        SkAiClassifyRedirectChainRisk(main_frame_redirect_count_);
    if (chain_risk.detected && !redirect_chain_warning_recorded_) {
      SkAiRecordSecurityEvent(
          SkAiSecurityEventType::suspicious_navigation, chain_risk.severity,
          SecurityOrigin(original_url),
          L"Обнаружена необычно длинная цепочка перенаправлений",
          chain_risk.evidence);
      redirect_chain_warning_recorded_ = true;
    }
  }
  RecordNavigationRisk(frame->GetURL().ToWString(), original_url, is_redirect);
  if (is_redirect) {
    SkAiRecordSecurityEvent(
        SkAiSecurityEventType::navigation_redirect, SkAiSecuritySeverity::info,
        SecurityOrigin(original_url), L"Обнаружен переход по перенаправлению");
  }
  const auto cleaned_url = SkAiStripTrackingParameters(original_url);
  if (cleaned_url != original_url) {
    request->SetURL(cleaned_url);
  }
  return false;
}

bool SkAiClient::OnRequestMediaAccessPermission(
    CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/,
    const CefString& requesting_origin, uint32_t requested_permissions,
    CefRefPtr<CefMediaAccessCallback> callback) {
  if (ui_browser_) {
    return false;
  }

  SkAiRecordSecurityEvent(
      SkAiSecurityEventType::permission_requested, SkAiSecuritySeverity::medium,
      requesting_origin.ToWString(), L"Сайт запросил доступ к устройству",
      L"Медиа-разрешение");
  return SkAiRequestSitePermission(tab_id_, requesting_origin.ToWString(),
                                   requested_permissions, true, nullptr,
                                   callback) != 0;
}

bool SkAiClient::OnShowPermissionPrompt(
    CefRefPtr<CefBrowser> /*browser*/, uint64_t prompt_id,
    const CefString& requesting_origin, uint32_t requested_permissions,
    CefRefPtr<CefPermissionPromptCallback> callback) {
  if (ui_browser_) {
    return false;
  }

  SkAiRecordSecurityEvent(
      SkAiSecurityEventType::permission_requested, SkAiSecuritySeverity::medium,
      requesting_origin.ToWString(), L"Сайт запросил разрешение браузера");
  return SkAiRequestSitePermission(tab_id_, requesting_origin.ToWString(),
                                   requested_permissions, false, callback,
                                   nullptr, prompt_id) != 0;
}

void SkAiClient::OnDismissPermissionPrompt(
    CefRefPtr<CefBrowser> /*browser*/, uint64_t prompt_id,
    cef_permission_request_result_t /*result*/) {
  if (!ui_browser_) {
    SkAiDismissPermission(prompt_id);
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
