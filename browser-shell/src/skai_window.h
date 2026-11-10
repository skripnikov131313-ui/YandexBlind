#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include "include/cef_download_handler.h"
#include "include/cef_download_item.h"
#include "include/cef_permission_handler.h"

HWND SkAiCreateMainWindow(HINSTANCE instance, int show_command);

void SkAiCreateTab(const std::wstring& url, bool activate);
void SkAiActivateTab(int tab_id);
void SkAiCloseTab(int tab_id);
void SkAiOnTabClosed(int tab_id);
void SkAiNavigateActive(const std::wstring& url);
void SkAiBrowserAction(const std::wstring& action);
void SkAiSetContentBounds(int x, int y, int width, int height);

void SkAiNotifyTabState(int tab_id, const std::wstring& title,
                        const std::wstring& url, bool loading);
void SkAiNotifyTabFavicon(int tab_id, const std::wstring& icon_url);
void SkAiNotifyNavigationState(int tab_id, bool can_go_back,
                               bool can_go_forward, bool is_loading);

void SkAiToggleFloatingAssistant();
void SkAiSetFloatingAssistantHandle(HWND hwnd);

// CEF browser lifecycle coordination. Returns true from OnBrowserCreated
// when the newly-created browser must be closed because shutdown has started.
bool SkAiOnBrowserCreated();
void SkAiOnUiClosed();
void SkAiOnAssistantClosed();

void SkAiNotifyDownloadState(CefRefPtr<CefDownloadItem> item,
                             CefRefPtr<CefDownloadItemCallback> callback);
void SkAiDownloadAction(std::uint32_t download_id, const std::wstring& action);

void SkAiOpenDownloadedFile(const std::wstring& path, const std::wstring& action);

std::uint64_t SkAiRequestSitePermission(
    int tab_id, const std::wstring& requesting_origin,
    std::uint32_t requested_permissions, bool media_request,
    CefRefPtr<CefPermissionPromptCallback> prompt_callback,
    CefRefPtr<CefMediaAccessCallback> media_callback,
    std::uint64_t cef_prompt_id = 0);
void SkAiResolvePermission(std::uint64_t request_id, bool allow, bool remember);
void SkAiDismissPermission(std::uint64_t cef_prompt_id);
void SkAiCancelAllPermissions();

void SkAiCancelPermissionsForTab(int tab_id);
