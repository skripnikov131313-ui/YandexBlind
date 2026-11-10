import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

const source = readFileSync(
  new URL("../ui/app.js", import.meta.url),
  "utf8"
);

// A single querySelector result is one Element, not an array. Collection
// operations must use the $$ helper (querySelectorAll).
const singleElementCollectionCall =
  /(?<!\$)\$\([^)]*\)(?:\s*\n\s*)*\.(?:forEach|filter|map|find|some|every|reduce)\b/;
assert.doesNotMatch(
  source,
  singleElementCollectionCall,
  "Use $$() before calling collection methods on DOM selections."
);

// Native CEF is the only owner of tab IDs. A stale JS counter assignment
// would throw in strict mode and break profile/session restoration.
assert.doesNotMatch(source, /\btabSequence\b/, "Legacy JS tab counter must not return.");
assert.match(
  source,
  /function restoreProfileSession\(\)[\s\S]*?\$\$\("\.tab"\)\.forEach/,
  "Profile restore must iterate over all tab elements."
);
assert.match(
  source,
  /function createTab\([\s\S]*?skaiNative\?\.newTab\(url\)/,
  "New tabs must be created by the native CEF controller."
);

const assistantSource = readFileSync(
  new URL("../ui/assistant.js", import.meta.url),
  "utf8"
);
const nativeWindowSource = readFileSync(
  new URL("../src/skai_window.cpp", import.meta.url),
  "utf8"
);

assert.match(
  assistantSource,
  /skai\.conversation\.v1\."\+activeProfile/,
  "Detached assistant conversation storage must be profile-scoped."
);
assert.match(
  assistantSource,
  /skai\.assistant\.mode\.v1\."\+activeProfile/,
  "Detached assistant mode storage must be profile-scoped."
);
assert.match(
  source,
  /window\.addEventListener\("storage"/,
  "Main assistant must listen for cross-window state updates."
);
assert.match(
  nativeWindowSource,
  /UiFile\(L"assistant\.html"\) \+ L"\?profile=" \+ g_active_profile/,
  "Detached assistant must receive the active profile identifier."
);
assert.match(
  nativeWindowSource,
  /g_assistant_client->RequestClose\(\);/,
  "Profile switching must close the detached assistant."
);

const aiSource = readFileSync(
  new URL("../src/skai_ai.cpp", import.meta.url),
  "utf8"
);
const aiHeaderSource = readFileSync(
  new URL("../src/skai_ai.h", import.meta.url),
  "utf8"
);
const clientSource = readFileSync(
  new URL("../src/skai_client.cpp", import.meta.url),
  "utf8"
);
const bridgeSource = readFileSync(
  new URL("../src/skai_app.cpp", import.meta.url),
  "utf8"
);
const cmakeSource = readFileSync(
  new URL("../CMakeLists.txt", import.meta.url),
  "utf8"
);
const workflowSource = readFileSync(
  new URL("../../.github/workflows/native-shell-windows.yml", import.meta.url),
  "utf8"
);

assert.match(
  aiSource,
  /qwen3\.5-2b-Q4_K_M\.gguf/,
  "The local model filename must be pinned."
);
assert.match(
  aiSource,
  /9a3d0aa6c920f096441139b6a254542da855223fdad06855ea00c6c087fd713d/,
  "The verified model SHA-256 must remain pinned."
);
assert.match(
  aiSource,
  /https:\/\/|kModelUrlHost\[\] = L"huggingface\.co"/,
  "The model downloader must use HTTPS."
);
assert.match(
  aiSource,
  /--host 127\.0\.0\.1/,
  "The inference server must bind only to loopback."
);
assert.doesNotMatch(
  aiSource,
  /--host 0\.0\.0\.0/,
  "The inference server must never bind to all network interfaces."
);
assert.match(
  aiSource,
  /WINHTTP_ACCESS_TYPE_NO_PROXY/,
  "Requests to the local inference endpoint must bypass proxies."
);
assert.match(
  aiSource,
  /BCRYPT_SHA256_ALGORITHM/,
  "The native installer must verify the model with Windows SHA-256."
);
assert.match(
  clientSource,
  /SkAiAiSendChat\(browser, args->GetString\(2\)\.ToWString\(\),\s*args->GetString\(3\)\.ToWString\(\)\)/,
  "CEF browser commands must route local chat into the native AI module."
);
assert.match(
  bridgeSource,
  /skaiNative\.sendLocalAi=function\(requestId,request\)\{aiAction\("send",String\(requestId\),String\(request\)\)\}/,
  "The renderer bridge must expose the native local chat action."
);
assert.match(
  aiHeaderSource,
  /void SkAiAiShutdown\(\)/,
  "The local runtime must have a shutdown entry point."
);
assert.match(
  cmakeSource,
  /src\/skai_ai\.cpp/,
  "The native local AI implementation must be part of the executable target."
);
assert.match(
  workflowSource,
  /Build pinned llama\.cpp CPU runtime/,
  "Windows CI must build the pinned local inference runtime."
);
assert.match(
  workflowSource,
  /runtime\/llama\/llama-server\.exe/,
  "The Windows package must include llama-server.exe."
);
assert.match(
  source,
  /window\.skaiNative\.sendLocalAi\(requestId, JSON\.stringify\(payload\)\)/,
  "The docked assistant must send local requests through the native bridge."
);
assert.match(
  assistantSource,
  /window\.skaiNative\.sendLocalAi\(requestId, JSON\.stringify\(makeLocalRequest\(\)\)\)/,
  "The floating assistant must use the same native local inference bridge."
);
assert.match(
  source,
  /Облачный провайдер пока не подключён\. Запрос и контекст не отправлены\./,
  "Cloud mode must remain non-transmitting until a provider and consent flow exist."
);

assert.match(
  source,
  /localAiPendingId = requestId/,
  "The docked assistant must correlate each completion to its request."
);
assert.match(
  source,
  /localAiPendingId = ""/,
  "Profile changes must discard the previous pending response identity."
);
assert.match(
  assistantSource,
  /requestPending !== id/,
  "The floating assistant must ignore stale completions."
);

process.stdout.write("Sk/Ai UI invariants: OK\n");
