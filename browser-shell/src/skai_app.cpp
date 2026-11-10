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
      if(name=="browserAction" && a.size()>1) {
        if(!a[1]->IsString()){exception="Browser action parameter must be a string";return true;}
        p->SetString(2,a[1]->GetStringValue());
      }
    } else if(name=="downloadAction") {
      if(a.size()<2 || !a[0]->IsString() || !a[1]->IsString()){exception="Download arguments required";return true;}
      p->SetString(0,"download");p->SetString(1,a[1]->GetStringValue());p->SetString(2,a[0]->GetStringValue());
    } else if(name=="downloadPathAction") {
      if(a.size()<2 || !a[0]->IsString() || !a[1]->IsString()){exception="Download path arguments required";return true;}
      p->SetString(0,"downloadPath");p->SetString(1,a[1]->GetStringValue());p->SetString(2,a[0]->GetStringValue());
    } else if(name=="profileListAction") {
      p->SetString(0,"profilesList");p->SetString(1,"list");
    } else if(name=="profileCreateAction") {
      if(a.empty() || !a[0]->IsString()){exception="Profile name required";return true;}
      p->SetString(0,"profileCreate");p->SetString(1,"create");p->SetString(2,a[0]->GetStringValue());
    } else if(name=="profileSwitchAction") {
      if(a.empty() || !a[0]->IsString()){exception="Profile name required";return true;}
      p->SetString(0,"profileSwitch");p->SetString(1,"switch");p->SetString(2,a[0]->GetStringValue());
    } else if(name=="permissionListAction") {
      p->SetString(0,"permissionsList");p->SetString(1,"list");
    } else if(name=="permissionRevokeAction") {
      if(a.empty() || !a[0]->IsString()){exception="Permission key required";return true;}
      p->SetString(0,"permissionRevoke");p->SetString(1,"revoke");p->SetString(2,a[0]->GetStringValue());
    } else if(name=="permissionAction") {
      if(a.size()<3 || !a[0]->IsString() || !a[1]->IsString() || !a[2]->IsBool()){exception="Permission arguments required";return true;}
      p->SetString(0,"permission");p->SetString(1,a[1]->GetStringValue());p->SetString(2,a[0]->GetStringValue());p->SetBool(3,a[2]->GetBoolValue());
    } else if(name=="tabAction") {
      if(a.size()<2){exception="Tab arguments required";return true;}
      p->SetString(0,"tab");p->SetString(1,a[0]->GetStringValue());p->SetString(2,a[1]->GetStringValue());
    } else if(name=="layoutAction") {
      if(a.empty()||!a[0]->IsString()){exception="Bounds required";return true;}
      p->SetString(0,"layout");p->SetString(1,a[0]->GetStringValue());
    } else if(name=="aiAction") {
      if(a.empty() || !a[0]->IsString()){exception="AI action required";return true;}
      p->SetString(0,"ai");p->SetString(1,a[0]->GetStringValue());
      if(a.size()>1) {
        if(!a[1]->IsString()){exception="AI request ID must be a string";return true;}
        p->SetString(2,a[1]->GetStringValue());
      }
      if(a.size()>2) {
        if(!a[2]->IsString()){exception="AI request must be a string";return true;}
        p->SetString(3,a[2]->GetStringValue());
      }
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
      native function tabAction(); native function downloadAction(); native function downloadPathAction(); native function permissionAction(); native function profileListAction(); native function profileCreateAction(); native function profileSwitchAction(); native function permissionListAction(); native function permissionRevokeAction(); native function layoutAction(); native function dragWindow(); native function aiAction();
      skaiNative.minimize=function(){windowAction("minimize")};
      skaiNative.toggleMaximize=function(){windowAction("toggleMaximize")};
      skaiNative.close=function(){windowAction("close")};
      skaiNative.beginDrag=function(){dragWindow()};
      skaiNative.navigate=function(v){navigate(String(v))};
      skaiNative.back=function(){browserAction("back")};
      skaiNative.forward=function(){browserAction("forward")};
      skaiNative.reload=function(){browserAction("reload")};
      skaiNative.stop=function(){browserAction("stop")};
      skaiNative.browserAction=function(action,parameter){browserAction(String(action),String(parameter||""))};
      skaiNative.newTab=function(v){tabAction("new",String(v||"about:blank"))};
      skaiNative.activateTab=function(v){tabAction("activate",String(v))};
      skaiNative.closeTab=function(v){tabAction("close",String(v))};
      skaiNative.downloadAction=function(id,action){downloadAction(String(id),String(action))};
      skaiNative.openDownloadedFile=function(path,action){downloadPathAction(String(path),String(action))};
      skaiNative.resolvePermission=function(id,action,remember){permissionAction(String(id),String(action),!!remember)};
      skaiNative.listSitePermissions=function(){permissionListAction()};
      skaiNative.revokeSitePermission=function(key){permissionRevokeAction(String(key))};
      skaiNative.listProfiles=function(){profileListAction()};
      skaiNative.createProfile=function(name){profileCreateAction(String(name))};
      skaiNative.switchProfile=function(name){profileSwitchAction(String(name))};
      skaiNative.setContentBounds=function(x,y,w,h){layoutAction([x,y,w,h].join(","))};
      skaiNative.toggleFloatingAssistant=function(){assistantAction("toggle")};
      skaiNative.aiStatus=function(){aiAction("status")};
      skaiNative.installLocalModel=function(){aiAction("install")};
      skaiNative.sendLocalAi=function(requestId,request){aiAction("send",String(requestId),String(request))};
    })();
  )",bridge);
}