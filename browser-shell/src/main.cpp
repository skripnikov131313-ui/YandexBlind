#include "include/cef_app.h"
#include "skai_app.h"
#include "skai_window.h"

#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int show_command) {
  CefMainArgs main_args(instance);
  const int subprocess_code = CefExecuteProcess(main_args, nullptr, nullptr);
  if (subprocess_code >= 0) return subprocess_code;

  CefSettings settings;
  settings.no_sandbox = true; // Development only. Production packaging must ship the CEF sandbox.
  settings.multi_threaded_message_loop = false;
  settings.external_message_pump = false;
  settings.windowless_rendering_enabled = false;
  settings.log_severity = LOGSEVERITY_WARNING;

  const auto app = new SkAiApp();
  if (!CefInitialize(main_args, settings, app, nullptr)) return CefGetExitCode();

  const HWND window = SkAiCreateMainWindow(instance, show_command);
  if (!window) {
    CefShutdown();
    return 1;
  }

  CefRunMessageLoop();
  CefShutdown();
  return 0;
}
