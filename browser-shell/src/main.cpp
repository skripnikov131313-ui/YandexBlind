#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "skai_app.h"
#include "skai_window.h"

#include <windows.h>
#include <shlobj.h>
#include <filesystem>

namespace {
int RunSkAi(HINSTANCE instance, int show_command, void* sandbox_info) {
  CefMainArgs args(instance);
  CefRefPtr<SkAiApp> app = new SkAiApp();

  // The CEF bootstrap supplies this handle. A missing handle is treated as
  // an explicit development fallback only; packaged builds use the sandbox.
  const int subprocess_code = CefExecuteProcess(args, app, sandbox_info);
  if (subprocess_code >= 0) {
    return subprocess_code;
  }

  CefSettings settings;
  settings.no_sandbox = (sandbox_info == nullptr);
  settings.multi_threaded_message_loop = false;
  settings.external_message_pump = false;
  settings.windowless_rendering_enabled = false;
  settings.log_severity = LOGSEVERITY_WARNING;

  PWSTR local = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(
          FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local))) {
    const auto profile =
        std::filesystem::path(local) / L"SkAi" / L"Profiles" / L"Default";
    std::filesystem::create_directories(profile);
    CefString(&settings.root_cache_path) = profile.wstring();
    CefString(&settings.cache_path) = (profile / L"Cache").wstring();
    settings.persist_session_cookies = true;
    settings.persist_user_preferences = true;
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
// CEF 151+ may append version metadata as the final argument. Keeping it
// opaque lets this client ignore optional bootstrap metadata while forwarding
// the sandbox handle required by CEF.
extern "C" __declspec(dllexport) int RunWinMain(
    HINSTANCE instance,
    LPWSTR /*command_line*/,
    int show_command,
    void* sandbox_info,
    void* /*version_info*/) {
  return RunSkAi(instance, show_command, sandbox_info);
}
