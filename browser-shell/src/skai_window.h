#pragma once
#include <windows.h>
#include <string>

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
