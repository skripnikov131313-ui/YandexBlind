#pragma once

#include <string>

// Returns true only for known third-party analytics, session-replay,
// advertising-beacon and fingerprinting service endpoints.
bool SkAiShouldBlockTrackingRequest(const std::wstring& url);

// Removes known marketing/click-attribution parameters without changing
// credentials, functional query parameters, path, origin or fragment.
std::wstring SkAiStripTrackingParameters(const std::wstring& url);

