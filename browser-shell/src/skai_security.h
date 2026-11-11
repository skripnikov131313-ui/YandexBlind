#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class SkAiSecuritySeverity : std::uint8_t {
  info = 0,
  low = 1,
  medium = 2,
  high = 3,
  critical = 4
};

enum class SkAiSecurityEventType : std::uint8_t {
  tracker_blocked = 0,
  permission_requested = 1,
  permission_allowed = 2,
  permission_denied = 3,
  download_started = 4,
  download_completed = 5,
  download_interrupted = 6,
  navigation_redirect = 7,
  suspicious_navigation = 8
};

struct SkAiNavigationRisk {
  bool detected = false;
  SkAiSecuritySeverity severity = SkAiSecuritySeverity::info;
  std::wstring evidence;
};

// Pure classifier: receives parsed URL components and never stores full URLs.
SkAiNavigationRisk SkAiClassifyNavigationRisk(
    const std::wstring& source_scheme, const std::wstring& target_scheme,
    const std::wstring& target_host, bool has_embedded_credentials,
    bool is_redirect);

struct SkAiSecurityEvent {
  std::uint64_t id = 0;
  std::int64_t timestamp_unix_ms = 0;
  SkAiSecurityEventType type = SkAiSecurityEventType::tracker_blocked;
  SkAiSecuritySeverity severity = SkAiSecuritySeverity::info;
  std::wstring origin;
  std::wstring summary;
  std::wstring evidence;
};

// The journal is process-local, bounded and contains no cookies, page text,
// credentials, full URLs, or local file contents.
std::uint64_t SkAiRecordSecurityEvent(
    SkAiSecurityEventType type, SkAiSecuritySeverity severity,
    const std::wstring& origin, const std::wstring& summary,
    const std::wstring& evidence = L"");
std::vector<SkAiSecurityEvent> SkAiGetSecurityEvents(std::size_t limit = 100);
void SkAiClearSecurityEvents();
