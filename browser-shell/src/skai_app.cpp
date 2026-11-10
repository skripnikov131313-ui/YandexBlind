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

    if (name == "windowAction") {
      if (arguments.empty() || !arguments[0]->IsString()) {
        exception = "A window action name is required.";
        return true;
      }
      const auto message = CefProcessMessage::Create("skai.windowAction");
      message->GetArgumentList()->SetString(0, arguments[0]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    return false;
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
      skaiNative.minimize = function() { windowAction("minimize"); };
      skaiNative.toggleMaximize = function() { windowAction("toggleMaximize"); };
      skaiNative.close = function() { windowAction("close"); };
    })();
  )", bridge);
}
