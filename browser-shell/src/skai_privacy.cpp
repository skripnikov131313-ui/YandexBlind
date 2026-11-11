#include <windows.h>

#include "skai_privacy.h"

#include "include/cef_parser.h"
#include "include/cef_request.h"
#include "include/cef_request_context.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_values.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace {
std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

bool ParseHttpUrl(const CefString& url, std::string& host,
                  std::string& path) {
  CefURLParts parts;
  if (!CefParseURL(url, parts)) return false;

  const std::string scheme = LowerAscii(CefString(&parts.scheme).ToString());
  if (scheme != "http" && scheme != "https") return false;

  host = LowerAscii(CefString(&parts.host).ToString());
  path = LowerAscii(CefString(&parts.path).ToString());
  while (!host.empty() && host.back() == '.') host.pop_back();
  return !host.empty();
}

bool IsDomainOrSubdomain(const std::string& host,
                         const std::string& registered_domain) {
  if (host == registered_domain) return true;
  if (host.size() <= registered_domain.size()) return false;

  const std::size_t suffix_start = host.size() - registered_domain.size();
  return host.compare(suffix_start, registered_domain.size(),
                      registered_domain) == 0 &&
         host[suffix_start - 1] == '.';
}

// This list intentionally targets third-party analytics, advertising,
// session-replay, attribution and pixel endpoints. It does not block broad
// platform domains such as google.com, facebook.com or linkedin.com.
bool IsKnownTrackerHost(const std::string& host) {
  static const char* const domains[] = {
      "google-analytics.com",
      "analytics.google.com",
      "doubleclick.net",
      "googlesyndication.com",
      "googletagservices.com",
      "adservice.google.com",
      "stats.g.doubleclick.net",
      "bat.bing.com",
      "clarity.ms",
      "static.hotjar.com",
      "script.hotjar.com",
      "insights.hotjar.com",
      "hotjar.io",
      "api.mixpanel.com",
      "api-eu.mixpanel.com",
      "cdn.mxpnl.com",
      "cdn-eu.mxpnl.com",
      "api.amplitude.com",
      "api2.amplitude.com",
      "cdn.amplitude.com",
      "heapanalytics.com",
      "rs.fullstory.com",
      "edge.fullstory.com",
      "contentsquare.net",
      "api.contentsquare.net",
      "t.contentsquare.net",
      "cdn.mouseflow.com",
      "tools.mouseflow.com",
      "luckyorange.com",
      "crazyegg.com",
      "static.chartbeat.com",
      "ping.chartbeat.net",
      "static.parsely.com",
      "srv.pixel.parsely.com",
      "scorecardresearch.com",
      "quantserve.com",
      "adroll.com",
      "criteo.com",
      "taboola.com",
      "outbrain.com",
      "amazon-adsystem.com",
      "adnxs.com",
      "rubiconproject.com",
      "pubmatic.com",
      "openx.net",
      "bidswitch.net",
      "casalemedia.com",
      "adsrvr.org",
      "serving-sys.com",
      "smartadserver.com",
      "2mdn.net",
      "demdex.net",
      "omtrdc.net",
      "everesttech.net",
      "bluekai.com",
      "krxd.net",
      "tapad.com",
      "agkn.com",
      "rlcdn.com",
      "ads-twitter.com",
      "analytics.twitter.com",
      "analytics.tiktok.com",
      "analytics-sg.tiktok.com",
      "pixel.tiktok.com",
      "pixel.wp.com",
      "stats.wp.com",
      "snap.licdn.com",
      "px.ads.linkedin.com",
      "browser.sentry-cdn.com",
      "ingest.sentry.io",
      "browser-intake-datadoghq.com",
      "datadoghq-browser-agent.com",
      "nr-data.net",
  };

  for (const char* domain : domains) {
    if (IsDomainOrSubdomain(host, domain)) return true;
  }
  return false;
}

std::string RegistrableDomain(const std::string& host) {
  // Preserve IP literals and single-label hosts.
  if (host.empty() || host.front() == '[' ||
      std::all_of(host.begin(), host.end(), [](unsigned char ch) {
        return (ch >= '0' && ch <= '9') || ch == '.';
      })) {
    return host;
  }

  const std::size_t last_dot = host.rfind('.');
  if (last_dot == std::string::npos) return host;
  const std::size_t previous_dot =
      last_dot == 0 ? std::string::npos : host.rfind('.', last_dot - 1);
  if (previous_dot == std::string::npos) return host;

  const std::string last_two = host.substr(previous_dot + 1);
  static const char* const multi_label_suffixes[] = {
      "co.uk", "org.uk", "ac.uk", "gov.uk",
      "com.au", "net.au", "org.au",
      "co.nz", "com.br", "com.mx", "com.sg",
      "co.jp", "co.in", "co.za", "com.tr",
      "com.tw", "com.hk",
  };
  for (const char* suffix : multi_label_suffixes) {
    if (last_two == suffix) {
      const std::size_t third_dot =
          previous_dot == 0 ? std::string::npos
                            : host.rfind('.', previous_dot - 1);
      return third_dot == std::string::npos
          ? host : host.substr(third_dot + 1);
    }
  }
  return last_two;
}

bool IsCrossSite(const std::string& resource_host,
                 const std::string& initiator_host) {
  if (resource_host.empty() || initiator_host.empty()) return false;
  return RegistrableDomain(resource_host) !=
         RegistrableDomain(initiator_host);
}

bool IsTrackingEndpointPath(const std::string& path) {
  static const char* const fragments[] = {
      "/tr/",
      "/collect",
      "/g/collect",
      "/j/collect",
      "/pixel",
      "/beacon",
      "/i/adsct",
      "/activityi",
      "/telemetry",
      "/analytics",
  };
  for (const char* fragment : fragments) {
    if (path.find(fragment) != std::string::npos) return true;
  }
  return false;
}

bool IsHighEntropyClientHint(const std::string& header_name) {
  static const char* const blocked_headers[] = {
      "sec-ch-ua-full-version",
      "sec-ch-ua-full-version-list",
      "sec-ch-ua-platform-version",
      "sec-ch-ua-arch",
      "sec-ch-ua-bitness",
      "sec-ch-ua-model",
      "sec-ch-ua-wow64",
      "sec-ch-ua-form-factors",
  };
  for (const char* name : blocked_headers) {
    if (header_name == name) return true;
  }
  return false;
}

void RemoveCrossSiteHighEntropyClientHints(CefRefPtr<CefRequest> request) {
  if (!request) return;
  CefRequest::HeaderMap original;
  request->GetHeaderMap(original);
  CefRequest::HeaderMap filtered;
  bool changed = false;
  for (const auto& header : original) {
    if (IsHighEntropyClientHint(LowerAscii(header.first.ToString()))) {
      changed = true;
      continue;
    }
    filtered.insert(header);
  }
  if (changed) request->SetHeaderMap(filtered);
}

bool IsTrackingQueryParameter(const std::string& raw_key) {
  const std::string key = LowerAscii(raw_key);
  if (key.rfind("utm_", 0) == 0) return true;

  static const char* const identifiers[] = {
      "gclid", "dclid", "gbraid", "wbraid", "gad_source",
      "gad_campaignid", "fbclid", "msclkid", "ttclid", "twclid",
      "yclid", "ymclid", "ysclid", "igshid", "epik", "srsltid",
      "li_fat_id", "mc_cid", "mc_eid", "vero_id", "oly_anon_id",
      "oly_enc_id", "rb_clickid", "irclickid", "sccid",
      "wickedid", "_gl", "_ga", "_gac",
  };
  for (const char* identifier : identifiers) {
    if (key == identifier) return true;
  }
  return false;
}

bool StripCrossSiteNavigationIdentifiers(CefRefPtr<CefRequest> request,
                                         const std::string& initiator_host) {
  if (!request || request->GetResourceType() != RT_MAIN_FRAME ||
      LowerAscii(request->GetMethod().ToString()) != "get") {
    return false;
  }

  CefURLParts parts;
  if (!CefParseURL(request->GetURL(), parts)) return false;

  const std::string target_host = LowerAscii(parts.host.ToString());
  if (target_host.empty() || !IsCrossSite(target_host, initiator_host)) {
    return false;
  }

  const std::string query = CefString(&parts.query).ToString();
  if (query.empty()) return false;

  std::string filtered_query;
  bool removed = false;
  std::size_t start = 0;
  while (start <= query.size()) {
    const std::size_t separator = query.find('&', start);
    const std::size_t end =
        separator == std::string::npos ? query.size() : separator;
    const std::string parameter = query.substr(start, end - start);
    const std::size_t equals = parameter.find('=');
    const std::string key = parameter.substr(0, equals);

    if (!parameter.empty() && IsTrackingQueryParameter(key)) {
      removed = true;
    } else if (!parameter.empty()) {
      if (!filtered_query.empty()) filtered_query.push_back('&');
      filtered_query += parameter;
    }

    if (separator == std::string::npos) break;
    start = separator + 1;
  }

  if (!removed) return false;
  CefString(&parts.query) = filtered_query;
  CefString rewritten_url;
  if (!CefCreateURL(parts, rewritten_url)) return false;
  request->SetURL(rewritten_url);
  return true;
}

bool ShouldBlockTrackerRequest(const CefString& request_url,
                               const CefString& request_initiator) {
  std::string resource_host;
  std::string resource_path;
  if (!ParseHttpUrl(request_url, resource_host, resource_path)) return false;

  if (request_initiator.empty()) return false;
  std::string initiator_host;
  std::string initiator_path;
  if (!ParseHttpUrl(request_initiator, initiator_host, initiator_path)) {
    return false;
  }
  if (!IsCrossSite(resource_host, initiator_host)) return false;

  // Known tracker hosts are blocked only when embedded or requested by a
  // different site. Direct navigation to the host and same-site resources
  // remain available.
  if (IsKnownTrackerHost(resource_host)) return true;

  // Generic pixel/beacon paths are blocked only when requested cross-site.
  // This avoids disabling a first-party API simply because its URL contains
  // words such as "collect" or "analytics".
  return IsTrackingEndpointPath(resource_path);
}

class SkAiPrivacyResourceRequestHandler final
    : public CefResourceRequestHandler {
 public:
  SkAiPrivacyResourceRequestHandler(bool block_request,
                                    const CefString& request_initiator)
      : block_request_(block_request),
        request_initiator_(request_initiator) {}

  ReturnValue OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> /*browser*/,
      CefRefPtr<CefFrame> /*frame*/,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> /*callback*/) override {
    if (!request) return RV_CANCEL;

    if (block_request_) return RV_CANCEL;

    // Global Privacy Control is a browser-level opt-out signal. It does not
    // replace request blocking and does not grant sites permission to track.
    std::string resource_host;
    std::string resource_path;
    if (ParseHttpUrl(request->GetURL(), resource_host, resource_path)) {
      request->SetHeaderByName("Sec-GPC", "1", true);
    }

    // Only strip high-entropy UA Client Hints on cross-site requests. Keep the
    // reduced User-Agent and low-entropy hints used by normal feature detection.
    if (!resource_host.empty() && !request_initiator_.empty()) {
      std::string initiator_host;
      std::string initiator_path;
      if (ParseHttpUrl(request_initiator_, initiator_host, initiator_path) &&
          IsCrossSite(resource_host, initiator_host)) {
        RemoveCrossSiteHighEntropyClientHints(request);
        StripCrossSiteNavigationIdentifiers(request, initiator_host);
      }
    }

    return RV_CONTINUE;
  }

 private:
  const bool block_request_;
  const CefString request_initiator_;
  IMPLEMENT_REFCOUNTING(SkAiPrivacyResourceRequestHandler);
};

class SkAiPrivacyRequestContextHandler final
    : public CefRequestContextHandler {
 public:
  void OnRequestContextInitialized(
      CefRefPtr<CefRequestContext> request_context) override {
    if (!request_context) return;

    // Chromium 154: CookieControlsMode::kBlockThirdParty has value 1.
    // First-party cookies remain available for normal site authentication.
    CefRefPtr<CefValue> value = CefValue::Create();
    value->SetInt(1);
    CefString error;
    if (!request_context->SetPreference(
            "profile.cookie_controls_mode", value, error)) {
      const std::wstring message =
          L"Sk/Ai: third-party cookie blocking preference could not be set: " +
          error.ToWString() + L"\n";
      OutputDebugStringW(message.c_str());
    }
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> /*browser*/,
      CefRefPtr<CefFrame> /*frame*/,
      CefRefPtr<CefRequest> request,
      bool /*is_navigation*/,
      bool /*is_download*/,
      const CefString& request_initiator,
      bool& disable_default_handling) override {
    disable_default_handling = false;
    if (!request) return nullptr;

    const bool block = ShouldBlockTrackerRequest(
        request->GetURL(), request_initiator);

    // Return a handler for all requests so Sec-GPC is applied consistently.
    return new SkAiPrivacyResourceRequestHandler(block, request_initiator);
  }

 private:
  IMPLEMENT_REFCOUNTING(SkAiPrivacyRequestContextHandler);
};
}  // namespace

CefRefPtr<CefRequestContextHandler>
SkAiCreatePrivacyRequestContextHandler() {
  return new SkAiPrivacyRequestContextHandler();
}
