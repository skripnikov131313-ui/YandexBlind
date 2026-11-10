#include "skai_window.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_task.h"
#include "skai_client.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace {
constexpr wchar_t kWindowClass[] = L"SkAi.NativeBrowserWindow";
constexpr UINT kLayoutMessage = WM_APP + 20;
HWND g_window = nullptr;
CefRefPtr<SkAiClient> g_ui_client;
struct BrowserTab {
  int id = 0;
  CefRefPtr<SkAiClient> client;
  bool active = false;
};
std::map<int, BrowserTab> g_tabs;
int g_next_tab_id = 1;
int g_active_tab_id = 0;
RECT g_content_bounds{0, 100, 800, 600};

std::wstring UiStartPage() {
  wchar_t executable[MAX_PATH]{};
  GetModuleFileNameW(nullptr, executable, MAX_PATH);
  const auto path = std::filesystem::path(executable).parent_path() / L"ui" / L"index.html";
  return L"file:///" + path.generic_wstring();
}
void LayoutChildren() {
  if (!IsWindow(g_window)) return;
  for (auto& [id, tab] : g_tabs) {
    const auto browser = tab.client ? tab.client->GetBrowser() : nullptr;
    if (!browser) continue;
    const HWND child = browser->GetHost()->GetWindowHandle();
    if (!child) continue;
    const int width = g_content_bounds.right - g_content_bounds.left;
    const int height = g_content_bounds.bottom - g_content_bounds.top;
    SetWindowPos(child, tab.active ? HWND_TOP : HWND_BOTTOM,
      g_content_bounds.left, g_content_bounds.top, width, height,
      SWP_NOACTIVATE | (tab.active ? 0 : SWP_NOZORDER));
    ShowWindow(child, tab.active ? SW_SHOW : SW_HIDE);
    browser->GetHost()->WasHidden(!tab.active);
  }
}
void SendTabSnapshot() {
  if (!g_ui_client || !g_ui_client->GetBrowser()) return;
  std::wstring json = L"[";
  bool first = true;
  for (const auto& [id, tab] : g_tabs) {
    if (!first) json += L",";
    first = false;
    json += L"{\"id\":" + std::to_wstring(id) +
      L",\"active\":" + (tab.active ? L"true" : L"false") +
      L",\"title\":\"Вкладка " + std::to_wstring(id) + L"\"}";
  }
  json += L"]";
  const std::wstring script = L"window.skaiNative&&window.skaiNative.syncTabs(" + json + L");";
  g_ui_client->GetBrowser()->GetMainFrame()->ExecuteJavaScript(script,
      g_ui_client->GetBrowser()->GetMainFrame()->GetURL(), 0);
}
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_SIZE:
      LayoutChildren();
      return 0;
    case kLayoutMessage:
      LayoutChildren();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_CLOSE:
      for (auto& [id, tab] : g_tabs)
        if (tab.client && tab.client->GetBrowser())
          tab.client->GetBrowser()->GetHost()->CloseBrowser(false);
      if (g_ui_client && g_ui_client->GetBrowser()) {
        g_ui_client->GetBrowser()->GetHost()->CloseBrowser(false);
        return 0;
      }
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      g_tabs.clear();
      g_ui_client = nullptr;
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(window, message, wparam, lparam);
  }
}
} // namespace

void SkAiCreateTab(const std::wstring& url, bool activate) {
  if (!g_window) return;
  const int id = g_next_tab_id++;
  BrowserTab tab;
  tab.id = id;
  tab.active = activate;
  tab.client = new SkAiClient(g_window, id, false);
  if (activate) {
    for (auto& [existing_id, existing] : g_tabs) existing.active = false;
    g_active_tab_id = id;
  }
  g_tabs.emplace(id, tab);
  CefWindowInfo info;
  RECT bounds = g_content_bounds;
  info.SetAsChild(g_window, bounds);
  CefBrowserSettings settings;
  CefBrowserHost::CreateBrowser(info, tab.client, url, settings, nullptr, nullptr);
  SendTabSnapshot();
}
void SkAiActivateTab(int tab_id) {
  if (!g_tabs.contains(tab_id)) return;
  for (auto& [id, tab] : g_tabs) tab.active = id == tab_id;
  g_active_tab_id = tab_id;
  LayoutChildren();
  SendTabSnapshot();
}
void SkAiCloseTab(int tab_id) {
  const auto it = g_tabs.find(tab_id);
  if (it == g_tabs.end()) return;
  if (it->second.client && it->second.client->GetBrowser())
    it->second.client->GetBrowser()->GetHost()->CloseBrowser(false);
  g_tabs.erase(it);
  if (g_active_tab_id == tab_id) {
    g_active_tab_id = g_tabs.empty() ? 0 : g_tabs.rbegin()->first;
    if (g_active_tab_id) SkAiActivateTab(g_active_tab_id);
  }
  SendTabSnapshot();
}
void SkAiNavigateActive(const std::wstring& url) {
  const auto it = g_tabs.find(g_active_tab_id);
  if (it != g_tabs.end() && it->second.client && it->second.client->GetBrowser())
    it->second.client->GetBrowser()->GetMainFrame()->LoadURL(url);
}
void SkAiBrowserAction(const std::wstring& action) {
  const auto it = g_tabs.find(g_active_tab_id);
  if (it == g_tabs.end() || !it->second.client || !it->second.client->GetBrowser()) return;
  auto browser = it->second.client->GetBrowser();
  if (action == L"back" && browser->CanGoBack()) browser->GoBack();
  else if (action == L"forward" && browser->CanGoForward()) browser->GoForward();
  else if (action == L"reload") browser->Reload();
}
void SkAiSetContentBounds(int x, int y, int width, int height) {
  g_content_bounds = {x, y, x + width, y + height};
  if (g_window) PostMessageW(g_window, kLayoutMessage, 0, 0);
}
void SkAiNotifyTabState(int tab_id, const std::wstring& title,
                        const std::wstring& url, bool loading) {
  if (!g_ui_client || !g_ui_client->GetBrowser()) return;
  auto make_json = [](const std::wstring& value) {
    auto v = CefValue::Create(); v->SetString(value);
    return CefWriteJSON(v, JSON_WRITER_DEFAULT).ToWString();
  };
  const std::wstring script = L"window.skaiNative&&window.skaiNative.updateTab(" +
    std::to_wstring(tab_id) + L"," + make_json(title) + L"," +
    make_json(url) + L"," + (loading ? L"true" : L"false") + L");";
  auto b = g_ui_client->GetBrowser();
  b->GetMainFrame()->ExecuteJavaScript(script, b->GetMainFrame()->GetURL(), 0);
}
HWND SkAiCreateMainWindow(HINSTANCE instance, int show_command) {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = WindowProc; wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wc.lpszClassName = kWindowClass;
  RegisterClassExW(&wc);
  g_window = CreateWindowExW(0, kWindowClass, L"Sk/Ai",
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1440, 900,
    nullptr, nullptr, instance, nullptr);
  if (!g_window) return nullptr;
  g_ui_client = new SkAiClient(g_window, 0, true);
  CefWindowInfo info; RECT bounds{}; GetClientRect(g_window, &bounds);
  info.SetAsChild(g_window, bounds);
  CefBrowserSettings settings;
  CefBrowserHost::CreateBrowser(info, g_ui_client, UiStartPage(), settings, nullptr, nullptr);
  ShowWindow(g_window, show_command); UpdateWindow(g_window);
  return g_window;
}
