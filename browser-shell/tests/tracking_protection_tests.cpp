#include "skai_tracking_protection.h"

#include <iostream>

struct TestCase {
  const wchar_t* url;
  bool should_block;
};

int main() {
  const TestCase cases[] = {
      {L"https://www.google-analytics.com/g/collect?v=2", true},
      {L"https://metrics.hotjar.com/api/v1/track", true},
      {L"https://cdn.fullstory.com/s/fs.js", true},
      {L"https://scripts.clarity.ms/0.8.20/clarity.js", true},
      {L"https://api.fingerprintjs.com/", true},
      {L"https://fpjs.io/v3/agent", true},
      {L"https://www.facebook.com/tr/?id=123", true},
      {L"https://subdomain.google-analytics.com/collect", true},
      {L"https://www.example.com/", false},
      {L"https://example.com/scripts/analytics.js", false},
      {L"https://evilgoogle-analytics.com/collect", false},
      {L"https://not-hotjar.com/script.js", false},
      {L"https://www.facebook.com/", false},
      {L"https://www.facebook.com/login/", false},
      {L"https://www.facebook.com/tracking-help/", false},
      {L"file:///C:/SkAi/ui/index.html", false},
      {L"ftp://google-analytics.com/collect", false},
      {L"https://example.com/?next=https://hotjar.com", false},
  };

  for (const auto& test : cases) {
    const bool actual = SkAiShouldBlockTrackingRequest(test.url);
    if (actual != test.should_block) {
      std::wcerr << L"FAIL: " << test.url << L" expected "
                 << (test.should_block ? L"blocked" : L"allowed")
                 << L" but was " << (actual ? L"blocked" : L"allowed")
                 << L"\n";
      return 1;
    }
  }

  std::cout << "Sk/Ai site tracking protection tests: "
            << (sizeof(cases) / sizeof(cases[0])) << " passed\n";
  return 0;
}
