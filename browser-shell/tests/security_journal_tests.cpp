#include "skai_security.h"

#include <iostream>
#include <string>

int main() {
  SkAiClearSecurityEvents();
  const auto first = SkAiRecordSecurityEvent(
      SkAiSecurityEventType::tracker_blocked, SkAiSecuritySeverity::low,
      L"https://metrics.example", L"Заблокирован запрос трекера",
      L"Сработало правило списка доменов");
  if (first == 0) return 1;

  const auto events = SkAiGetSecurityEvents(10);
  if (events.size() != 1 || events.front().id != first ||
      events.front().type != SkAiSecurityEventType::tracker_blocked ||
      events.front().severity != SkAiSecuritySeverity::low) {
    std::cerr << "Security event round-trip failed\n";
    return 1;
  }

  SkAiRecordSecurityEvent(SkAiSecurityEventType::permission_requested,
      SkAiSecuritySeverity::medium, L"https://example.test",
      L"Сайт запросил разрешение", L"Камера");
  const auto latest = SkAiGetSecurityEvents(1);
  if (latest.size() != 1 ||
      latest.front().type != SkAiSecurityEventType::permission_requested) {
    std::cerr << "Security event limit/order failed\n";
    return 1;
  }

  for (int i = 0; i < 250; ++i) {
    SkAiRecordSecurityEvent(SkAiSecurityEventType::download_started,
        SkAiSecuritySeverity::info, L"https://download.example",
        L"Загрузка начата");
  }
  const auto bounded = SkAiGetSecurityEvents(1000);
  if (bounded.size() != 200) {
    std::cerr << "Security journal must retain at most 200 events\n";
    return 1;
  }
  for (std::size_t i = 1; i < bounded.size(); ++i) {
    if (bounded[i].id <= bounded[i - 1].id) {
      std::cerr << "Security event IDs must be monotonic\n";
      return 1;
    }
  }

  SkAiClearSecurityEvents();
  if (!SkAiGetSecurityEvents().empty()) return 1;
  std::cout << "Sk/Ai security journal tests: passed\n";
  return 0;
}
