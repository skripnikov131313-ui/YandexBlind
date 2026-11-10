#include "skai_client.h"
#include "skai_window.h"
#include "include/cef_parser.h"
#include "include/cef_values.h"
#include <string>
#include <sstream>

SkAiClient::SkAiClient(HWND host_window,int tab_id,bool ui_browser)
 :host_window_(host_window),tab_id_(tab_id),ui_browser_(ui_browser){}
bool SkAiClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,CefRefPtr<CefFrame>,CefProcessId source,CefRefPtr<CefProcessMessage> msg){
 if(!ui_browser_||source!=PID_RENDERER||msg->GetName()!="skai.command")return false;
 auto a=msg->GetArgumentList();auto category=a->GetString(0);auto action=a->GetString(1);
 if(category=="window"){
  if(!IsWindow(host_window_))return true;
  if(action=="minimize")ShowWindow(host_window_,SW_MINIMIZE);
  else if(action=="toggleMaximize")ShowWindow(host_window_,IsZoomed(host_window_)?SW_RESTORE:SW_MAXIMIZE);
  else if(action=="close")PostMessageW(host_window_,WM_CLOSE,0,0);
  else if(action=="drag"){ReleaseCapture();SendMessageW(host_window_,WM_NCLBUTTONDOWN,HTCAPTION,0);}
  return true;
 }
 if(category=="assistant"){SkAiToggleFloatingAssistant();return true;}
 if(category=="tab"){
  if(action=="new")SkAiCreateTab(a->GetString(2).ToWString(),true);
  else if(action=="activate")SkAiActivateTab(std::stoi(a->GetString(2).ToString()));
  else if(action=="close")SkAiCloseTab(std::stoi(a->GetString(2).ToString()));
  return true;
 }
 if(category=="layout"){
  std::wistringstream s(a->GetString(1).ToWString());int x=0,y=0,w=0,h=0;wchar_t c1=0,c2=0,c3=0;
  if(s>>x>>c1>>y>>c2>>w>>c3>>h&&c1==L','&&c2==L','&&c3==L',')SkAiSetContentBounds(x,y,w,h);
  return true;
 }
 if(!browser)return true;
 if(category=="navigate"){std::wstring u=action.ToWString();if(u.find(L"://")==std::wstring::npos)u=L"https://"+u;SkAiNavigateActive(u);return true;}
 if(category=="browser"){SkAiBrowserAction(action.ToWString());return true;}
 return false;
}
void SkAiClient::OnAfterCreated(CefRefPtr<CefBrowser>b){browser_=b;if(ui_browser_&&tab_id_==-1){HWND hwnd=b->GetHost()->GetWindowHandle();SkAiSetFloatingAssistantHandle(hwnd);host_window_=hwnd;}if(!ui_browser_){SkAiNotifyTabState(tab_id_,L"Новая вкладка",b->GetMainFrame()->GetURL(),false);SkAiActivateTab(tab_id_);}}
bool SkAiClient::DoClose(CefRefPtr<CefBrowser>){return false;}
void SkAiClient::OnBeforeClose(CefRefPtr<CefBrowser>){browser_=nullptr;if(!ui_browser_)SkAiOnTabClosed(tab_id_);else if(IsWindow(host_window_)){if(tab_id_==-1)SkAiSetFloatingAssistantHandle(nullptr);else PostMessageW(host_window_,WM_APP+11,0,0);}}
void SkAiClient::OnAddressChange(CefRefPtr<CefBrowser>,CefRefPtr<CefFrame>f,const CefString&u){
 if(!f->IsMain())return;
 if(ui_browser_){auto v=CefValue::Create();v->SetString(u);auto json=CefWriteJSON(v,JSON_WRITER_DEFAULT);f->ExecuteJavaScript(L"window.skaiNative&&window.skaiNative.addressChanged("+json.ToWString()+L");",f->GetURL(),0);}
 else SkAiNotifyTabState(tab_id_,L"Загрузка…",u.ToWString(),true);
}
void SkAiClient::OnTitleChange(CefRefPtr<CefBrowser>,const CefString&t){if(!ui_browser_)SkAiNotifyTabState(tab_id_,t.ToWString(),browser_?browser_->GetMainFrame()->GetURL().ToWString():L"",false);}
void SkAiClient::OnFaviconURLChange(CefRefPtr<CefBrowser>,const std::vector<CefString>&){}
