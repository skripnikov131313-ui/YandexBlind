#pragma once

#include "include/cef_request_context_handler.h"

// Returns the request-context handler used by every persistent browser profile.
// It blocks known cross-site analytics/advertising endpoints, emits the
// Global Privacy Control signal, and applies Chromium's third-party cookie
// blocking preference once the context has initialized.
CefRefPtr<CefRequestContextHandler> SkAiCreatePrivacyRequestContextHandler();
