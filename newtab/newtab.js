(() => {
  const engines = {
    yandex: { name: "Яндекс", url: "https://yandex.ru/search/?text=" },
    google: { name: "Google", url: "https://www.google.com/search?q=" },
    bing: { name: "Bing", url: "https://www.bing.com/search?q=" },
    duckduckgo: { name: "DuckDuckGo", url: "https://duckduckgo.com/?q=" },
    brave: { name: "Brave Search", url: "https://search.brave.com/search?q=" },
    startpage: { name: "Startpage", url: "https://www.startpage.com/sp/search?query=" }
  };
  const $ = id => document.getElementById(id);
  const read = (key, fallback) => { try { return localStorage.getItem("yb." + key) || fallback; } catch { return fallback; } };
  const write = (key, value) => { try { localStorage.setItem("yb." + key, value); } catch {} };
  let engine = engines[read("engine", "yandex")] ? read("engine", "yandex") : "yandex";
  let theme = read("theme", "dark");
  let motion = read("motion", "on") !== "off";
  const applyTheme = () => {
    const light = theme === "light" || (theme === "system" && matchMedia("(prefers-color-scheme: light)").matches);
    document.body.classList.toggle("light", light);
    $("theme-select").value = theme;
  };
  const applyMotion = () => {
    document.body.classList.toggle("no-motion", !motion);
    $("motion-toggle").checked = motion;
  };
  const updateEngine = () => {
    $("engine-label").textContent = engines[engine].name;
    $("engine-quick").innerHTML = engines[engine].name + " <span>⌄</span>";
    $("engine-select").value = engine;
  };
  const overlay = $("overlay");
  const openSettings = () => { overlay.hidden = false; document.body.style.overflow = "hidden"; };
  const closeSettings = () => { overlay.hidden = true; document.body.style.overflow = ""; };
  $("settings-open").addEventListener("click", openSettings);
  $("settings-close").addEventListener("click", closeSettings);
  overlay.addEventListener("click", e => { if (e.target === overlay) closeSettings(); });
  document.addEventListener("keydown", e => {
    if (e.key === "Escape") closeSettings();
    if ((e.ctrlKey || e.metaKey) && e.key === ",") { e.preventDefault(); openSettings(); }
    if (e.key === "/" && !["INPUT","TEXTAREA"].includes(document.activeElement.tagName)) { e.preventDefault(); $("query").focus(); }
  });
  $("search-form").addEventListener("submit", e => {
    e.preventDefault();
    const q = $("query").value.trim();
    if (!q) { $("query").focus(); return; }
    location.href = engines[engine].url + encodeURIComponent(q);
  });
  $("engine-select").addEventListener("change", e => { engine = e.target.value; write("engine", engine); updateEngine(); });
  $("engine-quick").addEventListener("click", openSettings);
  $("theme-select").addEventListener("change", e => { theme = e.target.value; write("theme", theme); applyTheme(); });
  $("motion-toggle").addEventListener("change", e => { motion = e.target.checked; write("motion", motion ? "on" : "off"); applyMotion(); });
  $("appearance-tile").addEventListener("click", openSettings);
  $("privacy-tile").addEventListener("click", openSettings);
  $("ai-tile").addEventListener("click", () => showToast("Локальный модуль Qwen ещё не подключён."));
  function showToast(message) { const el = $("toast"); el.textContent = message; el.classList.add("show"); setTimeout(() => el.classList.remove("show"), 2600); }
  updateEngine(); applyTheme(); applyMotion(); $("query").focus({ preventScroll: true });
})();