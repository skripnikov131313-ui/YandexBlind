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
  const auto normalHttps = SkAiClassifyNavigationRisk(
      L"https", L"https", L"shop.example", false, false);
  if (normalHttps.detected) {
    std::cerr << "Ordinary HTTPS navigation must not be flagged\\n";
    return 1;
  }
  const auto normalHttp = SkAiClassifyNavigationRisk(
      L"https", L"http", L"legacy.example", false, false);
  if (normalHttp.detected) {
    std::cerr << "Direct HTTP navigation is not an HTTPS downgrade redirect\\n";
    return 1;
  }
  const auto downgrade = SkAiClassifyNavigationRisk(
      L"https", L"http", L"legacy.example", false, true);
  if (!downgrade.detected || downgrade.severity != SkAiSecuritySeverity::high) {
    std::cerr << "HTTPS-to-HTTP redirect must be high severity\\n";
    return 1;
  }
  const auto credentials = SkAiClassifyNavigationRisk(
      L"https", L"https", L"login.example", true, false);
  if (!credentials.detected || credentials.severity != SkAiSecuritySeverity::high) {
    std::cerr << "Embedded URL credentials must be high severity\\n";
    return 1;
  }
  const auto punycode = SkAiClassifyNavigationRisk(
      L"https", L"https", L"shop.XN--P1AI", false, false);
  if (!punycode.detected || punycode.severity != SkAiSecuritySeverity::medium) {
    std::cerr << "Punycode labels must be detected case-insensitively\\n";
    return 1;
  }
  const auto invalidScheme = SkAiClassifyNavigationRisk(
      L"https", L"javascript", L"xn--p1ai", false, true);
  if (invalidScheme.detected) {
    std::cerr << "Non-web schemes must not be classified as web navigation risks\\n";
    return 1;
  }
  const auto embeddedLabel = SkAiClassifyNavigationRisk(
      L"https", L"https", L"xn--p1ai.example", false, false);
  if (!embeddedLabel.detected) {
    std::cerr << "Punycode in a hostname label must be detected\\n";
    return 1;
  }

  const auto shortRedirectChain = SkAiClassifyRedirectChainRisk(7);
  if (shortRedirectChain.detected) {
    std::cerr << "Short redirect chains must not be flagged\n";
    return 1;
  }
  const auto longRedirectChain = SkAiClassifyRedirectChainRisk(8);
  if (!longRedirectChain.detected ||
      longRedirectChain.severity != SkAiSecuritySeverity::medium) {
    std::cerr << "Eight redirects must produce a medium advisory signal\n";
    return 1;
  }
  const auto extremeRedirectChain = SkAiClassifyRedirectChainRisk(16);
  if (!extremeRedirectChain.detected ||
      extremeRedirectChain.severity != SkAiSecuritySeverity::high) {
    std::cerr << "Sixteen redirects must produce a high advisory signal\n";
    return 1;
  }

  std::cout << "Sk/Ai security journal tests: passed\n";
  return 0;
}
