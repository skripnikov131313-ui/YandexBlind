#include "skai_app.h"

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_parser.h"
#include "include/cef_v8.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <string>
#include <filesystem>
#include <windows.h>

namespace {
class SkAiV8Bridge final : public CefV8Handler {
 public:
  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value>,
               const CefV8ValueList& args,
               CefRefPtr<CefV8Value>&,
               CefString& exception) override {
    const auto context = CefV8Context::GetCurrentContext();
    if (!context) return false;
    const auto frame = context->GetFrame();
    if (!frame) return false;

    auto message = CefProcessMessage::Create("skai.command");
    const auto values = message->GetArgumentList();
    auto send = [&](const char* category, const std::wstring& action) {
      values->SetString(0, category);
      values->SetString(1, action);
      frame->SendProcessMessage(PID_BROWSER, message);
    };
    auto require_string = [&](std::size_t index, const wchar_t* label) {
      if (index >= args.size() || !args[index] || !args[index]->IsString()) {
        exception = label;
        return false;
      }
      return true;
    };

    if (name == "minimize" || name == "toggleMaximize" || name == "close" ||
        name == "beginDrag") {
      const std::wstring action =
          name == "beginDrag" ? L"drag" : name.ToWString();
      send("window", action);
      return true;
    }
    if (name == "toggleFloatingAssistant") {
      send("assistant", L"toggle");
      return true;
    }
    if (name == "shareCurrentPage") {
      send("pageContext", L"share");
      return true;
    }
    if (name == "navigate") {
      if (!require_string(0, L"URL required")) return true;
      send("navigate", args[0]->GetStringValue().ToWString());
      return true;
    }
    if (name == "back" || name == "forward" || name == "reload" ||
        name == "stop") {
      send("browser", name.ToWString());
      return true;
    }
    if (name == "browserAction") {
      if (!require_string(0, L"Browser action required")) return true;
      std::wstring parameter;
      if (args.size() > 1) {
        if (!args[1] || !args[1]->IsString()) {
          exception = L"Browser action parameter must be a string";
          return true;
        }
        parameter = args[1]->GetStringValue().ToWString();
      }
      values->SetString(0, "browser");
      values->SetString(1, args[0]->GetStringValue());
      values->SetString(2, parameter);
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "newTab" || name == "activateTab" || name == "closeTab") {
      const std::wstring value =
          args.empty() ? L"about:blank" :
          (args[0] && args[0]->IsString()
              ? args[0]->GetStringValue().ToWString() : L"");
      if (value.empty()) {
        exception = L"Tab value required";
        return true;
      }
      const std::wstring action =
          name == "newTab" ? L"new" :
          name == "activateTab" ? L"activate" : L"close";
      values->SetString(0, "tab");
      values->SetString(1, action);
      values->SetString(2, value);
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "downloadAction" || name == "openDownloadedFile") {
      if (!require_string(0, L"Download target required") ||
          !require_string(1, L"Download action required")) {
        return true;
      }
      values->SetString(0, name == "downloadAction" ? "download" : "downloadPath");
      values->SetString(1, args[1]->GetStringValue());
      values->SetString(2, args[0]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "resolvePermission") {
      if (!require_string(0, L"Permission ID required") ||
          !require_string(1, L"Permission action required")) {
        return true;
      }
      if (args.size() < 3 || !args[2] || !args[2]->IsBool()) {
        exception = L"Remember flag required";
        return true;
      }
      values->SetString(0, "permission");
      values->SetString(1, args[1]->GetStringValue());
      values->SetString(2, args[0]->GetStringValue());
      values->SetBool(3, args[2]->GetBoolValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "listSitePermissions") {
      send("permissionsList", L"list");
      return true;
    }
    if (name == "revokeSitePermission") {
      if (!require_string(0, L"Permission key required")) return true;
      values->SetString(0, "permissionRevoke");
      values->SetString(1, "revoke");
      values->SetString(2, args[0]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "listProfiles") {
      send("profilesList", L"list");
      return true;
    }
    if (name == "createProfile" || name == "switchProfile") {
      if (!require_string(0, L"Profile name required")) return true;
      values->SetString(0, name == "createProfile" ? "profileCreate" : "profileSwitch");
      values->SetString(1, name == "createProfile" ? "create" : "switch");
      values->SetString(2, args[0]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "setContentBounds") {
      if (args.size() < 4) {
        exception = L"Content bounds require x, y, width and height";
        return true;
      }
      std::wstring bounds;
      for (std::size_t index = 0; index < 4; ++index) {
        if (!args[index] ||
            (!args[index]->IsInt() && !args[index]->IsUInt() &&
             !args[index]->IsDouble())) {
          exception = L"Content bounds must be numeric";
          return true;
        }
        const double coordinate = args[index]->IsInt()
            ? static_cast<double>(args[index]->GetIntValue())
            : args[index]->IsUInt()
                ? static_cast<double>(args[index]->GetUIntValue())
                : args[index]->GetDoubleValue();
        if (!std::isfinite(coordinate) ||
            coordinate < static_cast<double>(std::numeric_limits<int>::min()) ||
            coordinate > static_cast<double>(std::numeric_limits<int>::max())) {
          exception = L"Content bounds are outside the supported range";
          return true;
        }
        if (!bounds.empty()) bounds += L",";
        bounds += std::to_wstring(static_cast<int>(coordinate));
      }
      values->SetString(0, "layout");
      values->SetString(1, bounds);
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }
    if (name == "aiStatus" || name == "installLocalModel") {
      send("ai", name == "aiStatus" ? L"status" : L"install");
      return true;
    }
    if (name == "sendLocalAi") {
      if (!require_string(0, L"AI request ID required") ||
          !require_string(1, L"AI request body required")) {
        return true;
      }
      values->SetString(0, "ai");
      values->SetString(1, "send");
      values->SetString(2, args[0]->GetStringValue());
      values->SetString(3, args[1]->GetStringValue());
      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(SkAiV8Bridge);
};

bool IsTrustedSkAiUiFrame(CefRefPtr<CefFrame> frame) {
  if (!frame || !frame->IsMain()) return false;

  CefURLParts parts;
  if (!CefParseURL(frame->GetURL(), parts)) return false;
  std::wstring scheme = CefString(&parts.scheme).ToWString();
  std::wstring host = CefString(&parts.host).ToWString();
  std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  if (scheme != L"file" || !host.empty()) return false;

  const auto actual_path = CefURIDecode(
      CefString(&parts.path), true,
      static_cast<cef_uri_unescape_rule_t>(UU_NORMAL | UU_PATH_SEPARATORS))
                               .ToWString();

  wchar_t executable[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;

  const auto ui_directory =
      std::filesystem::path(executable).parent_path() / L"ui";
  std::wstring expected_index =
      (ui_directory / L"index.html").generic_wstring();
  std::wstring expected_assistant =
      (ui_directory / L"assistant.html").generic_wstring();
  if (expected_index.empty() || expected_assistant.empty()) return false;
  if (expected_index.front() != L'/') expected_index.insert(expected_index.begin(), L'/');
  if (expected_assistant.front() != L'/') expected_assistant.insert(expected_assistant.begin(), L'/');

  auto normalize = [](std::wstring& value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) {
                     return ch == L'\\'
                         ? L'/'
                         : static_cast<wchar_t>(std::towlower(ch));
                   });
  };
  std::wstring trusted_index = expected_index;
  std::wstring trusted_assistant = expected_assistant;
  normalize(trusted_index);
  normalize(trusted_assistant);
  std::wstring decoded_path = actual_path;
  normalize(decoded_path);

  return decoded_path == trusted_index ||
         decoded_path == trusted_assistant;
}

}  // namespace

namespace {
bool IsHttpPageFrame(CefRefPtr<CefFrame> frame) {
  if (!frame) return false;
  CefURLParts parts;
  if (!CefParseURL(frame->GetURL(), parts)) return false;
  std::wstring scheme = CefString(&parts.scheme).ToWString();
  std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  return scheme == L"http" || scheme == L"https";
}

void InstallSitePrivacyCompatibility(CefRefPtr<CefV8Context> context) {
  // This shim provides local compatibility only. It never downloads an ad,
  // fires an impression/click beacon or claims that consent was granted.
  static constexpr char kShim[] = R"SKAI_PRIVACY(
(() => {
  if (window.__skaiPrivacyCompat) return;
  const state = Object.freeze({
    ads: "suppressed",
    adSlotResult: "empty",
    consent: "denied",
    telemetry: "blocked-by-browser-policy"
  });
  try {
    Object.defineProperty(window, "__skaiPrivacyCompat", {
      value: state, configurable: false, enumerable: false, writable: false
    });
  } catch (_) {}

  try {
    Object.defineProperty(Navigator.prototype, "globalPrivacyControl", {
      configurable: true, get: () => true
    });
  } catch (_) {}

  // IAB TCF compatibility reports denial, never fabricated consent.
  const deniedPurposes = Object.create(null);
  for (let id = 1; id <= 24; id++) deniedPurposes[id] = false;
  const deniedVendors = Object.create(null);
  const tcData = {
    gdprApplies: true,
    cmpLoaded: true,
    cmpStatus: "loaded",
    displayStatus: "hidden",
    eventStatus: "tcloaded",
    listenerId: 1,
    tcString: "",
    tcfPolicyVersion: 4,
    purpose: { consents: deniedPurposes, legitimateInterests: deniedPurposes },
    vendor: { consents: deniedVendors, legitimateInterests: deniedVendors },
    specialFeatureOptins: Object.create(null)
  };
  if (typeof window.__tcfapi !== "function") {
    const listeners = new Map();
    window.__tcfapi = (command, version, callback, parameter) => {
      if (typeof callback !== "function") return;
      if (command === "ping") {
        callback({
          gdprApplies: true, cmpLoaded: true, cmpStatus: "loaded",
          displayStatus: "hidden", apiVersion: "2.2", cmpId: 0,
          cmpVersion: 0, gvlVersion: 0, tcfPolicyVersion: 4
        }, true);
      } else if (command === "getTCData" || command === "getData") {
        callback({ ...tcData, listenerId: parameter || 1 }, true);
      } else if (command === "addEventListener") {
        const id = listeners.size + 1;
        listeners.set(id, callback);
        callback({ ...tcData, listenerId: id }, true);
      } else if (command === "removeEventListener") {
        callback(listeners.delete(parameter), true);
      } else {
        callback(null, false);
      }
    };
  }

  // GPT surface: slots can be registered, but all render results are empty.
  // No ad network request, creative, click or impression is generated.
  const existing = window.googletag && typeof window.googletag === "object"
    ? window.googletag : {};
  const slots = [];
  const listeners = new Map();
  const service = {
    addEventListener(name, fn) {
      if (typeof fn === "function") {
        const group = listeners.get(name) || [];
        group.push(fn);
        listeners.set(name, group);
      }
      return service;
    },
    removeEventListener(name, fn) {
      listeners.set(name, (listeners.get(name) || []).filter(item => item !== fn));
      return true;
    },
    getSlots: () => slots.slice(),
    setTargeting: () => service,
    clearTargeting: () => service,
    enableSingleRequest: () => service,
    collapseEmptyDivs: () => service,
    disableInitialLoad: () => service,
    refresh(targets) {
      const selected = Array.isArray(targets) ? targets : slots;
      for (const slot of selected) emitEmpty(slot);
    }
  };
  function emitEmpty(slot) {
    const event = {
      slot, isEmpty: true, size: null, advertiserId: null,
      campaignId: null, creativeId: null, lineItemId: null,
      serviceName: "publisher_ads", sourceAgnosticCreativeId: null,
      sourceAgnosticLineItemId: null
    };
    for (const fn of listeners.get("slotRenderEnded") || []) {
      try { fn(event); } catch (_) {}
    }
  }
  function makeSlot(path, sizes, id) {
    const slot = {
      getAdUnitPath: () => path || "",
      getSlotElementId: () => id || "",
      getSlotId: () => ({ getId: () => path || "", getName: () => path || "" }),
      addService: () => slot,
      setTargeting: () => slot,
      clearTargeting: () => slot,
      setCollapseEmptyDiv: () => slot,
      defineSizeMapping: () => slot,
      setCategoryExclusion: () => slot,
      setClickUrl: () => slot,
      getTargeting: () => [],
      getSizes: () => sizes || [],
      getResponseInformation: () => null
    };
    slots.push(slot);
    return slot;
  }
  existing.cmd = Array.isArray(existing.cmd) ? existing.cmd : [];
  const queued = existing.cmd.slice();
  existing.cmd.length = 0;
  existing.cmd.push = function(command) {
    if (typeof command === "function") {
      try { command(); } catch (_) {}
    }
    return 0;
  };
  existing.defineSlot = (path, sizes, id) => makeSlot(path, sizes, id);
  existing.defineOutOfPageSlot = (path, id) => makeSlot(path, [], id);
  existing.defineSizeMapping = () => ({ addSize: () => ({ build: () => [] }) });
  existing.pubads = () => service;
  existing.companionAds = () => service;
  existing.content = () => service;
  existing.display = slotOrId => {
    const slot = typeof slotOrId === "string"
      ? slots.find(item => item.getSlotElementId() === slotOrId) : slotOrId;
    if (slot) queueMicrotask(() => emitEmpty(slot));
  };
  existing.enableServices = () => true;
  existing.destroySlots = targets => {
    const remove = Array.isArray(targets) ? targets : slots.slice();
    for (const slot of remove) {
      const index = slots.indexOf(slot);
      if (index >= 0) slots.splice(index, 1);
    }
    return true;
  };
  window.googletag = existing;
  for (const command of queued) {
    if (typeof command === "function") {
      try { command(); } catch (_) {}
    }
  }

  // AdSense-compatible queue: accepting a push must not fetch or render ads.
  if (!Array.isArray(window.adsbygoogle)) window.adsbygoogle = [];
  const adsQueue = window.adsbygoogle;
  adsQueue.push = function() { return adsQueue.length; };

  // Hide common ad creatives and frames without hiding the surrounding page.
  const css = [
    "ins.adsbygoogle",
    "iframe[id^='google_ads_iframe']",
    "iframe[src*='doubleclick.net']",
    "iframe[src*='googlesyndication.com']",
    "[data-ad-client]",
    "[data-ad-slot]",
    "[aria-label='advertisement']",
    "[aria-label='Advertisement']"
  ].join(",") + "{display:none!important;visibility:hidden!important;pointer-events:none!important}";
  const installStyle = () => {
    const root = document.documentElement || document.head;
    if (!root || document.getElementById("__skai_privacy_style")) return;
    const style = document.createElement("style");
    style.id = "__skai_privacy_style";
    style.textContent = css;
    (document.head || root).appendChild(style);
  };
  installStyle();
  try {
    new MutationObserver(installStyle).observe(document, {
      childList: true, subtree: true
    });
  } catch (_) {}
})();
)SKAI_PRIVACY";

  if (!context || !context->Enter()) return;
  CefRefPtr<CefV8Value> result;
  CefRefPtr<CefV8Exception> exception;
  context->Eval(kShim, "skai://site-privacy-compat", 1, result, exception);
  context->Exit();
}
}  // namespace

void SkAiApp::OnContextCreated(CefRefPtr<CefBrowser>,
                               CefRefPtr<CefFrame> frame,
                               CefRefPtr<CefV8Context> context) {
  if (!context || !frame) return;

  if (!IsTrustedSkAiUiFrame(frame)) {
    if (IsHttpPageFrame(frame)) InstallSitePrivacyCompatibility(context);
    return;
  }

  const auto global = context->GetGlobal();
  if (!global) return;

  const auto bridge = new SkAiV8Bridge();
  const auto api = CefV8Value::CreateObject(nullptr, nullptr);
  const char* const functions[] = {
      "minimize", "toggleMaximize", "close", "beginDrag",
      "toggleFloatingAssistant", "shareCurrentPage", "navigate", "back", "forward",
      "reload", "stop", "browserAction", "newTab", "activateTab",
      "closeTab", "downloadAction", "openDownloadedFile",
      "resolvePermission", "listSitePermissions", "revokeSitePermission",
      "listProfiles", "createProfile", "switchProfile", "setContentBounds",
      "aiStatus", "installLocalModel", "sendLocalAi"};
  for (const char* function : functions) {
    api->SetValue(function, CefV8Value::CreateFunction(function, bridge),
                  V8_PROPERTY_ATTRIBUTE_NONE);
  }
  global->SetValue("skaiNative", api, V8_PROPERTY_ATTRIBUTE_NONE);
}
