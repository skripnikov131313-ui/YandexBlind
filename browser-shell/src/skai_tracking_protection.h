#pragma once

#include <string>

// Returns true only for known third-party analytics, session-replay,
// advertising-beacon and fingerprinting service endpoints.
bool SkAiShouldBlockTrackingRequest(const std::wstring& url);
