#pragma once
#include <windows.h>
#include <memory>
#include <string>
#include <vector>

#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_find_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_download_handler.h"
#include "include/cef_permission_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_request.h"

CefRefPtr<CefResourceRequestHandler> SkAiCreateTrackingResourceHandler();

class SkAiClient final : public CefClient,
                         public CefLifeSpanHandler,
                         public CefDisplayHandler,
                         public CefFindHandler,
                         public CefLoadHandler,
                         public CefDownloadHandler,
                         public CefPermissionHandler,
                         public CefRequestHandler {
 public:
  SkAiClient(HWND host_window, int tab_id, bool ui_browser);
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefFindHandler> GetFindHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
  CefRefPtr<CefPermissionHandler> GetPermissionHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request, bool is_navigation, bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override;
  bool OnRequestMediaAccessPermission(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame, const CefString& requesting_origin,
      uint32_t requested_permissions,
      CefRefPtr<CefMediaAccessCallback> callback) override;
  bool OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser,
      uint64_t prompt_id, const CefString& requesting_origin,
      uint32_t requested_permissions,
      CefRefPtr<CefPermissionPromptCallback> callback) override;
  void OnDismissPermissionPrompt(CefRefPtr<CefBrowser> browser,
      uint64_t prompt_id, cef_permission_request_result_t result) override;
  bool OnBeforeDownload(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item, const CefString& suggested_name,
      CefRefPtr<CefBeforeDownloadCallback> callback) override;
  void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      CefRefPtr<CefDownloadItemCallback> callback) override;
  void OnFindResult(CefRefPtr<CefBrowser> browser, int identifier,
                    int count, const CefRect& selection_rect,
                    int active_match_ordinal, bool final_update) override;
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool is_loading,
      bool can_go_back, bool can_go_forward) override;
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame, CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame, const CefString& url) override;
  void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
  void OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
      const std::vector<CefString>& icon_urls) override;
  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
  bool is_closing() const { return closing_; }
  void RequestClose();
  int tab_id() const { return tab_id_; }
  bool is_ui_browser() const { return ui_browser_; }
 private:
  HWND host_window_ = nullptr;
  int tab_id_ = 0;
  bool ui_browser_ = false;
  bool closing_ = false;
  bool close_sent_ = false;
  CefRefPtr<CefBrowser> browser_;
  IMPLEMENT_REFCOUNTING(SkAiClient);
};
