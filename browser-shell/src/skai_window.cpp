#include "skai_window.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "skai_client.h"

#include <filesystem>
#include <string>

namespace {
constexpr wchar_t kWindowClass[] = L"SkAi.NativeBrowserWindow";
HWND g_window = nullptr;
CefRefPtr<SkAiClient> g_client;

std::wstring UiStartPage() {
  wchar_t executable[MAX_PATH]{};
  GetModuleFileNameW(nullptr, executable, MAX_PATH);
  const auto path = std::filesystem::path(executable).parent_path() / L"ui" / L"index.html";
  return L"file:///" + path.wstring();
}

void LayoutBrowser(HWND window) {
  if (!g_client || !g_client->GetBrowser()) return;
  RECT bounds{};
  GetClientRect(window, &bounds);
  const int width = bounds.right - bounds.left;
  const int height = bounds.bottom - bounds.top;
  const auto host = g_client->GetBrowser()->GetHost();
  if (host && host->GetWindowHandle()) {
    SetWindowPos(host->GetWindowHandle(), nullptr, 0, 0, width,
                 height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_SIZE:
      LayoutBrowser(window);
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_CLOSE:
      if (g_client && g_client->GetBrowser()) {
        g_client->GetBrowser()->GetHost()->CloseBrowser(false);
        return 0;
      }
      DestroyWindow(window);
      return 0;
    case WM_APP + 11:
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      g_client = nullptr;
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(window, message, wparam, lparam);
  }
}
} // namespace

HWND SkAiCreateMainWindow(HINSTANCE instance, int show_command) {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wc.lpszClassName = kWindowClass;
  RegisterClassExW(&wc);

  g_window = CreateWindowExW(0, kWindowClass, L"Sk/Ai", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 1440, 900, nullptr, nullptr, instance, nullptr);
  if (!g_window) return nullptr;

  g_client = new SkAiClient(g_window);
  CefWindowInfo info;
  RECT bounds{};
  GetClientRect(g_window, &bounds);
  info.SetAsChild(g_window, bounds);
  CefBrowserSettings browser_settings;
  CefBrowserHost::CreateBrowser(info, g_client, UiStartPage(), browser_settings, nullptr, nullptr);

  ShowWindow(g_window, show_command);
  UpdateWindow(g_window);
  return g_window;
}
