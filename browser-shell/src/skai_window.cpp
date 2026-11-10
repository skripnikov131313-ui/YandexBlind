#include "skai_window.h"

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_values.h"
#include "include/cef_parser.h"
#include "skai_client.h"

#include <windowsx.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

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
  bool active = false;
  bool can_go_back = false;
  bool can_go_forward = false;
  bool is_loading = false;
  std::wstring title = L"Новая вкладка";
  std::wstring url;
};
std::map<int, Tab> g_tabs;

int g_next_tab_id = 1;
int g_active_tab_id = 0;
RECT g_content_bounds{0, 100, 800, 600};

bool g_close_requested = false;
bool g_finish_close_posted = false;
int g_pending_browser_creates = 0;
int g_pending_browser_closes = 0;

std::wstring UiFile(const wchar_t* file) {
  wchar_t exe[MAX_PATH]{};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  const auto path = std::filesystem::path(exe).parent_path() / L"ui" / file;
  return L"file:///" + path.generic_wstring();
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

    if (tab.active) {
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
  for (auto it = g_tabs.begin(); it != g_tabs.end(); ++it) {
    if (it != g_tabs.begin()) {
      json += L",";
    }

    const auto& tab = it->second;
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

  const int id = g_next_tab_id++;
  Tab tab;
  tab.id = id;
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
  window_info.SetAsChild(g_window, g_content_bounds);
  CefBrowserSettings browser_settings;

  ++g_pending_browser_creates;
  const bool started = CefBrowserHost::CreateBrowser(
      window_info, tab.client, url, browser_settings, nullptr, nullptr);
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
  if (tab_id != 0 && !g_tabs.contains(tab_id)) {
    return;
  }

  for (auto& [id, tab] : g_tabs) {
    tab.active = (id == tab_id);
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
    g_active_tab_id = g_tabs.empty() ? 0 : g_tabs.rbegin()->first;
    SkAiActivateTab(g_active_tab_id);
  } else if (!g_close_requested) {
    SendTabSnapshot();
  }

  BrowserClosedDuringShutdown();
}

void SkAiCloseTab(int tab_id) {
  const auto it = g_tabs.find(tab_id);
  if (it == g_tabs.end() || g_close_requested) {
    return;
  }

  if (it->second.client) {
    it->second.client->RequestClose();
  }
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

void SkAiBrowserAction(const std::wstring& action) {
  if (g_close_requested) {
    return;
  }
  const auto it = g_tabs.find(g_active_tab_id);
  const auto browser = it != g_tabs.end() && it->second.client
                           ? it->second.client->GetBrowser()
                           : nullptr;
  if (!browser) {
    return;
  }

  if (action == L"back" && browser->CanGoBack()) {
    browser->GoBack();
  } else if (action == L"forward" && browser->CanGoForward()) {
    browser->GoForward();
  } else if (action == L"reload") {
    browser->Reload();
  } else if (action == L"stop") {
    browser->StopLoad();
  }
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
    if (!title.empty()) tab->second.title = title;
    if (!url.empty()) tab->second.url = url;
    tab->second.is_loading = loading;
    if (effective_title.empty()) effective_title = tab->second.title;
    if (effective_url.empty()) effective_url = tab->second.url;
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
  const bool started = CefBrowserHost::CreateBrowser(
      window_info, g_assistant_client, UiFile(L"assistant.html"),
      browser_settings, nullptr, nullptr);
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

  g_ui_client = new SkAiClient(g_window, 0, true);

  CefWindowInfo window_info;
  RECT client_rect{};
  GetClientRect(g_window, &client_rect);
  window_info.SetAsChild(g_window, client_rect);
  CefBrowserSettings browser_settings;

  ++g_pending_browser_creates;
  const bool started = CefBrowserHost::CreateBrowser(
      window_info, g_ui_client, UiFile(L"index.html"), browser_settings,
      nullptr, nullptr);
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
