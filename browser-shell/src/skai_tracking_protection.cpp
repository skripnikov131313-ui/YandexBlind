#include "skai_tracking_protection.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

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

namespace {
std::wstring NormalizeTrackingKey(std::wstring_view raw_key) {
  std::wstring decoded;
  decoded.reserve(raw_key.size());
  for (std::size_t i = 0; i < raw_key.size(); ++i) {
    wchar_t ch = raw_key[i];
    if (ch == L'%' && i + 2 < raw_key.size()) {
      const auto hex = [](wchar_t value) -> int {
        if (value >= L'0' && value <= L'9') return value - L'0';
        if (value >= L'a' && value <= L'f') return value - L'a' + 10;
        if (value >= L'A' && value <= L'F') return value - L'A' + 10;
        return -1;
      };
      const int high = hex(raw_key[i + 1]);
      const int low = hex(raw_key[i + 2]);
      if (high >= 0 && low >= 0) {
        decoded.push_back(static_cast<wchar_t>((high << 4) | low));
        i += 2;
        continue;
      }
    }
    decoded.push_back(ch == L'+' ? L' ' : ch);
  }
  std::transform(decoded.begin(), decoded.end(), decoded.begin(),
                 [](wchar_t value) {
                   return static_cast<wchar_t>(std::towlower(value));
                 });
  return decoded;
}

bool IsTrackingParameter(std::wstring_view raw_key) {
  static constexpr std::array<std::wstring_view, 35> kTrackingParameters = {
      L"utm_id", L"utm_source", L"utm_medium", L"utm_campaign",
      L"utm_term", L"utm_content", L"utm_source_platform",
      L"utm_creative_format", L"utm_marketing_tactic",
      L"gclid", L"dclid", L"gbraid", L"wbraid", L"fbclid",
      L"msclkid", L"ttclid", L"twclid", L"li_fat_id",
      L"mc_cid", L"mc_eid", L"igshid", L"yclid", L"ysclid",
      L"_ga", L"_gl", L"_gid", L"_gac", L"_fbp", L"_fbc",
      L"srsltid", L"epik", L"irclickid", L" ScCid", L"oly_anon_id",
      L"oly_enc_id"
  };
  const auto key = NormalizeTrackingKey(raw_key);
  return std::find(kTrackingParameters.begin(), kTrackingParameters.end(),
                   key) != kTrackingParameters.end();
}
}  // namespace

std::wstring SkAiStripTrackingParameters(const std::wstring& url) {
  const auto scheme_end = url.find(L"://");
  if (scheme_end == std::wstring::npos) return url;
  const auto scheme = std::wstring_view(url).substr(0, scheme_end);
  if (scheme != L"http" && scheme != L"https") return url;

  const auto query_start = url.find(L'?', scheme_end + 3);
  if (query_start == std::wstring::npos) return url;
  const auto fragment_start = url.find(L'#', query_start + 1);
  const auto query_end = fragment_start == std::wstring::npos
      ? url.size() : fragment_start;
  const auto query = std::wstring_view(url).substr(
      query_start + 1, query_end - query_start - 1);

  std::vector<std::wstring_view> kept;
  bool removed = false;
  std::size_t start = 0;
  while (start <= query.size()) {
    const auto amp = query.find(L'&', start);
    const auto end = amp == std::wstring_view::npos ? query.size() : amp;
    const auto pair = query.substr(start, end - start);
    const auto equals = pair.find(L'=');
    const auto key = pair.substr(0, equals);
    if (IsTrackingParameter(key)) {
      removed = true;
    } else {
      kept.push_back(pair);
    }
    if (amp == std::wstring_view::npos) break;
    start = amp + 1;
  }

  if (!removed) return url;

  std::wstring result = url.substr(0, query_start);
  for (const auto pair : kept) {
    if (result.back() != L'?') result.push_back(L'&');
    else if (!kept.empty() && &pair == nullptr) {}
    result.append(pair);
  }
  if (fragment_start != std::wstring::npos) {
    result.append(url.substr(fragment_start));
  }
  return result;
}
