#include "skai_window.h"

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/cef_values.h"
#include "include/cef_parser.h"
#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_string_visitor.h"
#include "include/cef_task.h"
#include "skai_client.h"
#include "skai_ai.h"

#include <windowsx.h>
#include <algorithm>
#include <filesystem>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>

namespace {
constexpr wchar_t kClass[] = L"SkAi.NativeBrowserWindow";
constexpr UINT kLayout = WM_APP + 20;
constexpr UINT kFinishClose = WM_APP + 21;

HWND g_window = nullptr;
HWND g_assistant_window = nullptr;
CefRefPtr<SkAiClient> g_ui_client;
CefRefPtr<SkAiClient> g_assistant_client;

struct Tab {
  int id = 0;
  CefRefPtr<SkAiClient> client;
  CefRefPtr<CefRequestContext> request_context;
  std::wstring profile = L"default";
  bool closing = false;
  bool active = false;
  bool can_go_back = false;
  bool can_go_forward = false;
  bool is_loading = false;
  std::wstring title = L"Новая вкладка";
  std::wstring url;
};
std::map<int, Tab> g_tabs;
std::map<std::wstring, CefRefPtr<CefRequestContext>> g_profile_contexts;
std::wstring g_active_profile = L"default";

int g_next_tab_id = 1;
int g_active_tab_id = 0;
RECT g_content_bounds{0, 100, 800, 600};

bool g_close_requested = false;
bool g_finish_close_posted = false;
int g_pending_browser_creates = 0;
int g_pending_browser_closes = 0;

struct DownloadRecord {
  std::wstring path;
  CefRefPtr<CefDownloadItemCallback> callback;
};
std::map<std::uint32_t, DownloadRecord> g_downloads;

struct PendingPermission {
  int tab_id = 0;
  std::wstring origin;
  std::wstring description;
  std::uint32_t requested_permissions = 0;
  bool media_request = false;
  std::uint64_t cef_prompt_id = 0;
  CefRefPtr<CefPermissionPromptCallback> prompt_callback;
  CefRefPtr<CefMediaAccessCallback> media_callback;
  std::wstring permission_key;
};
std::map<std::uint64_t, PendingPermission> g_pending_permissions;
std::map<std::uint64_t, std::uint64_t> g_cef_prompt_to_request;
std::map<std::wstring, bool> g_site_permission_decisions;
bool g_permission_store_loaded = false;
std::uint64_t g_next_permission_id = (1ULL << 52);

std::wstring UiFile(const wchar_t* file) {
  wchar_t exe[MAX_PATH]{};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  const auto path = std::filesystem::path(exe).parent_path() / L"ui" / file;
  return L"file:///" + path.generic_wstring();
}

constexpr std::size_t kMaxSharedPageCharacters = 6000;

void ExecutePageContextNotification(CefRefPtr<CefBrowser> browser,
                                   bool ok,
                                   const std::wstring& title,
                                   const std::wstring& url,
                                   const std::wstring& text,
                                   const std::wstring& error) {
  if (!browser) return;
  const auto frame = browser->GetMainFrame();
  if (!frame) return;

  const auto dictionary = CefDictionaryValue::Create();
  dictionary->SetBool("ok", ok);
  dictionary->SetString("title", title);
  dictionary->SetString("url", url);
  dictionary->SetString("text", text);
  dictionary->SetString("error", error);

  const auto value = CefValue::Create();
  value->SetDictionary(dictionary);
  const std::wstring json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToWString();
  if (json.empty()) return;

  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.pageContextReceived&&"
      L"window.skaiNative.pageContextReceived(" + json + L");",
      frame->GetURL(), 0);
}

class PageContextNotificationTask final : public CefTask {
 public:
  PageContextNotificationTask(CefRefPtr<CefBrowser> browser,
                              bool ok,
                              std::wstring title,
                              std::wstring url,
                              std::wstring text,
                              std::wstring error)
      : browser_(browser),
        ok_(ok),
        title_(std::move(title)),
        url_(std::move(url)),
        text_(std::move(text)),
        error_(std::move(error)) {}

  void Execute() override {
    ExecutePageContextNotification(browser_, ok_, title_, url_, text_, error_);
  }

 private:
  CefRefPtr<CefBrowser> browser_;
  bool ok_;
  std::wstring title_;
  std::wstring url_;
  std::wstring text_;
  std::wstring error_;
  IMPLEMENT_REFCOUNTING(PageContextNotificationTask);
};

void NotifyPageContext(CefRefPtr<CefBrowser> browser,
                       bool ok,
                       const std::wstring& title,
                       const std::wstring& url,
                       const std::wstring& text,
                       const std::wstring& error) {
  if (!browser) return;
  CefPostTask(TID_UI, new PageContextNotificationTask(
      browser, ok, title, url, text, error));
}

class PageTextSnapshotTask final : public CefTask {
 public:
  PageTextSnapshotTask(CefRefPtr<CefBrowser> ui_browser,
                       CefRefPtr<CefFrame> source_frame,
                       int source_tab_id,
                       std::wstring profile,
                       std::wstring title,
                       std::wstring url,
                       std::wstring text,
                       std::wstring error)
      : ui_browser_(ui_browser),
        source_frame_(source_frame),
        source_tab_id_(source_tab_id),
        profile_(std::move(profile)),
        title_(std::move(title)),
        url_(std::move(url)),
        text_(std::move(text)),
        error_(std::move(error)) {}

  void Execute() override {
    const std::wstring current_url =
        source_frame_ ? source_frame_->GetURL().ToWString() : L"";
    if (source_tab_id_ != g_active_tab_id ||
        profile_ != g_active_profile || current_url != url_) {
      ExecutePageContextNotification(
          ui_browser_, false, title_, url_, L"",
          L"Профиль или страница изменились во время чтения. Повторите передачу.");
      return;
    }
    const bool ok = error_.empty() && !text_.empty();
    ExecutePageContextNotification(
        ui_browser_, ok, title_, url_, ok ? text_ : L"",
        ok ? L"" : (error_.empty()
                         ? L"На странице не найден доступный текст."
                         : error_));
  }

 private:
  CefRefPtr<CefBrowser> ui_browser_;
  CefRefPtr<CefFrame> source_frame_;
  int source_tab_id_;
  std::wstring profile_;
  std::wstring title_;
  std::wstring url_;
  std::wstring text_;
  std::wstring error_;
  IMPLEMENT_REFCOUNTING(PageTextSnapshotTask);
};

class PageTextVisitor final : public CefStringVisitor {
 public:
  PageTextVisitor(CefRefPtr<CefBrowser> ui_browser,
                  CefRefPtr<CefFrame> source_frame,
                  int source_tab_id,
                  std::wstring title,
                  std::wstring url,
                  std::wstring profile)
      : ui_browser_(ui_browser),
        source_frame_(source_frame),
        source_tab_id_(source_tab_id),
        title_(std::move(title)),
        url_(std::move(url)),
        profile_(std::move(profile)) {}

  void Visit(const CefString& value) override {
    std::wstring text = value.ToWString();
    if (text.size() > kMaxSharedPageCharacters) {
      text.resize(kMaxSharedPageCharacters);
      if (!text.empty() && text.back() >= 0xD800 && text.back() <= 0xDBFF) {
        text.pop_back();
      }
    }
    const bool has_text =
        text.find_first_not_of(L" \t\r\n") != std::wstring::npos;
    CefPostTask(TID_UI, new PageTextSnapshotTask(
        ui_browser_, source_frame_, source_tab_id_, profile_, title_, url_,
        has_text ? std::move(text) : L"",
        has_text ? L"" : L"На странице не найден доступный текст."));
  }

 private:
  CefRefPtr<CefBrowser> ui_browser_;
  CefRefPtr<CefFrame> source_frame_;
  int source_tab_id_;
  std::wstring title_;
  std::wstring url_;
  std::wstring profile_;
  IMPLEMENT_REFCOUNTING(PageTextVisitor);
};

std::filesystem::path GetSkAiDataRoot() {
  PWSTR local = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                 nullptr, &local)) || !local) {
    return {};
  }
  const auto root = std::filesystem::path(local) / L"SkAi";
  CoTaskMemFree(local);
  return root;
}

std::wstring NormalizeProfileName(const std::wstring& raw_name) {
  if (raw_name.empty() || raw_name.size() > 24) return {};
  std::wstring name;
  name.reserve(raw_name.size());
  for (wchar_t ch : raw_name) {
    if (ch >= L'A' && ch <= L'Z') {
      name.push_back(static_cast<wchar_t>(ch - L'A' + L'a'));
    } else if ((ch >= L'a' && ch <= L'z') ||
               (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_') {
      name.push_back(ch);
    } else {
      return {};
    }
  }
  if (name.empty() || name == L"." || name == L"..") return {};
  return name;
}

std::filesystem::path GetProfileDirectory(const std::wstring& name) {
  const auto root = GetSkAiDataRoot();
  if (root.empty()) return {};
  return root / L"Profiles" / name;
}

void ConfigureProfilePrivacyPreferences(
    CefRefPtr<CefRequestContext> context) {
  if (!context) return;

  // Chromium CookieControlsMode: 1 = block third-party cookies.
  // First-party cookies remain available for normal site sign-in sessions.
  const CefString preference_name = "profile.cookie_controls_mode";
  if (!context->HasPreference(preference_name) ||
      !context->CanSetPreference(preference_name)) {
    OutputDebugStringW(
        L"Sk/Ai: third-party cookie preference is not available in this CEF context.\n");
    return;
  }

  auto value = CefValue::Create();
  if (!value) return;
  value->SetInt(1);
  CefString error;
  if (!context->SetPreference(preference_name, value, error)) {
    const std::wstring message =
        L"Sk/Ai: unable to set third-party cookie protection: " +
        error.ToWString() + L"\n";
    OutputDebugStringW(message.c_str());
  }
}

CefRefPtr<CefRequestContext> GetOrCreateProfileContext(
    const std::wstring& raw_name, bool create_if_missing) {
  const auto name = NormalizeProfileName(raw_name);
  if (name.empty()) return nullptr;

  const auto existing = g_profile_contexts.find(name);
  if (existing != g_profile_contexts.end()) return existing->second;

  const auto directory = GetProfileDirectory(name);
  if (directory.empty()) return nullptr;

  std::error_code error;
  if (!std::filesystem::exists(directory, error) && !create_if_missing) {
    return nullptr;
  }
  error.clear();
  std::filesystem::create_directories(directory / L"Cache", error);
  if (error) return nullptr;

  CefRequestContextSettings context_settings;
  CefString(&context_settings.cache_path) = (directory / L"Cache").wstring();
  context_settings.persist_session_cookies = true;
  auto context = CefRequestContext::CreateContext(context_settings, nullptr);
  if (context) {
    ConfigureProfilePrivacyPreferences(context);
    g_profile_contexts.emplace(name, context);
  }
  return context;
}

void NotifyProfileError(const std::wstring& message) {
  if (!g_ui_client || !g_ui_client->GetBrowser()) return;
  auto value = CefValue::Create();
  value->SetString(message);
  const auto json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToWString();
  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.profileError&&window.skaiNative.profileError(" +
          json + L");",
      frame->GetURL(), 0);
}

void NotifyProfileChanged() {
  if (!g_ui_client || !g_ui_client->GetBrowser()) return;
  auto value = CefValue::Create();
  value->SetString(g_active_profile);
  const auto json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToWString();
  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.profileChanged&&window.skaiNative.profileChanged(" +
          json + L");",
      frame->GetURL(), 0);
}

std::filesystem::path GetManagedDownloadsDirectory() {
  PWSTR downloads = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Downloads, KF_FLAG_CREATE,
                                 nullptr, &downloads)) || !downloads) {
    return {};
  }

  const auto directory = std::filesystem::path(downloads) / L"SkAi";
  CoTaskMemFree(downloads);
  return directory;
}

bool ResolveManagedDownloadPath(const std::wstring& raw_path,
                                std::filesystem::path& resolved_path) {
  if (raw_path.empty()) {
    return false;
  }

  std::error_code error;
  const auto root = std::filesystem::weakly_canonical(
      GetManagedDownloadsDirectory(), error);
  if (error || root.empty()) {
    return false;
  }

  const auto candidate =
      std::filesystem::weakly_canonical(std::filesystem::path(raw_path), error);
  if (error || candidate.empty()) {
    return false;
  }

  const std::wstring root_text = root.native();
  const std::wstring candidate_text = candidate.native();
  if (candidate_text.size() <= root_text.size() ||
      CompareStringOrdinal(candidate_text.c_str(),
                           static_cast<int>(root_text.size()),
                           root_text.c_str(),
                           static_cast<int>(root_text.size()), TRUE) !=
          CSTR_EQUAL) {
    return false;
  }

  const wchar_t boundary = candidate_text[root_text.size()];
  if (boundary != L'\\' && boundary != L'/') {
    return false;
  }

  error.clear();
  if (!std::filesystem::is_regular_file(candidate, error) || error) {
    return false;
  }

  resolved_path = candidate;
  return true;
}

std::filesystem::path GetPermissionStorePath() {
  PWSTR local = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                 nullptr, &local)) || !local) {
    return {};
  }

  const auto profile = GetProfileDirectory(g_active_profile);
  CoTaskMemFree(local);
  if (profile.empty()) return {};
  std::error_code error;
  std::filesystem::create_directories(profile, error);
  if (error) return {};
  return profile / L"permissions.json";
}

std::wstring MakePermissionDecisionKey(const std::wstring& origin,
                                       std::uint32_t requested_permissions,
                                       bool media_request) {
  return origin + (media_request ? L"|media|" : L"|site|") +
         std::to_wstring(requested_permissions);
}

void LoadSitePermissionDecisions() {
  if (g_permission_store_loaded) {
    return;
  }
  g_permission_store_loaded = true;

  const auto path = GetPermissionStorePath();
  if (path.empty()) {
    return;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return;
  }
  const std::string json((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  if (json.empty()) {
    return;
  }

  const auto value = CefParseJSON(json.data(), json.size(), JSON_PARSER_RFC);
  if (!value || value->GetType() != VTYPE_DICTIONARY) {
    return;
  }

  const auto dictionary = value->GetDictionary();
  if (!dictionary || !dictionary->IsValid()) {
    return;
  }

  CefDictionaryValue::KeyList keys;
  if (!dictionary->GetKeys(keys)) {
    return;
  }
  for (const auto& key : keys) {
    if (dictionary->GetType(key) == VTYPE_BOOL) {
      g_site_permission_decisions[key.ToWString()] =
          dictionary->GetBool(key);
    }
  }
}

bool SaveSitePermissionDecisions() {
  const auto path = GetPermissionStorePath();
  if (path.empty()) {
    return false;
  }

  auto dictionary = CefDictionaryValue::Create();
  for (const auto& [key, allow] : g_site_permission_decisions) {
    dictionary->SetBool(key, allow);
  }

  auto value = CefValue::Create();
  value->SetDictionary(dictionary);
  const auto json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();
  auto temporary_path = path;
  temporary_path += L".tmp";
  {
    std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);
    if (!file) {
      return false;
    }
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    file.flush();
    if (!file.good()) {
      file.close();
      std::error_code cleanup_error;
      std::filesystem::remove(temporary_path, cleanup_error);
      return false;
    }
  }

  if (!MoveFileExW(temporary_path.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::error_code cleanup_error;
    std::filesystem::remove(temporary_path, cleanup_error);
    return false;
  }
  return true;
}

std::wstring DescribePermissionRequest(std::uint32_t permissions,
                                            bool media_request) {
  std::vector<std::wstring> labels;
  const auto add = [&labels, permissions](std::uint32_t bit, const wchar_t* label) {
    if ((permissions & bit) != 0) labels.emplace_back(label);
  };

  if (media_request) {
    add(CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE, L"микрофон");
    add(CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE, L"камера");
    add(CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE, L"звук рабочего стола");
    add(CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE, L"запись экрана");
  } else {
    add(CEF_PERMISSION_TYPE_AR_SESSION, L"AR-сессия");
    add(CEF_PERMISSION_TYPE_CAMERA_PAN_TILT_ZOOM, L"управление камерой");
    add(CEF_PERMISSION_TYPE_CAPTURED_SURFACE_CONTROL, L"управление захваченным экраном");
    add(CEF_PERMISSION_TYPE_CLIPBOARD, L"буфер обмена");
    add(CEF_PERMISSION_TYPE_TOP_LEVEL_STORAGE_ACCESS, L"доступ к хранилищу");
    add(CEF_PERMISSION_TYPE_DISK_QUOTA, L"квота диска");
    add(CEF_PERMISSION_TYPE_GEOLOCATION, L"геолокация");
    add(CEF_PERMISSION_TYPE_CAMERA_STREAM, L"камера");
    add(CEF_PERMISSION_TYPE_MIC_STREAM, L"микрофон");
    add(CEF_PERMISSION_TYPE_MIDI_SYSEX, L"MIDI-устройства");
    add(CEF_PERMISSION_TYPE_MULTIPLE_DOWNLOADS, L"несколько загрузок");
    add(CEF_PERMISSION_TYPE_LOCAL_FONTS, L"системные шрифты");
    add(CEF_PERMISSION_TYPE_STORAGE_ACCESS, L"доступ к хранилищу сайта");
    add(CEF_PERMISSION_TYPE_FILE_SYSTEM_ACCESS, L"файловая система");
    add(CEF_PERMISSION_TYPE_WINDOW_MANAGEMENT, L"управление окнами");
    add(CEF_PERMISSION_TYPE_IDLE_DETECTION, L"состояние активности");
    add(CEF_PERMISSION_TYPE_LOCAL_NETWORK, L"локальная сеть");
    add(CEF_PERMISSION_TYPE_LOOPBACK_NETWORK, L"локальные подключения");
    add(CEF_PERMISSION_TYPE_PROTECTED_MEDIA_IDENTIFIER, L"защищённые медиа");
    add(CEF_PERMISSION_TYPE_REGISTER_PROTOCOL_HANDLER, L"регистрация обработчика протокола");
    add(CEF_PERMISSION_TYPE_VR_SESSION, L"VR-сессия");
    add(CEF_PERMISSION_TYPE_WEB_APP_INSTALLATION, L"установка веб-приложения");
    add(CEF_PERMISSION_TYPE_SENSORS, L"датчики устройства");
    add(CEF_PERMISSION_TYPE_LOCAL_NETWORK_ACCESS_DEPRECATED, L"доступ к локальной сети");
  }

  if (labels.empty()) {
    return L"доступ к функции сайта";
  }

  std::wstring result;
  for (std::size_t i = 0; i < labels.size(); ++i) {
    if (i != 0) result += L", ";
    result += labels[i];
  }
  return result;
}

void NotifyPermissionRequested(std::uint64_t request_id,
                               const PendingPermission& request) {
  if (!g_ui_client || !g_ui_client->GetBrowser()) {
    return;
  }

  auto dictionary = CefDictionaryValue::Create();
  dictionary->SetString("id", std::to_wstring(request_id));
  dictionary->SetString("origin", request.origin);
  dictionary->SetString("description", request.description);
  dictionary->SetBool("media", request.media_request);

  auto value = CefValue::Create();
  value->SetDictionary(dictionary);
  const auto json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToWString();

  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.permissionRequested&&window.skaiNative.permissionRequested(" +
          json + L");",
      frame->GetURL(), 0);
}

void NotifyPermissionDismissed(std::uint64_t request_id) {
  if (!g_ui_client || !g_ui_client->GetBrowser()) {
    return;
  }
  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.permissionDismissed&&window.skaiNative.permissionDismissed(" +
          std::to_wstring(request_id) + L");",
      frame->GetURL(), 0);
}

void MaybeFinishClose() {
  if (!g_close_requested || g_finish_close_posted ||
      g_pending_browser_creates != 0 || g_pending_browser_closes != 0) {
    return;
  }
  g_finish_close_posted = true;
  if (IsWindow(g_window)) {
    PostMessageW(g_window, kFinishClose, 0, 0);
  }
}

void BrowserClosedDuringShutdown() {
  if (!g_close_requested) {
    return;
  }
  if (g_pending_browser_closes > 0) {
    --g_pending_browser_closes;
  }
  MaybeFinishClose();
}

void LayoutChildren() {
  if (!IsWindow(g_window)) {
    return;
  }

  const int width = g_content_bounds.right - g_content_bounds.left;
  const int height = g_content_bounds.bottom - g_content_bounds.top;

  for (auto& [id, tab] : g_tabs) {
    const auto browser = tab.client ? tab.client->GetBrowser() : nullptr;
    if (!browser) {
      continue;
    }

    const HWND child = browser->GetHost()->GetWindowHandle();
    if (!IsWindow(child)) {
      continue;
    }

    if (tab.active && !tab.closing && tab.profile == g_active_profile) {
      SetWindowPos(child, HWND_TOP, g_content_bounds.left, g_content_bounds.top,
                   width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
      browser->GetHost()->WasHidden(false);
    } else {
      ShowWindow(child, SW_HIDE);
      browser->GetHost()->WasHidden(true);
    }
  }
}

void SendTabSnapshot() {
  if (!g_ui_client || !g_ui_client->GetBrowser()) {
    return;
  }

  const auto json_string = [](const std::wstring& value) {
    auto cef_value = CefValue::Create();
    cef_value->SetString(value);
    return CefWriteJSON(cef_value, JSON_WRITER_DEFAULT).ToWString();
  };

  std::wstring json = L"[";
  bool first = true;
  for (auto it = g_tabs.begin(); it != g_tabs.end(); ++it) {
    const auto& tab = it->second;
    if (tab.profile != g_active_profile || tab.closing) continue;
    if (!first) json += L",";
    first = false;
    json += L"{\"id\":" + std::to_wstring(tab.id) +
            L",\"active\":" + (tab.active ? L"true" : L"false") +
            L",\"loading\":" + (tab.is_loading ? L"true" : L"false") +
            L",\"title\":" + json_string(tab.title) +
            L",\"url\":" + json_string(tab.url) + L"}";
  }
  json += L"]";

  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.syncTabs(" + json + L");",
      frame->GetURL(), 0);
}

void BeginApplicationClose(HWND window) {
  SkAiAiShutdown();
  SkAiCancelAllPermissions();
  if (g_close_requested) {
    return;
  }

  g_close_requested = true;
  struct PendingClose {
    CefRefPtr<CefBrowser> browser;
    CefRefPtr<SkAiClient> client;
  };
  std::vector<PendingClose> browsers;
  browsers.reserve(g_tabs.size() + 2);

  for (const auto& [id, tab] : g_tabs) {
    const auto browser = tab.client ? tab.client->GetBrowser() : nullptr;
    if (browser) {
      browsers.push_back({browser, tab.client});
    }
  }

  if (g_ui_client && g_ui_client->GetBrowser()) {
    browsers.push_back({g_ui_client->GetBrowser(), g_ui_client});
  }
  if (g_assistant_client && g_assistant_client->GetBrowser()) {
    browsers.push_back({g_assistant_client->GetBrowser(), g_assistant_client});
  }

  // Count every live browser, including one already closing because the
  // user clicked its own close button. Never issue CloseBrowser twice.
  g_pending_browser_closes = static_cast<int>(browsers.size());
  for (const auto& pending : browsers) {
    if (pending.client && !pending.client->is_closing()) {
      pending.client->RequestClose();
    }
  }

  MaybeFinishClose();
  if (!IsWindow(window)) {
    return;
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                             LPARAM lparam) {
  switch (message) {
    case WM_SIZE:
    case kLayout:
      LayoutChildren();
      return 0;

    case WM_NCHITTEST: {
      const LRESULT hit = DefWindowProcW(hwnd, message, wparam, lparam);
      if (hit != HTCLIENT) {
        return hit;
      }

      POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      ScreenToClient(hwnd, &point);

      RECT client{};
      GetClientRect(hwnd, &client);
      constexpr int edge = 8;
      const bool left = point.x < edge;
      const bool right = point.x >= client.right - edge;
      const bool top = point.y < edge;
      const bool bottom = point.y >= client.bottom - edge;

      if (top && left) return HTTOPLEFT;
      if (top && right) return HTTOPRIGHT;
      if (bottom && left) return HTBOTTOMLEFT;
      if (bottom && right) return HTBOTTOMRIGHT;
      if (left) return HTLEFT;
      if (right) return HTRIGHT;
      if (top) return HTTOP;
      if (bottom) return HTBOTTOM;
      return HTCLIENT;
    }

    case WM_ERASEBKGND:
      return 1;

    case WM_CLOSE:
      BeginApplicationClose(hwnd);
      return 0;

    case kFinishClose:
      if (g_close_requested && g_pending_browser_creates == 0 &&
          g_pending_browser_closes == 0) {
        DestroyWindow(hwnd);
      }
      return 0;

    case WM_DESTROY:
      if (hwnd == g_window) {
        g_tabs.clear();
        g_downloads.clear();
        g_pending_permissions.clear();
        g_cef_prompt_to_request.clear();
        g_ui_client = nullptr;
        g_assistant_client = nullptr;
        g_window = nullptr;
        PostQuitMessage(0);
      }
      return 0;

    default:
      return DefWindowProcW(hwnd, message, wparam, lparam);
  }
}
}  // namespace

void SkAiSendProfileList() {
  if (g_close_requested) return;

  const auto profiles_root = GetSkAiDataRoot() / L"Profiles";
  std::error_code error;
  std::filesystem::create_directories(
      profiles_root / L"default" / L"Cache", error);
  if (error) {
    NotifyProfileError(L"Не удалось открыть каталог профилей Sk/Ai.");
    return;
  }

  std::vector<std::wstring> names{L"default"};
  error.clear();
  for (const auto& entry : std::filesystem::directory_iterator(profiles_root, error)) {
    if (error || !entry.is_directory()) continue;
    const auto name = NormalizeProfileName(entry.path().filename().wstring());
    if (!name.empty() &&
        std::find(names.begin(), names.end(), name) == names.end()) {
      names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end());

  auto list = CefListValue::Create();
  std::size_t index = 0;
  for (const auto& name : names) {
    auto item = CefDictionaryValue::Create();
    item->SetString("name", name);
    item->SetBool("active", name == g_active_profile);
    list->SetDictionary(index++, item);
  }

  auto value = CefValue::Create();
  value->SetList(list);
  const auto json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToWString();
  if (!g_ui_client || !g_ui_client->GetBrowser()) return;
  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.profilesUpdated&&window.skaiNative.profilesUpdated(" +
          json + L");",
      frame->GetURL(), 0);
}

void SkAiCreateProfile(const std::wstring& raw_name) {
  if (g_close_requested) return;
  const auto name = NormalizeProfileName(raw_name);
  if (name.empty() || name == L"default") {
    NotifyProfileError(L"Имя профиля: латинские буквы, цифры, дефис или подчёркивание. Имя default зарезервировано.");
    return;
  }

  const auto directory = GetProfileDirectory(name);
  std::error_code error;
  if (directory.empty() || std::filesystem::exists(directory, error) || error) {
    NotifyProfileError(L"Профиль с таким именем уже существует или каталог недоступен.");
    return;
  }

  std::filesystem::create_directories(directory / L"Cache", error);
  if (error || !GetOrCreateProfileContext(name, false)) {
    NotifyProfileError(L"Не удалось создать изолированный профиль.");
    return;
  }
  SkAiSendProfileList();
}

void SkAiSwitchProfile(const std::wstring& raw_name) {
  if (g_close_requested) return;
  const auto name = NormalizeProfileName(raw_name);
  if (name.empty()) {
    NotifyProfileError(L"Некорректное имя профиля.");
    return;
  }

  const auto request_context = GetOrCreateProfileContext(name, false);
  if (!request_context) {
    NotifyProfileError(L"Профиль не найден или его данные недоступны.");
    SkAiSendProfileList();
    return;
  }

  if (name == g_active_profile) {
    NotifyProfileChanged();
    SkAiSendProfileList();
    return;
  }

  // Close the detached assistant before changing profiles so its UI and
  // conversation cannot remain attached to the previous profile.
  if (g_assistant_client && !g_assistant_client->is_closing()) {
    g_assistant_client->RequestClose();
  }

  // Pending permission callbacks belong to the previous profile and must
  // never be stored using the newly selected profile path.
  SkAiCancelAllPermissions();
  g_site_permission_decisions.clear();
  g_permission_store_loaded = false;
  g_active_profile = name;
  g_active_tab_id = 0;

  for (auto& [id, tab] : g_tabs) {
    tab.active = false;
    tab.closing = true;
    if (tab.client) tab.client->RequestClose();
  }

  LayoutChildren();
  SendTabSnapshot();
  NotifyProfileChanged();
  SkAiSendProfileList();
}

bool SkAiOnBrowserCreated() {
  if (g_pending_browser_creates > 0) {
    --g_pending_browser_creates;
  }

  if (!g_close_requested) {
    return false;
  }

  ++g_pending_browser_closes;
  MaybeFinishClose();
  return true;
}

void SkAiOnUiClosed() {
  g_ui_client = nullptr;
  BrowserClosedDuringShutdown();
}

void SkAiOnAssistantClosed() {
  g_assistant_window = nullptr;
  g_assistant_client = nullptr;

  if (!g_close_requested && g_ui_client && g_ui_client->GetBrowser()) {
    const auto ui_browser = g_ui_client->GetBrowser();
    const auto frame = ui_browser->GetMainFrame();
    frame->ExecuteJavaScript(
        L"window.skaiNative&&window.skaiNative.assistantClosed&&window.skaiNative.assistantClosed();",
        frame->GetURL(), 0);
  }

  BrowserClosedDuringShutdown();
}

void SkAiCreateTab(const std::wstring& url, bool activate) {
  if (!g_window || g_close_requested) {
    return;
  }

  const auto request_context = GetOrCreateProfileContext(g_active_profile, true);
  if (!request_context) {
    NotifyProfileError(L"Не удалось открыть профиль браузера.");
    return;
  }

  const int id = g_next_tab_id++;
  Tab tab;
  tab.id = id;
  tab.profile = g_active_profile;
  tab.request_context = request_context;
  tab.client = new SkAiClient(g_window, id, false);
  tab.active = activate;
  tab.url = url;

  if (activate) {
    for (auto& [existing_id, existing_tab] : g_tabs) {
      existing_tab.active = false;
    }
    g_active_tab_id = id;
  }

  g_tabs.emplace(id, tab);

  CefWindowInfo window_info;
  window_info.SetAsChild(
      g_window,
      CefRect(g_content_bounds.left, g_content_bounds.top,
              g_content_bounds.right - g_content_bounds.left,
              g_content_bounds.bottom - g_content_bounds.top));
  CefBrowserSettings browser_settings;

  ++g_pending_browser_creates;
  const bool started = CefBrowserHost::CreateBrowser(
      window_info, tab.client, url, browser_settings, nullptr,
      tab.request_context);
  if (!started) {
    --g_pending_browser_creates;
    g_tabs.erase(id);
    if (g_active_tab_id == id) {
      g_active_tab_id = 0;
    }
    SendTabSnapshot();
    MaybeFinishClose();
    return;
  }

  SendTabSnapshot();
}

void SkAiActivateTab(int tab_id) {
  if (g_close_requested) {
    return;
  }
  if (tab_id != 0) {
    const auto target = g_tabs.find(tab_id);
    if (target == g_tabs.end() || target->second.closing ||
        target->second.profile != g_active_profile) {
      return;
    }
  }

  for (auto& [id, tab] : g_tabs) {
    tab.active = (id == tab_id && !tab.closing &&
                  tab.profile == g_active_profile);
  }
  g_active_tab_id = tab_id;
  LayoutChildren();
  SendTabSnapshot();

  const auto active = g_tabs.find(tab_id);
  if (active != g_tabs.end()) {
    SkAiNotifyNavigationState(tab_id, active->second.can_go_back,
                              active->second.can_go_forward,
                              active->second.is_loading);
  } else {
    SkAiNotifyNavigationState(0, false, false, false);
  }
}

void SkAiOnTabClosed(int tab_id) {
  g_tabs.erase(tab_id);

  if (!g_close_requested && g_active_tab_id == tab_id) {
    int fallback_tab = 0;
    for (auto it = g_tabs.rbegin(); it != g_tabs.rend(); ++it) {
      if (it->second.profile == g_active_profile && !it->second.closing) {
        fallback_tab = it->first;
        break;
      }
    }
    g_active_tab_id = fallback_tab;
    SkAiActivateTab(g_active_tab_id);
  } else if (!g_close_requested) {
    SendTabSnapshot();
  }

  BrowserClosedDuringShutdown();
}

void SkAiCloseTab(int tab_id) {
  const auto it = g_tabs.find(tab_id);
  if (it == g_tabs.end() || g_close_requested || it->second.closing) {
    return;
  }

  it->second.closing = true;
  it->second.active = false;
  if (it->second.client) {
    it->second.client->RequestClose();
  }
  if (g_active_tab_id == tab_id) {
    g_active_tab_id = 0;
  }
  LayoutChildren();
  SendTabSnapshot();
}

void SkAiNavigateActive(const std::wstring& url) {
  if (g_close_requested) {
    return;
  }
  if (g_active_tab_id == 0) {
    SkAiCreateTab(url, true);
    return;
  }

  const auto it = g_tabs.find(g_active_tab_id);
  const auto browser = it != g_tabs.end() && it->second.client
                           ? it->second.client->GetBrowser()
                           : nullptr;
  if (browser) {
    browser->GetMainFrame()->LoadURL(url);
  }
}


void SkAiShareCurrentPage(CefRefPtr<CefBrowser> ui_browser) {
  if (g_close_requested) return;
  if (!ui_browser) return;
  const auto it = g_tabs.find(g_active_tab_id);
  if (it == g_tabs.end() || it->second.closing || !it->second.client) {
    NotifyPageContext(ui_browser, false, L"", L"", L"",
                      L"Сначала откройте веб-страницу во вкладке браузера.");
    return;
  }

  const auto browser = it->second.client->GetBrowser();
  const auto frame = browser ? browser->GetMainFrame() : nullptr;
  if (!frame) {
    NotifyPageContext(ui_browser, false, L"", L"", L"",
                      L"Не удалось получить активную страницу.");
    return;
  }

  const std::wstring url = frame->GetURL().ToWString();
  std::wstring normalized_url = url;
  std::transform(normalized_url.begin(), normalized_url.end(),
                 normalized_url.begin(), [](wchar_t ch) {
                   return static_cast<wchar_t>(std::towlower(ch));
                 });
  if (normalized_url.rfind(L"https://", 0) != 0 &&
      normalized_url.rfind(L"http://", 0) != 0) {
    NotifyPageContext(ui_browser, false, L"", url, L"",
                      L"Передача доступна только для открытых HTTP(S)-страниц.");
    return;
  }

  const std::wstring title =
      it->second.title.empty() ? url : it->second.title;
  frame->GetText(new PageTextVisitor(ui_browser, frame, g_active_tab_id, title, url, g_active_profile));
}

void SkAiBrowserAction(const std::wstring& action,
                        const std::wstring& parameter) {
  if (g_close_requested) return;
  const auto it = g_tabs.find(g_active_tab_id);
  const auto browser = it != g_tabs.end() && it->second.client
                           ? it->second.client->GetBrowser()
                           : nullptr;
  if (!browser) return;

  const auto host = browser->GetHost();
  if (action == L"back" && browser->CanGoBack()) {
    browser->GoBack();
  } else if (action == L"forward" && browser->CanGoForward()) {
    browser->GoForward();
  } else if (action == L"reload") {
    browser->Reload();
  } else if (action == L"stop") {
    browser->StopLoad();
  } else if (action == L"find") {
    if (parameter.empty()) {
      host->StopFinding(true);
    } else {
      host->Find(parameter, true, false, false);
    }
  } else if (action == L"findNext") {
    if (!parameter.empty()) host->Find(parameter, true, false, true);
  } else if (action == L"findPrevious") {
    if (!parameter.empty()) host->Find(parameter, false, false, true);
  } else if (action == L"stopFind") {
    host->StopFinding(true);
  } else if (action == L"zoomIn") {
    host->SetZoomLevel(std::clamp(host->GetZoomLevel() + 0.5, -3.0, 5.0));
  } else if (action == L"zoomOut") {
    host->SetZoomLevel(std::clamp(host->GetZoomLevel() - 0.5, -3.0, 5.0));
  } else if (action == L"zoomReset") {
    host->SetZoomLevel(0.0);
  } else if (action == L"print") {
    host->Print();
  }
}

void SkAiNotifyFindResult(int tab_id, int match_count,
                          int active_match_ordinal, bool final_update) {
  if (g_close_requested || tab_id != g_active_tab_id ||
      !g_ui_client || !g_ui_client->GetBrowser()) {
    return;
  }
  const auto ui_browser = g_ui_client->GetBrowser();
  const auto frame = ui_browser->GetMainFrame();
  if (!frame) return;
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.findResult&&"
      L"window.skaiNative.findResult(" + std::to_wstring(match_count) + L"," +
      std::to_wstring(active_match_ordinal) + L"," +
      (final_update ? L"true" : L"false") + L");",
      frame->GetURL(), 0);
}

void SkAiSetContentBounds(int x, int y, int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  g_content_bounds = {x, y, x + width, y + height};
  if (g_window) {
    PostMessageW(g_window, kLayout, 0, 0);
  }
}

void SkAiNotifyTabState(int tab_id, const std::wstring& title,
                        const std::wstring& url, bool loading) {
  if (g_close_requested) {
    return;
  }

  const auto tab = g_tabs.find(tab_id);
  std::wstring effective_title = title;
  std::wstring effective_url = url;
  if (tab != g_tabs.end()) {
    if (!title.empty() && title != L"Загрузка…") tab->second.title = title;
    if (!url.empty()) tab->second.url = url;
    tab->second.is_loading = loading;
    if (effective_title.empty()) effective_title = tab->second.title;
    if (effective_url.empty()) effective_url = tab->second.url;
    if (effective_title == L"Новая вкладка" && !effective_url.empty()) {
      effective_title = effective_url;
    }
  }

  if (!g_ui_client || !g_ui_client->GetBrowser()) {
    return;
  }

  const auto json_string = [](const std::wstring& value) {
    auto cef_value = CefValue::Create();
    cef_value->SetString(value);
    return CefWriteJSON(cef_value, JSON_WRITER_DEFAULT).ToWString();
  };

  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.updateTab(" +
          std::to_wstring(tab_id) + L"," + json_string(effective_title) + L"," +
          json_string(effective_url) + L"," + (loading ? L"true" : L"false") + L");",
      frame->GetURL(), 0);
}

void SkAiNotifyNavigationState(int tab_id, bool can_go_back,
                               bool can_go_forward, bool is_loading) {
  if (g_close_requested || !g_ui_client || !g_ui_client->GetBrowser()) {
    return;
  }

  const auto tab = g_tabs.find(tab_id);
  if (tab != g_tabs.end()) {
    tab->second.can_go_back = can_go_back;
    tab->second.can_go_forward = can_go_forward;
    tab->second.is_loading = is_loading;
  }

  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.navigationState(" +
          std::to_wstring(tab_id) + L"," +
          (can_go_back ? L"true" : L"false") + L"," +
          (can_go_forward ? L"true" : L"false") + L"," +
          (is_loading ? L"true" : L"false") + L");",
      frame->GetURL(), 0);
}

std::uint64_t SkAiRequestSitePermission(
    int tab_id, const std::wstring& requesting_origin,
    std::uint32_t requested_permissions,
    bool media_request,
    CefRefPtr<CefPermissionPromptCallback> prompt_callback,
    CefRefPtr<CefMediaAccessCallback> media_callback,
    std::uint64_t cef_prompt_id) {
  if (g_close_requested || !g_ui_client || !g_ui_client->GetBrowser()) {
    return 0;
  }
  if ((media_request && !media_callback) ||
      (!media_request && !prompt_callback)) {
    return 0;
  }

  LoadSitePermissionDecisions();
  const std::wstring permission_key = MakePermissionDecisionKey(
      requesting_origin, requested_permissions, media_request);
  const auto saved = g_site_permission_decisions.find(permission_key);
  if (saved != g_site_permission_decisions.end()) {
    if (media_request) {
      if (saved->second) media_callback->Continue(requested_permissions);
      else media_callback->Cancel();
    } else {
      prompt_callback->Continue(saved->second
          ? CEF_PERMISSION_RESULT_ACCEPT : CEF_PERMISSION_RESULT_DENY);
    }
    return 1;
  }

  const std::uint64_t request_id = g_next_permission_id++;
  PendingPermission request;
  request.tab_id = tab_id;
  request.origin = requesting_origin;
  request.permission_key = permission_key;
  request.description =
      DescribePermissionRequest(requested_permissions, media_request);
  request.requested_permissions = requested_permissions;
  request.media_request = media_request;
  request.cef_prompt_id = cef_prompt_id;
  request.prompt_callback = prompt_callback;
  request.media_callback = media_callback;
  g_pending_permissions.emplace(request_id, request);

  if (!media_request) {
    g_cef_prompt_to_request[cef_prompt_id] = request_id;
  }

  NotifyPermissionRequested(request_id, request);
  return request_id;
}

void SkAiResolvePermission(std::uint64_t request_id, bool allow, bool remember) {
  const auto it = g_pending_permissions.find(request_id);
  if (it == g_pending_permissions.end()) {
    return;
  }

  PendingPermission request = it->second;
  g_pending_permissions.erase(it);
  if (remember && !request.permission_key.empty()) {
    g_site_permission_decisions[request.permission_key] = allow;
    if (!SaveSitePermissionDecisions()) {
      g_site_permission_decisions.erase(request.permission_key);
      if (g_ui_client && g_ui_client->GetBrowser()) {
        const auto ui_browser = g_ui_client->GetBrowser();
        const auto frame = ui_browser->GetMainFrame();
        frame->ExecuteJavaScript(
            L"window.skaiNative&&window.skaiNative.permissionStorageFailed&&window.skaiNative.permissionStorageFailed();",
            frame->GetURL(), 0);
      }
    }
  }
  if (!request.media_request) {
    g_cef_prompt_to_request.erase(request.cef_prompt_id);
  }

  if (request.media_request) {
    if (request.media_callback) {
      if (allow) {
        request.media_callback->Continue(request.requested_permissions);
      } else {
        request.media_callback->Cancel();
      }
    }
  } else if (request.prompt_callback) {
    request.prompt_callback->Continue(
        allow ? CEF_PERMISSION_RESULT_ACCEPT : CEF_PERMISSION_RESULT_DENY);
  }
  NotifyPermissionDismissed(request_id);
}

void SkAiDismissPermission(std::uint64_t cef_prompt_id) {
  const auto mapping = g_cef_prompt_to_request.find(cef_prompt_id);
  if (mapping == g_cef_prompt_to_request.end()) {
    return;
  }
  const std::uint64_t request_id = mapping->second;
  g_cef_prompt_to_request.erase(mapping);
  g_pending_permissions.erase(request_id);
  NotifyPermissionDismissed(request_id);
}

void SkAiSendSitePermissionList() {
  if (g_close_requested) return;
  LoadSitePermissionDecisions();
  auto list = CefListValue::Create();
  std::size_t index = 0;

  for (const auto& [key, allow] : g_site_permission_decisions) {
    const auto last_separator = key.rfind(L'|');
    if (last_separator == std::wstring::npos) continue;
    const auto previous_separator = key.rfind(L'|', last_separator - 1);
    if (previous_separator == std::wstring::npos) continue;

    const std::wstring origin = key.substr(0, previous_separator);
    const std::wstring kind = key.substr(previous_separator + 1,
                                         last_separator - previous_separator - 1);
    const std::wstring mask_text = key.substr(last_separator + 1);
    wchar_t* end = nullptr;
    const unsigned long mask_value = std::wcstoul(mask_text.c_str(), &end, 10);
    if (end == mask_text.c_str() || *end != L'\0' ||
        mask_value > UINT32_MAX) {
      continue;
    }

    const bool media_request = kind == L"media";
    auto item = CefDictionaryValue::Create();
    item->SetString("key", key);
    item->SetString("origin", origin);
    item->SetString("description",
                    DescribePermissionRequest(
                        static_cast<std::uint32_t>(mask_value),
                        media_request));
    item->SetBool("allow", allow);
    item->SetBool("media", media_request);
    list->SetDictionary(index++, item);
  }

  auto value = CefValue::Create();
  value->SetList(list);
  const auto json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToWString();

  if (!g_ui_client || !g_ui_client->GetBrowser()) return;
  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.sitePermissionsUpdated&&window.skaiNative.sitePermissionsUpdated(" +
          json + L");",
      frame->GetURL(), 0);
}

void SkAiRevokeSitePermission(const std::wstring& permission_key) {
  if (g_close_requested) return;
  LoadSitePermissionDecisions();
  const auto existing = g_site_permission_decisions.find(permission_key);
  if (existing == g_site_permission_decisions.end()) return;

  const bool previous_decision = existing->second;
  g_site_permission_decisions.erase(existing);
  if (!SaveSitePermissionDecisions()) {
    g_site_permission_decisions[permission_key] = previous_decision;
    if (g_ui_client && g_ui_client->GetBrowser()) {
      const auto browser = g_ui_client->GetBrowser();
      const auto frame = browser->GetMainFrame();
      frame->ExecuteJavaScript(
          L"window.skaiNative&&window.skaiNative.permissionStorageFailed&&window.skaiNative.permissionStorageFailed();",
          frame->GetURL(), 0);
    }
    return;
  }
  SkAiSendSitePermissionList();
}

void SkAiCancelPermissionsForTab(int tab_id) {
  for (auto it = g_pending_permissions.begin();
       it != g_pending_permissions.end();) {
    if (it->second.tab_id != tab_id) {
      ++it;
      continue;
    }

    const auto request_id = it->first;
    const auto request = it->second;
    if (request.media_request) {
      if (request.media_callback) request.media_callback->Cancel();
    } else {
      g_cef_prompt_to_request.erase(request.cef_prompt_id);
      if (request.prompt_callback) {
        request.prompt_callback->Continue(CEF_PERMISSION_RESULT_DISMISS);
      }
    }
    it = g_pending_permissions.erase(it);
    NotifyPermissionDismissed(request_id);
  }
}

void SkAiCancelAllPermissions() {
  // Detach state before invoking CEF callbacks. CEF may synchronously emit
  // dismissal notifications while callbacks are being completed.
  auto pending = std::move(g_pending_permissions);
  g_pending_permissions.clear();
  g_cef_prompt_to_request.clear();

  for (auto& [request_id, request] : pending) {
    if (request.media_request) {
      if (request.media_callback) request.media_callback->Cancel();
    } else if (request.prompt_callback) {
      request.prompt_callback->Continue(CEF_PERMISSION_RESULT_DENY);
    }
  }
}

void SkAiSetFloatingAssistantHandle(HWND hwnd) {
  g_assistant_window = hwnd;
  if (IsWindow(hwnd)) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 480, 760,
                 SWP_NOMOVE | SWP_SHOWWINDOW);
    ShowWindow(hwnd, SW_SHOW);
  }
}

void SkAiNotifyTabFavicon(int tab_id, const std::wstring& icon_url) {
  if (g_close_requested || !g_ui_client || !g_ui_client->GetBrowser()) {
    return;
  }

  auto cef_value = CefValue::Create();
  cef_value->SetString(icon_url);
  const auto json = CefWriteJSON(cef_value, JSON_WRITER_DEFAULT).ToWString();
  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.updateFavicon(" +
          std::to_wstring(tab_id) + L"," + json + L");",
      frame->GetURL(), 0);
}

void SkAiNotifyDownloadState(CefRefPtr<CefDownloadItem> item,
                             CefRefPtr<CefDownloadItemCallback> callback) {
  if (g_close_requested || !item || !item->IsValid()) {
    return;
  }

  const std::uint32_t id = item->GetId();
  const bool in_progress = item->IsInProgress();
  const bool paused = item->IsPaused();
  const bool complete = item->IsComplete();
  const bool canceled = item->IsCanceled();
  const bool interrupted = item->IsInterrupted();

  std::wstring state = L"pending";
  if (complete) state = L"complete";
  else if (canceled) state = L"cancelled";
  else if (interrupted) state = L"interrupted";
  else if (paused) state = L"paused";
  else if (in_progress) state = L"active";

  auto& record = g_downloads[id];
  record.path = item->GetFullPath().ToWString();
  record.callback = (in_progress || paused) ? callback : nullptr;

  auto dictionary = CefDictionaryValue::Create();
  dictionary->SetDouble("id", static_cast<double>(id));
  dictionary->SetString("url", item->GetURL());
  dictionary->SetString("fileName", item->GetSuggestedFileName());
  dictionary->SetString("path", record.path);
  dictionary->SetString("state", state);
  dictionary->SetString("mimeType", item->GetMimeType());
  dictionary->SetInt("percent", item->GetPercentComplete());
  dictionary->SetDouble("receivedBytes",
                        static_cast<double>(item->GetReceivedBytes()));
  dictionary->SetDouble("totalBytes",
                        static_cast<double>(item->GetTotalBytes()));
  dictionary->SetDouble("speedBytesPerSec",
                        static_cast<double>(item->GetCurrentSpeed()));

  auto value = CefValue::Create();
  value->SetDictionary(dictionary);
  const auto json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToWString();

  if (!g_ui_client || !g_ui_client->GetBrowser()) {
    return;
  }
  const auto browser = g_ui_client->GetBrowser();
  const auto frame = browser->GetMainFrame();
  frame->ExecuteJavaScript(
      L"window.skaiNative&&window.skaiNative.downloadUpdated&&window.skaiNative.downloadUpdated(" +
          json + L");",
      frame->GetURL(), 0);
}

void SkAiDownloadAction(std::uint32_t download_id, const std::wstring& action) {
  const auto it = g_downloads.find(download_id);
  if (it == g_downloads.end()) {
    return;
  }

  if (action == L"pause" && it->second.callback) {
    it->second.callback->Pause();
    return;
  }
  if (action == L"resume" && it->second.callback) {
    it->second.callback->Resume();
    return;
  }
  if (action == L"cancel" && it->second.callback) {
    it->second.callback->Cancel();
    return;
  }

  if (it->second.path.empty()) {
    return;
  }
  std::error_code error;
  if (!std::filesystem::exists(it->second.path, error) || error) {
    return;
  }

  if (action == L"open") {
    ShellExecuteW(nullptr, L"open", it->second.path.c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
  } else if (action == L"show") {
    const std::wstring arguments = L"/select,\"" + it->second.path + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr,
                  SW_SHOWNORMAL);
  }
}

void SkAiOpenDownloadedFile(const std::wstring& path,
                            const std::wstring& action) {
  if (action != L"open" && action != L"show") {
    return;
  }

  std::filesystem::path validated_path;
  if (!ResolveManagedDownloadPath(path, validated_path)) {
    return;
  }

  if (action == L"open") {
    ShellExecuteW(nullptr, L"open", validated_path.c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
  } else {
    const std::wstring arguments =
        L"/select,\"" + validated_path.wstring() + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr,
                  SW_SHOWNORMAL);
  }
}

void SkAiToggleFloatingAssistant() {
  if (g_close_requested) {
    return;
  }

  if (IsWindow(g_assistant_window)) {
    ShowWindow(g_assistant_window, SW_SHOW);
    SetWindowPos(g_assistant_window, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    return;
  }

  CefWindowInfo window_info;
  window_info.SetAsPopup(nullptr, L"Sk/Ai Assistant");
  CefBrowserSettings browser_settings;
  g_assistant_client = new SkAiClient(nullptr, -1, true);

  ++g_pending_browser_creates;
  const std::wstring assistant_url =
      UiFile(L"assistant.html") + L"?profile=" + g_active_profile;
  const auto assistant_context =
      GetOrCreateProfileContext(g_active_profile, true);
  if (!assistant_context) {
    --g_pending_browser_creates;
    g_assistant_client = nullptr;
    return;
  }
  const bool started = CefBrowserHost::CreateBrowser(
      window_info, g_assistant_client, assistant_url,
      browser_settings, nullptr, assistant_context);
  if (!started) {
    --g_pending_browser_creates;
    g_assistant_client = nullptr;
    MaybeFinishClose();
  }
}

HWND SkAiCreateMainWindow(HINSTANCE instance, int show_command) {
  WNDCLASSEXW window_class{sizeof(window_class)};
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  window_class.lpszClassName = kClass;

  if (!RegisterClassExW(&window_class) &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return nullptr;
  }

  g_window = CreateWindowExW(
      0, kClass, L"Sk/Ai", WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX |
                              WS_MAXIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, 1440, 900, nullptr, nullptr, instance,
      nullptr);
  if (!g_window) {
    return nullptr;
  }

  const auto ui_context = GetOrCreateProfileContext(L"default", true);
  if (!ui_context) {
    DestroyWindow(g_window);
    g_window = nullptr;
    return nullptr;
  }
  g_ui_client = new SkAiClient(g_window, 0, true);

  CefWindowInfo window_info;
  RECT client_rect{};
  GetClientRect(g_window, &client_rect);
  window_info.SetAsChild(
      g_window,
      CefRect(client_rect.left, client_rect.top,
              client_rect.right - client_rect.left,
              client_rect.bottom - client_rect.top));
  CefBrowserSettings browser_settings;

  ++g_pending_browser_creates;
  const bool started = CefBrowserHost::CreateBrowser(
      window_info, g_ui_client, UiFile(L"index.html"), browser_settings,
      nullptr, ui_context);
  if (!started) {
    --g_pending_browser_creates;
    g_ui_client = nullptr;
    DestroyWindow(g_window);
    g_window = nullptr;
    return nullptr;
  }

  ShowWindow(g_window, show_command);
  UpdateWindow(g_window);
  return g_window;
}
