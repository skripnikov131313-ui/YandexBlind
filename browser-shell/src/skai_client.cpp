#include "skai_client.h"

void SkAiClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  browser_ = browser;
}
bool SkAiClient::DoClose(CefRefPtr<CefBrowser>) {
  return false;
}
void SkAiClient::OnBeforeClose(CefRefPtr<CefBrowser>) {
  browser_ = nullptr;
}
