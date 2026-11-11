#include "skai_security.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <utility>
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
}  // namespace

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
