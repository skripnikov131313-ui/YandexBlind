(() => {
  "use strict";
  const $ = (s, root = document) => root.querySelector(s);
  const $$ = (s, root = document) => [...root.querySelectorAll(s)];
  const browser = $("#browserWindow");
  const panel = $("#assistantPanel");
  const toast = $("#toast");
  const providers = [
    {name:"Яндекс", url:q=>"https://yandex.ru/search/?text="+encodeURIComponent(q)},
    {name:"Google", url:q=>"https://www.google.com/search?q="+encodeURIComponent(q)},
    {name:"Bing", url:q=>"https://www.bing.com/search?q="+encodeURIComponent(q)},
    {name:"DuckDuckGo", url:q=>"https://duckduckgo.com/?q="+encodeURIComponent(q)},
    {name:"Brave", url:q=>"https://search.brave.com/search?q="+encodeURIComponent(q)},
    {name:"Startpage", url:q=>"https://www.startpage.com/sp/search?query="+encodeURIComponent(q)}
  ];
  let providerIndex = 0;
  try {
    providerIndex = Number(localStorage.getItem("skai.search.provider.v1") || 0);
  } catch (_) {}
  if (!Number.isInteger(providerIndex) || providerIndex < 0 || providerIndex >= providers.length) {
    providerIndex = 0;
  }
  let themeMode = "system";
  try { themeMode = localStorage.getItem("skai.theme.mode.v1") || "system"; } catch (_) {}
  if (!["system", "dark", "light"].includes(themeMode)) themeMode = "system";
  let mode = "local";
  let toastTimer = 0;
  let activePageLoading = false;
  const conversationKey = "skai.conversation.v1";
  const modeKey = "skai.assistant.mode.v1";
  let conversation = [];
  try { conversation = JSON.parse(localStorage.getItem(conversationKey) || "[]"); } catch (_) { conversation = []; }
  try { mode = localStorage.getItem(modeKey) || "local"; } catch (_) {}

  const systemTheme = window.matchMedia("(prefers-color-scheme: light)");
  function applyTheme() {
    const theme = themeMode === "system" ? (systemTheme.matches ? "light" : "dark") : themeMode;
    document.body.dataset.theme = theme;
    document.body.dataset.themeMode = themeMode;
    const button = $("#themeButton");
    if (button) button.title = "Тема: " + (themeMode === "system" ? "как в Windows" : themeMode === "light" ? "светлая" : "тёмная");
  }
  function cycleTheme() {
    themeMode = themeMode === "system" ? "dark" : themeMode === "dark" ? "light" : "system";
    try { localStorage.setItem("skai.theme.mode.v1", themeMode); } catch (_) {}
    applyTheme();
    notify(themeMode === "system" ? "Тема Windows" : themeMode === "light" ? "Светлая тема" : "Тёмная тема");
  }
  applyTheme();
  systemTheme.addEventListener?.("change", () => {
    if (themeMode === "system") applyTheme();
  });

  function notify(message) {
    if (!toast) return;
    toast.textContent = message;
    toast.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toast.classList.remove("show"), 2300);
  }
  function showAssistant() {
    browser.classList.remove("assistant-collapsed", "assistant-floating");
    panel.classList.remove("mobile-hidden");
  }
  function hideAssistant() {
    browser.classList.add("assistant-collapsed");
    browser.classList.remove("assistant-floating");
  }
  $("#nativeMinimize")?.addEventListener("click", () => window.skaiNative?.minimize());
  $("#nativeMaximize")?.addEventListener("click", () => window.skaiNative?.toggleMaximize());
  $("#nativeClose")?.addEventListener("click", () => window.skaiNative?.close());

  $("#assistantRail")?.addEventListener("click", showAssistant);
  $("#reopenAssistant")?.addEventListener("click", showAssistant);
  $("#hideAssistant")?.addEventListener("click", hideAssistant);
  $("#collapseAssistant")?.addEventListener("click", hideAssistant);
  $("#floatAssistant")?.addEventListener("click", () => {
    hideAssistant();
    window.skaiNative?.toggleFloatingAssistant();
    notify("Помощник открыт в отдельном окне");
  });

  function applyAssistantMode(persist = false) {
    $$(".mode-option").forEach(button => {
      button.classList.toggle("active", button.dataset.mode === mode);
    });
    const note = $("#modelFootnote");
    if (note) {
      note.textContent = mode === "local"
        ? "Работает на этом устройстве"
        : "Облако · перед отправкой потребуется согласие";
    }
    const badge = $(".local-badge");
    if (badge) {
      badge.textContent = mode === "local" ? "LOCAL" : "CLOUD";
      badge.classList.toggle("cloud-badge", mode === "cloud");
    }
    if (persist) {
      try { localStorage.setItem(modeKey, mode); } catch (_) {}
    }
  }
  applyAssistantMode();
  $$(".mode-option").forEach(button => button.addEventListener("click", () => {
    mode = button.dataset.mode;
    applyAssistantMode(true);
    notify(mode === "local" ? "Выбран локальный ИИ" : "Выбрано облако. Передача данных пока отключена.");
  }));

  function addMessage(kind, value) {
    conversation.push({ role: kind, text: value });
    conversation = conversation.slice(-200);
    try { localStorage.setItem(conversationKey, JSON.stringify(conversation)); } catch (_) {}
    const chat = $("#chatArea");
    $(".assistant-intro", chat)?.remove();
    $(".suggestion-list", chat)?.remove();
    const message = document.createElement("div");
    message.className = "chat-message " + kind;
    message.textContent = value;
    chat.append(message);
    chat.scrollTop = chat.scrollHeight;
  }
  if (conversation.length) {
    $(".assistant-intro", $("#chatArea"))?.remove();
    $(".suggestion-list", $("#chatArea"))?.remove();
    conversation.forEach(item => {
      const message = document.createElement("div");
      message.className = "chat-message " + (item.role === "user" ? "user" : "assistant");
      message.textContent = item.text || "";
      $("#chatArea").append(message);
    });
  }
  function sendPrompt(value) {
    const prompt = value.trim();
    if (!prompt) return;
    addMessage("user", prompt);
    addMessage("assistant", mode === "local"
      ? "Это интерфейсный прототип Sk/Ai. На следующем этапе подключим локальную модель Qwen — сейчас запрос не отправляется модели."
      : "Облачный режим выбран, но провайдер пока не подключён. Запрос и контекст не покидают устройство.");
    $("#promptInput").value = "";
  }
  $("#sendPrompt")?.addEventListener("click", () => sendPrompt($("#promptInput").value));
  $("#promptInput")?.addEventListener("keydown", event => {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      sendPrompt(event.currentTarget.value);
    }
  });
  $$(".suggestion").forEach(button => button.addEventListener("click", () => {
    $("#promptInput").value = button.dataset.prompt || "";
    $("#promptInput").focus();
  }));
  $("#sharePage")?.addEventListener("click", () => notify("Передача страницы появится после подключения браузерного движка"));

  function syncProvider() {
    $("#engineButton").innerHTML = providers[providerIndex].name + " <span>⌄</span>";
    $("#currentEngine").textContent = providers[providerIndex].name;
  }
  syncProvider();
  function cycleProvider() {
    providerIndex = (providerIndex + 1) % providers.length;
    try { localStorage.setItem("skai.search.provider.v1", String(providerIndex)); } catch (_) {}
    syncProvider();
    
    notify("Поисковая система: " + providers[providerIndex].name);
  }
  $("#engineButton")?.addEventListener("click", cycleProvider);
  function search(value) {
    const query = value.trim();
    if (!query) return;
    const looksLikeUrl = /^(https?:\/\/|localhost(?::\d+)?|[\w-]+\.[a-z]{2,})(\/.*)?$/i.test(query);
    const target = looksLikeUrl ? (/^https?:\/\//i.test(query) ? query : "https://" + query) : providers[providerIndex].url(query);
    $("#addressInput").value = target;
    if (window.skaiNative?.navigate) window.skaiNative.navigate(target);
    else notify("Браузерное ядро не подключено");
  }
  $("#homeSearch")?.addEventListener("keydown", event => {
    if (event.key === "Enter") search(event.currentTarget.value);
  });
  $("#homeSearchSubmit")?.addEventListener("click", () => search($("#homeSearch").value));
  $("#addressInput")?.addEventListener("keydown", event => {
    if (event.key === "Enter") search(event.currentTarget.value);
  });
  $("#goBack")?.addEventListener("click", () => window.skaiNative?.back());
  $("#goForward")?.addEventListener("click", () => window.skaiNative?.forward());
  $("#reloadPage")?.addEventListener("click", () => {
    if (activePageLoading) window.skaiNative?.stop();
    else window.skaiNative?.reload();
  });

  $("#themeButton")?.addEventListener("click", cycleTheme);
  $$(".shortcut-card[data-url]").forEach(card => card.addEventListener("click", () => {
    $("#addressInput").value = card.dataset.url;
    notify("Раздел откроется после подключения Sk/Ai Core");
  }));
  $("#bookmarkButton")?.addEventListener("click", event => {
    event.currentTarget.textContent = event.currentTarget.textContent === "☆" ? "★" : "☆";
    notify(event.currentTarget.textContent === "★" ? "Закладка добавлена (демо)" : "Закладка удалена (демо)");
  });

  window.skaiNative = window.skaiNative || {};
  window.skaiNative.addressChanged = url => { $("#addressInput").value = url; };
  window.skaiNative.navigationState = (id, canGoBack, canGoForward, isLoading) => {
    const selected = id === 0 || $('.tab[data-tab-id="' + id + '']')?.classList.contains("active");
    if (!selected) return;
    activePageLoading = !!isLoading;
    const back = $("#goBack");
    const forward = $("#goForward");
    const reload = $("#reloadPage");
    if (back) back.disabled = !canGoBack;
    if (forward) forward.disabled = !canGoForward;
    if (reload) {
      reload.textContent = activePageLoading ? "×" : "↻";
      reload.title = activePageLoading ? "Остановить загрузку" : "Обновить страницу";
      reload.setAttribute("aria-label", reload.title);
    }
  };
  window.skaiNative.assistantClosed = () => {
    browser.classList.remove("assistant-collapsed", "assistant-floating");
    panel.classList.remove("mobile-hidden");
    notify("Помощник возвращён в браузер");
  };

  let tabSequence = 1;
  function activateTab(tab) {
    $$(".tab").forEach(item => {
      const active = item === tab;
      item.classList.toggle("active", active);
      item.setAttribute("aria-selected", String(active));
    });
    const id = Number(tab.dataset.tabId || 0);
    window.skaiNative?.activateTab(id);
    $("#pageArea")?.classList.toggle("site-active", id !== 0);
    updateContentBounds();
  }
  function bindTab(tab) {
    tab.addEventListener("click", event => {
      if (event.target.closest(".tab-close")) {
        event.stopPropagation();
        const id = Number(tab.dataset.tabId || 0);
        if (id === 0) { notify("Стартовая вкладка остаётся открытой"); return; }
        window.skaiNative?.closeTab(id);
        tab.remove();
        const remaining = $$(".tab");
        if (remaining.length) activateTab(remaining[remaining.length - 1]);
        return;
      }
      activateTab(tab);
    });
  }
  function createTab(url = "about:blank") {
    const tab = document.createElement("button");
    tab.className = "tab";
    tab.dataset.tabId = String(tabSequence++);
    tab.setAttribute("role", "tab");
    tab.setAttribute("aria-selected", "false");
    tab.innerHTML = '<span class="tab-favicon">◉</span><span class="tab-name">Новая вкладка</span><span class="tab-close">×</span>';
    $("#tabs").append(tab);
    bindTab(tab);
    window.skaiNative?.newTab(url);
    activateTab(tab);
  }
  const homeTab = $(".tab");
  if (homeTab) { homeTab.dataset.tabId = "0"; bindTab(homeTab); }
  $("#newTab")?.addEventListener("click", () => createTab("about:blank"));
  window.skaiNative = window.skaiNative || {};
  window.skaiNative.addressChanged = url => { $("#addressInput").value = url; };
  window.skaiNative.syncTabs = tabs => {
    tabs.forEach(info => {
      const tab = $('.tab[data-tab-id="' + info.id + '"]');
      if (!tab) return;
      $(".tab-name", tab).textContent = info.title || "Новая вкладка";
      if (info.active) activateTab(tab);
    });
  };
  window.skaiNative.updateFavicon = (id, url) => {
    const tab = $('.tab[data-tab-id="' + id + '"]');
    if (!tab || !/^https?:\/\//i.test(url)) return;
    const icon = $(".tab-favicon", tab);
    if (!icon) return;
    const image = document.createElement("img");
    image.alt = "";
    image.width = 16;
    image.height = 16;
    image.referrerPolicy = "no-referrer";
    image.src = url;
    image.onerror = () => { image.remove(); };
    icon.replaceChildren(image);
  };
  window.skaiNative.updateTab = (id, title, url, loading) => {
    const tab = $('.tab[data-tab-id="' + id + '"]');
    if (!tab) return;
    $(".tab-name", tab).textContent = title || "Новая вкладка";
    tab.title = url || "";
    tab.classList.toggle("loading", !!loading);
    if (tab.classList.contains("active")) $("#addressInput").value = url || "";
  };
  function updateContentBounds() {
    const area = $("#pageArea");
    if (!area || !window.skaiNative?.setContentBounds) return;
    const rect = area.getBoundingClientRect();
    const scale = window.devicePixelRatio || 1;
    window.skaiNative.setContentBounds(
      Math.round(rect.left * scale), Math.round(rect.top * scale),
      Math.round(rect.width * scale), Math.round(rect.height * scale)
    );
  }
  const boundsObserver = new ResizeObserver(updateContentBounds);
  boundsObserver.observe($("#pageArea"));
  window.addEventListener("resize", updateContentBounds);
  requestAnimationFrame(updateContentBounds);
  $(".rail-toggle")?.addEventListener("click", cycleTheme);
  $(".rail-item.moon")?.addEventListener("click", cycleTheme);
})();