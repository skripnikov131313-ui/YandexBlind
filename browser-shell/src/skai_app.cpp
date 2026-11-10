#include "skai_app.h"
#include "include/cef_v8.h"
#include "include/cef_process_message.h"

namespace {
class SkAiV8Bridge final : public CefV8Handler {
 public:
  bool Execute(const CefString& name, CefRefPtr<CefV8Value>,
               const CefV8ValueList& arguments, CefRefPtr<CefV8Value>&,
               CefString& exception) override {
    const auto context = CefV8Context::GetCurrentContext();
    if (!context) return false;
    const auto frame = context->GetFrame();
    if (!frame) return false;

    const auto message = CefProcessMessage::Create("skai.command");
    auto args = message->GetArgumentList();
    if (name == "windowAction") {
      if (arguments.empty() || !arguments[0]->IsString()) {
        exception = "A window action name is required.";
        return true;
      }
      args->SetString(0, "window");
      args->SetString(1, arguments[0]->GetStringValue());
    } else if (name == "navigate") {
      if (arguments.empty() || !arguments[0]->IsString()) {
        exception = "A URL is required.";
        return true;
      }
      args->SetString(0, "navigate");
      args->SetString(1, arguments[0]->GetStringValue());
    } else if (name == "browserAction") {
      if (arguments.empty() || !arguments[0]->IsString()) {
        exception = "A browser action is required.";
        return true;
      }
      args->SetString(0, "browser");
      args->SetString(1, arguments[0]->GetStringValue());
    } else {
      return false;
    }
    frame->SendProcessMessage(PID_BROWSER, message);
    return true;
  }
  IMPLEMENT_REFCOUNTING(SkAiV8Bridge);
};
} // namespace

void SkAiApp::OnWebKitInitialized() {
  const auto bridge = new SkAiV8Bridge();
  CefRegisterExtension("skai/native", R"(
    var skaiNative = {};
    (function() {
      native function windowAction();
      native function navigate();
      native function browserAction();
      skaiNative.minimize = function() { windowAction("minimize"); };
      skaiNative.toggleMaximize = function() { windowAction("toggleMaximize"); };
      skaiNative.close = function() { windowAction("close"); };
      skaiNative.navigate = function(url) { navigate(String(url)); };
      skaiNative.back = function() { browserAction("back"); };
      skaiNative.forward = function() { browserAction("forward"); };
      skaiNative.reload = function() { browserAction("reload"); };
    })();
  )", bridge);
}
