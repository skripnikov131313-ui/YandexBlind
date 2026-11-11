#include "skai_security.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <utility>
#include <cwctype>
#include <deque>

namespace {
constexpr std::size_t kMaxEvents = 200;
constexpr std::size_t kMaxOrigin = 256;
constexpr std::size_t kMaxSummary = 240;
constexpr std::size_t kMaxEvidence = 512;

std::mutex g_mutex;
std::deque<SkAiSecurityEvent> g_events;
std::uint64_t g_next_id = 1;

std::wstring Bounded(std::wstring value, std::size_t maximum) {
  if (value.size() > maximum) value.resize(maximum);
  for (auto& ch : value) {
    if (ch < 0x20 && ch != L'\t') ch = L' ';
  }
  return value;
}
bool HasPunycodeLabel(const std::wstring& host) {
  std::size_t start = 0;
  while (start < host.size()) {
    const auto end = host.find(L'.', start);
    const auto length = (end == std::wstring::npos ? host.size() : end) - start;
    if (length > 4 && host.compare(start, 4, L"xn--") == 0) return true;
    if (end == std::wstring::npos) break;
    start = end + 1;
  }
  return false;
}
}  // namespace

SkAiNavigationRisk SkAiClassifyNavigationRisk(
    const std::wstring& source_scheme, const std::wstring& target_scheme,
    const std::wstring& target_host, bool has_embedded_credentials,
    bool is_redirect) {
  SkAiNavigationRisk result;
  std::wstring source = source_scheme;
  std::wstring target = target_scheme;
  std::wstring host = target_host;
  std::transform(source.begin(), source.end(), source.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  std::transform(target.begin(), target.end(), target.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  std::transform(host.begin(), host.end(), host.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });

  if ((target != L"http" && target != L"https") || host.empty()) return result;
  if (has_embedded_credentials) {
    result.detected = true;
    result.severity = SkAiSecuritySeverity::high;
    result.evidence = L"В адресе присутствуют встроенные учётные данные";
  } else if (is_redirect && source == L"https" && target == L"http") {
    result.detected = true;
    result.severity = SkAiSecuritySeverity::high;
    result.evidence = L"Перенаправление понижает защищённое HTTPS-соединение до HTTP";
  } else if (HasPunycodeLabel(host)) {
    result.detected = true;
    result.severity = SkAiSecuritySeverity::medium;
    result.evidence = L"Домен содержит IDN punycode; проверьте отображаемое имя";
  }
  return result;
}

std::uint64_t SkAiRecordSecurityEvent(
    SkAiSecurityEventType type, SkAiSecuritySeverity severity,
    const std::wstring& origin, const std::wstring& summary,
    const std::wstring& evidence) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

  std::lock_guard<std::mutex> lock(g_mutex);
  SkAiSecurityEvent event;
  event.id = g_next_id++;
  event.timestamp_unix_ms = timestamp;
  event.type = type;
  event.severity = severity;
  event.origin = Bounded(origin, kMaxOrigin);
  event.summary = Bounded(summary, kMaxSummary);
  event.evidence = Bounded(evidence, kMaxEvidence);
  g_events.push_back(std::move(event));
  while (g_events.size() > kMaxEvents) g_events.pop_front();
  return g_events.back().id;
}

std::vector<SkAiSecurityEvent> SkAiGetSecurityEvents(std::size_t limit) {
  std::lock_guard<std::mutex> lock(g_mutex);
  const auto count = std::min(limit, g_events.size());
  std::vector<SkAiSecurityEvent> result;
  result.reserve(count);
  const auto start = g_events.size() - count;
  for (std::size_t i = start; i < g_events.size(); ++i) {
    result.push_back(g_events[i]);
  }
  return result;
}

void SkAiClearSecurityEvents() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_events.clear();
}
