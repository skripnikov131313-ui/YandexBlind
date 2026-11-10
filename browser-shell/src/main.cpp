#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "skai_app.h"
#include "skai_window.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>

int APIENTRY wWinMain(HINSTANCE instance,HINSTANCE,wchar_t*,int show){
 CefMainArgs args(instance);auto app=new SkAiApp();
 int code=CefExecuteProcess(args,app,nullptr);if(code>=0)return code;
 CefSettings settings;
 settings.no_sandbox=false;
 settings.multi_threaded_message_loop=false;
 settings.external_message_pump=false;
 settings.windowless_rendering_enabled=false;
 settings.log_severity=LOGSEVERITY_WARNING;
 PWSTR local=nullptr;
 if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&local))){
  auto profile=std::filesystem::path(local)/L"SkAi"/L"Profiles"/L"Default";
  std::filesystem::create_directories(profile);
  CefString(&settings.root_cache_path)=profile.wstring();
  CefString(&settings.cache_path)=(profile/L"Cache").wstring();
  settings.persist_session_cookies=true;
  settings.persist_user_preferences=true;
  CoTaskMemFree(local);
 }
 if(!CefInitialize(args,settings,app,nullptr))return CefGetExitCode();
 HWND window=SkAiCreateMainWindow(instance,show);
 if(!window){CefShutdown();return 1;}
 CefRunMessageLoop();CefShutdown();return 0;
}