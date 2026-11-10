#include "skai_app.h"
#include "include/cef_v8.h"
#include "include/cef_process_message.h"

namespace {
class SkAiV8Bridge final : public CefV8Handler {
 public:
  bool Execute(const CefString& name, CefRefPtr<CefV8Value>,
      const CefV8ValueList& a, CefRefPtr<CefV8Value>&, CefString& exception) override {
    auto context=CefV8Context::GetCurrentContext(); if(!context) return false;
    auto frame=context->GetFrame(); if(!frame) return false;
    auto msg=CefProcessMessage::Create("skai.command"); auto p=msg->GetArgumentList();
    if(name=="windowAction" || name=="assistantAction") {
      if(a.empty() || !a[0]->IsString()){exception="Action required";return true;}
      p->SetString(0,name=="windowAction"?"window":"assistant");
      p->SetString(1,a[0]->GetStringValue());
    } else if(name=="navigate" || name=="browserAction") {
      if(a.empty()||!a[0]->IsString()){exception="Value required";return true;}
      p->SetString(0,name=="navigate"?"navigate":"browser");
      p->SetString(1,a[0]->GetStringValue());
    } else if(name=="tabAction") {
      if(a.size()<2){exception="Tab arguments required";return true;}
      p->SetString(0,"tab");p->SetString(1,a[0]->GetStringValue());p->SetString(2,a[1]->GetStringValue());
    } else if(name=="layoutAction") {
      if(a.empty()||!a[0]->IsString()){exception="Bounds required";return true;}
      p->SetString(0,"layout");p->SetString(1,a[0]->GetStringValue());
    } else if(name=="dragWindow") {
      p->SetString(0,"window");p->SetString(1,"drag");
    } else {return false;}
    frame->SendProcessMessage(PID_BROWSER,msg); return true;
  }
  IMPLEMENT_REFCOUNTING(SkAiV8Bridge);
};
}
void SkAiApp::OnWebKitInitialized() {
  auto bridge=new SkAiV8Bridge();
  CefRegisterExtension("skai/native", R"(
    var skaiNative={};
    (function(){
      native function windowAction(); native function assistantAction();
      native function navigate(); native function browserAction();
      native function tabAction(); native function layoutAction(); native function dragWindow();
      skaiNative.minimize=function(){windowAction("minimize")};
      skaiNative.toggleMaximize=function(){windowAction("toggleMaximize")};
      skaiNative.close=function(){windowAction("close")};
      skaiNative.beginDrag=function(){dragWindow()};
      skaiNative.navigate=function(v){navigate(String(v))};
      skaiNative.back=function(){browserAction("back")};
      skaiNative.forward=function(){browserAction("forward")};
      skaiNative.reload=function(){browserAction("reload")};
      skaiNative.stop=function(){browserAction("stop")};
      skaiNative.newTab=function(v){tabAction("new",String(v||"about:blank"))};
      skaiNative.activateTab=function(v){tabAction("activate",String(v))};
      skaiNative.closeTab=function(v){tabAction("close",String(v))};
      skaiNative.setContentBounds=function(x,y,w,h){layoutAction([x,y,w,h].join(","))};
      skaiNative.toggleFloatingAssistant=function(){assistantAction("toggle")};
    })();
  )",bridge);
}