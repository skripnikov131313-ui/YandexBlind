#include "skai_tracking_protection.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>
#include <string_view>

namespace {
constexpr std::array<std::wstring_view, 52> kBlockedTrackerDomains = {
    L"google-analytics.com",
    L"googletagmanager.com",
    L"googlesyndication.com",
    L"doubleclick.net",
    L"googleadservices.com",
    L"adservice.google.com",
    L"hotjar.com",
    L"hotjar.io",
    L"fullstory.com",
    L"clarity.ms",
    L"mouseflow.com",
    L"smartlook.com",
    L"logrocket.io",
    L"lr-ingest.io",
    L"contentsquare.net",
    L"quantummetric.com",
    L"glassboxdigital.io",
    L"heap.io",
    L"heapanalytics.com",
    L"segment.io",
    L"segment.com",
    L"mixpanel.com",
    L"amplitude.com",
    L"rudderstack.com",
    L"posthog.com",
    L"matomo.cloud",
    L"piwik.pro",
    L"bat.bing.com",
    L"px.ads.linkedin.com",
    L"snap.licdn.com",
    L"analytics.tiktok.com",
    L"branch.io",
    L"adnxs.com",
    L"criteo.com",
    L"criteo.net",
    L"taboola.com",
    L"outbrain.com",
    L"scorecardresearch.com",
    L"quantserve.com",
    L"demdex.net",
    L"bluekai.com",
    L"krxd.net",
    L"adsrvr.org",
    L"pubmatic.com",
    L"rubiconproject.com",
    L"openx.net",
    L"fingerprintjs.com",
    L"fingerprint.com",
    L"fpjs.io",
    L"fpcdn.io",
    L"openfpcdn.io",
    L"deviceatlas.com"
};

bool MatchesDomain(std::wstring_view host, std::wstring_view domain) {
  if (host == domain) return true;
  if (host.size() <= domain.size()) return false;
  const auto offset = host.size() - domain.size();
  return host[offset - 1] == L'.' && host.substr(offset) == domain;
}

bool ExtractHostAndPath(const std::wstring& url,
                        std::wstring& host,
                        std::wstring& path) {
  const auto scheme_end = url.find(L"://");
  if (scheme_end == std::wstring::npos) return false;
  const auto scheme = std::wstring_view(url).substr(0, scheme_end);
  if (scheme != L"http" && scheme != L"https") return false;

  const std::size_t authority_start = scheme_end + 3;
  const auto authority_end = url.find_first_of(L"/?#", authority_start);
  const auto end = authority_end == std::wstring::npos
      ? url.size() : authority_end;
  if (end <= authority_start) return false;

  std::wstring authority = url.substr(authority_start, end - authority_start);
  const auto at = authority.rfind(L'@');
  if (at != std::wstring::npos) authority.erase(0, at + 1);
  if (authority.empty()) return false;

  if (authority.front() == L'[') {
    const auto bracket = authority.find(L']');
    if (bracket == std::wstring::npos) return false;
    host = authority.substr(1, bracket - 1);
  } else {
    const auto port = authority.find(L':');
    host = authority.substr(0, port);
  }
  if (!host.empty() && host.back() == L'.') host.pop_back();
  if (host.empty()) return false;

  path.clear();
  if (authority_end != std::wstring::npos && url[authority_end] == L'/') {
    const auto path_end = url.find_first_of(L"?#", authority_end);
    path = url.substr(authority_end,
                      path_end == std::wstring::npos
                          ? std::wstring::npos : path_end - authority_end);
  }
  return true;
}
}  // namespace

bool SkAiShouldBlockTrackingRequest(const std::wstring& raw_url) {
  if (raw_url.empty()) return false;
  std::wstring url = raw_url;
  std::transform(url.begin(), url.end(), url.begin(),
                 [](wchar_t value) {
                   return static_cast<wchar_t>(std::towlower(value));
                 });

  std::wstring host;
  std::wstring path;
  if (!ExtractHostAndPath(url, host, path)) return false;

  for (const auto domain : kBlockedTrackerDomains) {
    if (MatchesDomain(host, domain)) return true;
  }

  // Facebook's /tr endpoint is a tracking pixel. Do not block the whole
  // facebook.com host because ordinary navigation and login must keep working.
  if ((host == L"facebook.com" || host == L"www.facebook.com") &&
      (path == L"/tr" || path.rfind(L"/tr/", 0) == 0)) {
    return true;
  }
  return false;
}
