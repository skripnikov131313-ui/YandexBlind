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

process.stdout.write("Sk/Ai UI invariants: OK\n");
