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
      {L"https://cdn.onetrust.com/scripttemplates/otSDKStub.js", true},
      {L"https://consentmanager.net/consent.js", true},
      {L"https://static.usercentrics.eu/browser-ui/latest/loader.js", true},
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
                 << L"\n";
                 << L"\n";
      return 1;
    }
  }

  struct ParameterCase {
    const wchar_t* input;
    const wchar_t* expected;
  };
  const ParameterCase parameter_cases[] = {
      {L"https://example.com/page?utm_source=mail&gclid=abc&lang=ru#section",
       L"https://example.com/page?lang=ru#section"},
      {L"https://example.com/?fbclid=123", L"https://example.com/"},
      {L"https://example.com/?%75tm_source=ads&keep=a%26b",
       L"https://example.com/?keep=a%26b"},
      {L"https://example.com/?foo=1&bar=2",
       L"https://example.com/?foo=1&bar=2"},
      {L"file:///C:/SkAi/ui/index.html?utm_source=local",
       L"file:///C:/SkAi/ui/index.html?utm_source=local"},
      {L"https://example.com/path?utm_campaign=spring#top",
       L"https://example.com/path#top"},
  };
  for (const auto& test : parameter_cases) {
    const auto actual = SkAiStripTrackingParameters(test.input);
    if (actual != test.expected) {
      std::wcerr << L"FAIL: URL cleanup for " << test.input
                 << L" expected " << test.expected
                 << L" but was " << actual << L"\n";
      return 1;
    }
  }

  struct CookieCase {
    const wchar_t* name;
    bool should_block;
  };
  const CookieCase cookie_cases[] = {
      {L"_ga", true},
      {L"_ga_ABCDEFG", true},
      {L"_gcl_au", true},
      {L"_fbp", true},
      {L"IDE", true},
      {L"hubspotutk", true},
      {L"ajs_anonymous_id", true},
      {L"ph_phc_test_posthog", true},
      {L"sessionid", false},
      {L"__Host-auth", false},
      {L"csrftoken", false},
      {L"language", false},
  };
  for (const auto& test : cookie_cases) {
    const bool actual = SkAiShouldBlockTrackingCookie(test.name);
    if (actual != test.should_block) {
      std::wcerr << L"FAIL: cookie " << test.name << L" expected "
                 << (test.should_block ? L"blocked" : L"allowed")
                 << L" but was " << (actual ? L"blocked" : L"allowed")
                 << L"\\n";
      return 1;
    }
  }

  std::cout << "Sk/Ai site tracking protection tests: "
            << (sizeof(cases) / sizeof(cases[0]) +
                sizeof(parameter_cases) / sizeof(parameter_cases[0]) +
                sizeof(cookie_cases) / sizeof(cookie_cases[0])) << " passed\n";
  return 0;
}
