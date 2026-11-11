#include "skai_app.h"

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_v8.h"

#include <algorithm>
#include <cwctype>
#include <string>

namespace {
class SkAiV8Bridge final : public CefV8Handler {
 public:
  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value>,
               const CefV8ValueList& args,
               CefRefPtr<CefV8Value>&,
               CefString& exception) override {
    const auto context = CefV8Context::GetCurrentContext();
    if (!context) return false;
    const auto frame = context->GetFrame();
    if (!frame) return false;

    auto message = CefProcessMessage::Create("skai.command");
    const auto values = message->GetArgumentList();
    auto send = [&](const char* category, const std::wstring& action) {
      values->SetString(0, category);
      values->SetString(1, action);
      frame->SendProcessMessage(PID_BROWSER, message);
    };
    auto require_string = [&](std::size_t index, const wchar_t* label) {
      if (index >= args.size() || !args[index] || !args[index]->IsString()) {
        exception = label;
        return false;
      }
      return true;
    };

    if (name == "minimize" || name == "toggleMaximize" || name == "close" ||
        name == "beginDrag") {
      const std::wstring action =
          name == "beginDrag" ? L"drag" : name.ToWString();
      send("window", action);
      return true;
    }
    if (name == "toggleFloatingAssistant") {
      send("assistant", L"toggle");
      return true;
    }
    if (name == "navigate") {
      if (!require_string(0, L"URL required")) return true;
      send("navigate", args[0]->GetStringValue().ToWString());
      return true;
    }
    if (name == "back" || name == "forward" || name == "reload" ||
        name == "stop") {
      send("browser", name.ToWString());
      return true;
    }
    if (name == "browserAction") {
      if (!require_string(0, L"Browser action required")) return true;
      values->SetString(2, args[0]->GetStringValue());
      if (args.size() > 1) {
        if (!args[1] || !args[1]->IsString()) {
          exception = L"Browser action parameter must be a string";
          return true;
        }
        values->SetString(3, args[1]->GetStringValue());
      }
      values->SetString(0, "browser");
      values->SetString(1, args[0]->GetStringValue());
      if (args.size() > 1) {
        values->SetString(2, args[1]->GetStringValue());
      } else {
        values->SetString(2, "");
      }
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "newTab" || name == "activateTab" || name == "closeTab") {
      const std::wstring value =
          args.empty() ? L"about:blank" :
          (args[0] && args[0]->IsString()
              ? args[0]->GetStringValue().ToWString() : L"");
      if (value.empty()) {
        exception = L"Tab value required";
        return true;
      }
      const std::wstring action =
          name == "newTab" ? L"new" :
          name == "activateTab" ? L"activate" : L"close";
      values->SetString(0, "tab");
      values->SetString(1, action);
      values->SetString(2, value);
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "downloadAction" || name == "openDownloadedFile") {
      if (!require_string(0, L"Download target required") ||
          !require_string(1, L"Download action required")) {
        return true;
      }
      values->SetString(0, name == "downloadAction" ? "download" : "downloadPath");
      values->SetString(1, args[1]->GetStringValue());
      values->SetString(2, args[0]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "resolvePermission") {
      if (!require_string(0, L"Permission ID required") ||
          !require_string(1, L"Permission action required")) {
        return true;
      }
      if (args.size() < 3 || !args[2] || !args[2]->IsBool()) {
        exception = L"Remember flag required";
        return true;
      }
      values->SetString(0, "permission");
      values->SetString(1, args[1]->GetStringValue());
      values->SetString(2, args[0]->GetStringValue());
      values->SetBool(3, args[2]->GetBoolValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "listSitePermissions") {
      send("permissionsList", L"list");
      return true;
    }
    if (name == "revokeSitePermission") {
      if (!require_string(0, L"Permission key required")) return true;
      values->SetString(0, "permissionRevoke");
      values->SetString(1, "revoke");
      values->SetString(2, args[0]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "listProfiles") {
      send("profilesList", L"list");
      return true;
    }
    if (name == "createProfile" || name == "switchProfile") {
      if (!require_string(0, L"Profile name required")) return true;
      values->SetString(0, name == "createProfile" ? "profileCreate" : "profileSwitch");
      values->SetString(1, name == "createProfile" ? "create" : "switch");
      values->SetString(2, args[0]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "setContentBounds") {
      if (args.size() < 4) {
        exception = L"Content bounds require x, y, width and height";
        return true;
      }
      std::wstring bounds;
      for (std::size_t index = 0; index < 4; ++index) {
        if (!args[index] || !args[index]->IsInt()) {
          exception = L"Content bounds must be integers";
          return true;
        }
        if (!bounds.empty()) bounds += L",";
        bounds += std::to_wstring(args[index]->GetIntValue());
      }
      values->SetString(0, "layout");
      values->SetString(1, bounds);
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "aiStatus" || name == "installLocalModel") {
      send("ai", name == "aiStatus" ? L"status" : L"install");
      return true;
    }
    if (name == "sendLocalAi") {
      if (!require_string(0, L"AI request ID required") ||
          !require_string(1, L"AI request body required")) {
        return true;
      }
      values->SetString(0, "ai");
      values->SetString(1, "send");
      values->SetString(2, args[0]->GetStringValue());
      values->SetString(3, args[1]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(SkAiV8Bridge);
};

bool IsTrustedSkAiUiFrame(CefRefPtr<CefFrame> frame) {
  if (!frame || !frame->IsMain()) return false;
  std::wstring url = frame->GetURL().ToWString();
  std::transform(url.begin(), url.end(), url.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  if (url.rfind(L"file:///", 0) != 0) return false;
  return url.find(L"/ui/index.html") != std::wstring::npos ||
         url.find(L"/ui/assistant.html") != std::wstring::npos;
}

}  // namespace

void SkAiApp::OnContextCreated(CefRefPtr<CefBrowser>,
                               CefRefPtr<CefFrame> frame,
                               CefRefPtr<CefV8Context> context) {
  if (!IsTrustedSkAiUiFrame(frame) || !context) return;

  const auto global = context->GetGlobal();
  if (!global) return;

  const auto bridge = new SkAiV8Bridge();
  const auto api = CefV8Value::CreateObject(nullptr, nullptr);
  const char* const functions[] = {
      "minimize", "toggleMaximize", "close", "beginDrag",
      "toggleFloatingAssistant", "navigate", "back", "forward",
      "reload", "stop", "browserAction", "newTab", "activateTab",
      "closeTab", "downloadAction", "openDownloadedFile",
      "resolvePermission", "listSitePermissions", "revokeSitePermission",
      "listProfiles", "createProfile", "switchProfile", "setContentBounds",
      "aiStatus", "installLocalModel", "sendLocalAi"};
  for (const char* function : functions) {
    api->SetValue(function, CefV8Value::CreateFunction(function, bridge),
                  V8_PROPERTY_ATTRIBUTE_NONE);
  }
  global->SetValue("skaiNative", api, V8_PROPERTY_ATTRIBUTE_NONE);
}
