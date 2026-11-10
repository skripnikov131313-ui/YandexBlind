#include "skai_window.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_values.h"
#include "include/cef_parser.h"
#include "skai_client.h"
#include <filesystem>
#include <map>
#include <string>

namespace {
constexpr wchar_t kClass[]=L"SkAi.NativeBrowserWindow";
constexpr UINT kLayout=WM_APP+20;
HWND g_window=nullptr;
HWND g_assistant_window=nullptr;
CefRefPtr<SkAiClient> g_ui_client;
CefRefPtr<SkAiClient> g_assistant_client;
struct Tab{int id;CefRefPtr<SkAiClient> client;bool active=false;};
std::map<int,Tab> g_tabs;
int g_next=1,g_active=0;
RECT g_bounds{0,100,800,600};
std::wstring UiFile(const wchar_t* file){
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
 auto path=std::filesystem::path(exe).parent_path()/L"ui"/file;
 return L"file:///"+path.generic_wstring();
}
void Layout(){
 if(!IsWindow(g_window))return;
 for(auto&[id,t]:g_tabs){auto b=t.client?t.client->GetBrowser():nullptr;if(!b)continue;
  HWND child=b->GetHost()->GetWindowHandle();if(!child)continue;
  SetWindowPos(child,t.active?HWND_TOP:HWND_BOTTOM,g_bounds.left,g_bounds.top,
   g_bounds.right-g_bounds.left,g_bounds.bottom-g_bounds.top,SWP_NOACTIVATE|(t.active?0:SWP_NOZORDER));
  ShowWindow(child,t.active?SW_SHOW:SW_HIDE);b->GetHost()->WasHidden(!t.active);
 }
}
void Snapshot(){
 if(!g_ui_client||!g_ui_client->GetBrowser())return;
 std::wstring j=L"[";
 for(auto it=g_tabs.begin();it!=g_tabs.end();++it){
  if(it!=g_tabs.begin())j+=L",";
  auto v=CefValue::Create();v->SetString(L"Вкладка "+std::to_wstring(it->first));
  j+=L"{\"id\":"+std::to_wstring(it->first)+L",\"active\":"+(it->second.active?L"true":L"false")+L",\"title\":"+CefWriteJSON(v,JSON_WRITER_DEFAULT).ToWString()+L"}";
 }
 j+=L"]";auto b=g_ui_client->GetBrowser();b->GetMainFrame()->ExecuteJavaScript(L"window.skaiNative&&window.skaiNative.syncTabs("+j+L");",b->GetMainFrame()->GetURL(),0);
}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_SIZE:Layout();return 0;
 case kLayout:Layout();return 0;
 case WM_NCHITTEST:{
  const LRESULT hit=DefWindowProcW(h,m,w,l);if(hit!=HTCLIENT)return hit;
  POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);
  RECT r{};GetClientRect(h,&r);const int edge=8;
  const bool left=p.x<edge,right=p.x>=r.right-edge,top=p.y<edge,bottom=p.y>=r.bottom-edge;
  if(top&&left)return HTTOPLEFT;if(top&&right)return HTTOPRIGHT;if(bottom&&left)return HTBOTTOMLEFT;if(bottom&&right)return HTBOTTOMRIGHT;
  if(left)return HTLEFT;if(right)return HTRIGHT;if(top)return HTTOP;if(bottom)return HTBOTTOM;return HTCLIENT;
 }
 case WM_ERASEBKGND:return 1;
 case WM_CLOSE:
  for(auto&[id,t]:g_tabs)if(t.client&&t.client->GetBrowser())t.client->GetBrowser()->GetHost()->CloseBrowser(false);
  if(g_ui_client&&g_ui_client->GetBrowser()){g_ui_client->GetBrowser()->GetHost()->CloseBrowser(false);return 0;}
  DestroyWindow(h);return 0;
 case WM_APP+11:
  if(h==g_assistant_window){g_assistant_window=nullptr;g_assistant_client=nullptr;return 0;}
  DestroyWindow(h);return 0;
 case WM_DESTROY:
  if(h==g_window){g_tabs.clear();g_ui_client=nullptr;PostQuitMessage(0);}
  return 0;
 default:return DefWindowProcW(h,m,w,l);
 }
}
}
void SkAiCreateTab(const std::wstring&url,bool activate){
 if(!g_window)return;int id=g_next++;Tab t{id,new SkAiClient(g_window,id,false),activate};
 if(activate){for(auto&[n,x]:g_tabs)x.active=false;g_active=id;}
 g_tabs.emplace(id,t);CefWindowInfo wi;wi.SetAsChild(g_window,g_bounds);CefBrowserSettings s;
 CefBrowserHost::CreateBrowser(wi,t.client,url,s,nullptr,nullptr);Snapshot();
}
void SkAiActivateTab(int id){
 if(id!=0&&!g_tabs.contains(id))return;for(auto&[n,t]:g_tabs)t.active=(n==id);g_active=id;Layout();Snapshot();
}
void SkAiOnTabClosed(int id){
 g_tabs.erase(id);if(g_active==id){g_active=g_tabs.empty()?0:g_tabs.rbegin()->first;SkAiActivateTab(g_active);}Snapshot();
}
void SkAiCloseTab(int id){
 auto i=g_tabs.find(id);if(i==g_tabs.end())return;
 if(i->second.client&&i->second.client->GetBrowser())i->second.client->GetBrowser()->GetHost()->CloseBrowser(false);
}
void SkAiNavigateActive(const std::wstring&url){
 auto i=g_tabs.find(g_active);if(i!=g_tabs.end()&&i->second.client&&i->second.client->GetBrowser())i->second.client->GetBrowser()->GetMainFrame()->LoadURL(url);
}
void SkAiBrowserAction(const std::wstring&a){
 auto i=g_tabs.find(g_active);if(i==g_tabs.end()||!i->second.client||!i->second.client->GetBrowser())return;
 auto b=i->second.client->GetBrowser();if(a==L"back"&&b->CanGoBack())b->GoBack();else if(a==L"forward"&&b->CanGoForward())b->GoForward();else if(a==L"reload")b->Reload();
}
void SkAiSetContentBounds(int x,int y,int w,int h){g_bounds={x,y,x+w,y+h};if(g_window)PostMessageW(g_window,kLayout,0,0);}
void SkAiNotifyTabState(int id,const std::wstring&t,const std::wstring&u,bool loading){
 if(!g_ui_client||!g_ui_client->GetBrowser())return;auto j=[](const std::wstring&s){auto v=CefValue::Create();v->SetString(s);return CefWriteJSON(v,JSON_WRITER_DEFAULT).ToWString();};
 auto b=g_ui_client->GetBrowser();b->GetMainFrame()->ExecuteJavaScript(L"window.skaiNative&&window.skaiNative.updateTab("+std::to_wstring(id)+L","+j(t)+L","+j(u)+L","+(loading?L"true":L"false")+L");",b->GetMainFrame()->GetURL(),0);
}
void SkAiSetFloatingAssistantHandle(HWND hwnd){g_assistant_window=hwnd;if(IsWindow(hwnd)){SetWindowPos(hwnd,HWND_TOPMOST,0,0,480,760,SWP_NOMOVE|SWP_SHOWWINDOW);ShowWindow(hwnd,SW_SHOW);}}
void SkAiNotifyTabFavicon(int id,const std::wstring&url){if(!g_ui_client||!g_ui_client->GetBrowser())return;auto v=CefValue::Create();v->SetString(url);auto j=CefWriteJSON(v,JSON_WRITER_DEFAULT).ToWString();auto b=g_ui_client->GetBrowser();b->GetMainFrame()->ExecuteJavaScript(L"window.skaiNative&&window.skaiNative.updateFavicon("+std::to_wstring(id)+L","+j+L");",b->GetMainFrame()->GetURL(),0);}
void SkAiToggleFloatingAssistant(){
 if(IsWindow(g_assistant_window)){ShowWindow(g_assistant_window,SW_SHOW);SetWindowPos(g_assistant_window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);return;}
 CefWindowInfo wi;wi.SetAsPopup(nullptr,L"Sk/Ai Assistant");CefBrowserSettings s;
 g_assistant_client=new SkAiClient(nullptr,-1,true);
 CefBrowserHost::CreateBrowser(wi,g_assistant_client,UiFile(L"assistant.html"),s,nullptr,nullptr);
}
HWND SkAiCreateMainWindow(HINSTANCE instance,int show){
 WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);wc.lpszClassName=kClass;RegisterClassExW(&wc);
 g_window=CreateWindowExW(0,kClass,L"Sk/Ai",WS_POPUP|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,1440,900,nullptr,nullptr,instance,nullptr);
 if(!g_window)return nullptr;g_ui_client=new SkAiClient(g_window,0,true);
 CefWindowInfo wi;RECT r{};GetClientRect(g_window,&r);wi.SetAsChild(g_window,r);CefBrowserSettings s;
 CefBrowserHost::CreateBrowser(wi,g_ui_client,UiFile(L"index.html"),s,nullptr,nullptr);
 ShowWindow(g_window,show);UpdateWindow(g_window);return g_window;
}
