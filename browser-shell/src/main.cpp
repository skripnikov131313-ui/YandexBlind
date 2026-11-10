#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "skai_app.h"
#include "skai_window.h"

#include <windows.h>
#include <shlobj.h>
#include <filesystem>

namespace {
int RunSkAi(HINSTANCE instance, int show_command, void* sandbox_info) {
  CefMainArgs args(instance);
  CefRefPtr<SkAiApp> app = new SkAiApp();

  // Refuse to start without the bootstrap-provided sandbox handle. Running
  // this browser without Chromium's renderer sandbox is not an accepted mode.
  if (sandbox_info == nullptr) {
    return 2;
  }

  const int subprocess_code = CefExecuteProcess(args, app, sandbox_info);
  if (subprocess_code >= 0) {
    return subprocess_code;
  }

  CefSettings settings;
  settings.no_sandbox = false;
  settings.multi_threaded_message_loop = false;
  settings.external_message_pump = false;
  settings.windowless_rendering_enabled = false;
  settings.log_severity = LOGSEVERITY_WARNING;

  PWSTR local = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(
          FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local))) {
    const auto profile =
        std::filesystem::path(local) / L"SkAi" / L"Profiles" / L"Default";
    std::error_code filesystem_error;
    std::filesystem::create_directories(profile, filesystem_error);
    if (!filesystem_error) {
      CefString(&settings.root_cache_path) = profile.wstring();
      CefString(&settings.cache_path) = (profile / L"Cache").wstring();
      settings.persist_session_cookies = true;
      settings.persist_user_preferences = true;
    }
    CoTaskMemFree(local);
  }

  if (!CefInitialize(args, settings, app, sandbox_info)) {
    return CefGetExitCode();
  }

  HWND window = SkAiCreateMainWindow(instance, show_command);
  if (!window) {
    CefShutdown();
    return 1;
  }

  CefRunMessageLoop();
  CefShutdown();
  return 0;
}
}  // namespace

// The CEF bootstrap resolves this exact exported symbol from SkAi.dll.
// Keep the exported entry point signature identical to cef_sandbox_win.h.
CEF_BOOTSTRAP_EXPORT int RunWinMain(
    HINSTANCE instance,
    LPTSTR /*command_line*/,
    int show_command,
    void* sandbox_info,
    cef_version_info_t* /*version_info*/) {
  return RunSkAi(instance, show_command, sandbox_info);
}
