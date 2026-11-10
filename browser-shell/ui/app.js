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
  const sessionKey = "skai.session.tabs.v1";
  let sessionToRestore = [];
  try {
    const savedSession = JSON.parse(localStorage.getItem(sessionKey) || "[]");
    if (Array.isArray(savedSession)) {
      sessionToRestore = savedSession
        .filter(item => item && typeof item.url === "string" && /^https?:\/\//i.test(item.url))
        .slice(0, 50);
    }
  } catch (_) { sessionToRestore = []; }

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
        ? "Модель ещё не подключена · запросы не отправляются"
        : "Облачная модель не подключена · передача отключена";
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
  const bookmarkKey = "skai.bookmarks.v1";
  const historyKey = "skai.history.v1";
  let bookmarks = [];
  let historyItems = [];
  let downloads = [];
  let internalViewType = "";
  try { bookmarks = JSON.parse(localStorage.getItem(bookmarkKey) || "[]"); } catch (_) { bookmarks = []; }
  try { historyItems = JSON.parse(localStorage.getItem(historyKey) || "[]"); } catch (_) { historyItems = []; }
  if (!Array.isArray(bookmarks)) bookmarks = [];
  if (!Array.isArray(historyItems)) historyItems = [];

  const pageSections = $$(".page-scroll > :not(#internalView)");
  function syncBookmarkButton() {
    const url = $("#addressInput")?.value || "";
    const saved = bookmarks.some(item => item.url === url);
    const button = $("#bookmarkButton");
    if (!button) return;
    button.textContent = saved ? "★" : "☆";
    button.title = saved ? "Удалить закладку" : "Добавить закладку";
    button.setAttribute("aria-pressed", String(saved));
  }
  function persistBookmarks() {
    try { localStorage.setItem(bookmarkKey, JSON.stringify(bookmarks)); }
    catch (_) { notify("Не удалось сохранить закладки в профиле"); }
  }
  function persistHistory() {
    try { localStorage.setItem(historyKey, JSON.stringify(historyItems)); }
    catch (_) { notify("Не удалось сохранить историю в профиле"); }
  }
  function addHistoryVisit(id, title, url) {
    if (!/^https?:\/\//i.test(url)) return;
    const now = Date.now();
    const previous = historyItems[0];
    if (previous && previous.url === url && now - previous.visitedAt < 60000) {
      previous.title = title || previous.title || url;
      previous.visitedAt = now;
      previous.tabId = id;
    } else {
      historyItems.unshift({ title: title || url, url, visitedAt: now, tabId: id });
    }
    historyItems = historyItems.slice(0, 2000);
    persistHistory();
    if (internalViewType === "history") renderInternalView("history");
  }
  function formatVisitTime(value) {
    try { return new Intl.DateTimeFormat("ru-RU", { dateStyle: "short", timeStyle: "short" }).format(new Date(value)); }
    catch (_) { return ""; }
  }
  function openSavedUrl(url) {
    if (!/^https?:\/\//i.test(url)) return;
    createTab(url);
    closeInternalView();
  }
  function formatDownloadBytes(value) {
    if (!Number.isFinite(value) || value < 0) return "Размер неизвестен";
    const units = ["Б", "КБ", "МБ", "ГБ"];
    let size = value, unit = 0;
    while (size >= 1024 && unit < units.length - 1) { size /= 1024; unit++; }
    return (unit === 0 ? Math.round(size) : size.toFixed(1)) + " " + units[unit];
  }
  function downloadStatusLabel(item) {
    if (item.state === "complete") return "Завершено";
    if (item.state === "paused") return "Приостановлено";
    if (item.state === "cancelled") return "Отменено";
    if (item.state === "interrupted") return "Прервано";
    if (item.state === "active") return item.percent >= 0 ? "Загрузка · " + item.percent + "%" : "Загрузка";
    return "Подготовка";
  }
  function runDownloadAction(item, action) {
    window.skaiNative?.downloadAction(item.id, action);
  }
  function renderInternalView(type) {
    internalViewType = type;
    const view = $("#internalView");
    if (!view) return;
    view.hidden = false;
    pageSections.forEach(section => { section.hidden = true; });
    $("#internalViewTitle").textContent =
      type === "bookmarks" ? "Закладки" : type === "history" ? "История" : "Загрузки";
    $("#internalViewDescription").textContent =
      type === "bookmarks" ? "Сохранённые страницы в вашем профиле" :
      type === "history" ? "Недавно посещённые страницы · хранится локально" :
      "Файлы, полученные через браузер · папка Загрузки/SkAi";
    const list = $("#internalViewList");
    list.replaceChildren();
    const allItems = type === "bookmarks" ? bookmarks :
      type === "history" ? historyItems : downloads;
    const searchInput = $("#internalViewSearch");
    if (searchInput) searchInput.placeholder =
      type === "bookmarks" ? "Найти закладку по названию или адресу" :
      type === "history" ? "Найти посещённую страницу" : "Найти файл по имени или адресу";
    const query = (searchInput?.value || "").trim().toLocaleLowerCase("ru-RU");
    const items = allItems.map((item, index) => ({ item, index })).filter(entry => {
      const haystack = ((entry.item.title || entry.item.fileName || "") + " " +
        (entry.item.url || "") + " " + (entry.item.path || "")).toLocaleLowerCase("ru-RU");
      return !query || haystack.includes(query);
    });
    $("#internalViewEmpty").hidden = items.length > 0;
    $("#internalViewEmpty").textContent = allItems.length === 0
      ? (type === "bookmarks" ? "Пока нет закладок. Нажмите ☆ рядом с адресом страницы." :
         type === "history" ? "История посещений пока пуста." :
         "Пока нет загрузок. Загруженные файлы появятся здесь.")
      : "По вашему запросу ничего не найдено.";

    items.forEach(({ item, index }) => {
      const row = document.createElement("article");
      row.className = "library-row";
      const open = document.createElement("button");
      open.className = "library-open";
      open.type = "button";
      const title = document.createElement("b");
      const detail = document.createElement("small");

      if (type === "downloads") {
        title.textContent = item.fileName || "Загрузка";
        const received = formatDownloadBytes(item.receivedBytes || 0);
        const total = item.totalBytes > 0 ? " из " + formatDownloadBytes(item.totalBytes) : "";
        const speed = item.state === "active" && item.speedBytesPerSec > 0
          ? " · " + formatDownloadBytes(item.speedBytesPerSec) + "/с" : "";
        detail.textContent = downloadStatusLabel(item) + " · " + received + total + speed;
        open.addEventListener("click", () => {
          if (item.state === "complete") runDownloadAction(item, "open");
        });
        open.disabled = item.state !== "complete";
      } else {
        title.textContent = item.title || item.url;
        detail.textContent = type === "history"
          ? item.url + " · " + formatVisitTime(item.visitedAt)
          : item.url;
        open.addEventListener("click", () => openSavedUrl(item.url));
      }
      open.append(title, detail);
      row.append(open);

      if (type === "downloads") {
        const actions = document.createElement("div");
        actions.className = "download-actions";
        const addAction = (label, action, variant = "") => {
          const button = document.createElement("button");
          button.className = "library-remove " + variant;
          button.type = "button";
          button.textContent = label;
          button.addEventListener("click", () => runDownloadAction(item, action));
          actions.append(button);
        };
        if (item.state === "active") {
          addAction("Пауза", "pause");
          addAction("Отменить", "cancel");
        } else if (item.state === "paused") {
          addAction("Продолжить", "resume");
          addAction("Отменить", "cancel");
        } else if (item.state === "complete") {
          addAction("Открыть", "open");
          addAction("Папка", "show");
        } else {
          const remove = document.createElement("button");
          remove.className = "library-remove";
          remove.type = "button";
          remove.textContent = "Убрать";
          remove.addEventListener("click", () => {
            downloads.splice(index, 1);
            renderInternalView("downloads");
          });
          actions.append(remove);
        }
        row.append(actions);
      } else {
        const remove = document.createElement("button");
        remove.className = "library-remove";
        remove.type = "button";
        remove.textContent = type === "bookmarks" ? "Удалить" : "Убрать";
        remove.addEventListener("click", () => {
          if (type === "bookmarks") {
            bookmarks.splice(index, 1);
            persistBookmarks();
            syncBookmarkButton();
          } else {
            historyItems.splice(index, 1);
            persistHistory();
          }
          renderInternalView(type);
        });
        row.append(remove);
      }
      list.append(row);
    });

    const footer = $("#internalViewFooter");
    footer.replaceChildren();
    if (type === "history" && historyItems.length) {
      const clear = document.createElement("button");
      clear.className = "library-clear";
      clear.type = "button";
      clear.textContent = "Очистить историю";
      clear.addEventListener("click", () => {
        historyItems = [];
        persistHistory();
        renderInternalView("history");
      });
      footer.append(clear);
    } else if (type === "downloads" && downloads.length) {
      const note = document.createElement("span");
      note.className = "downloads-note";
      note.textContent = "Список активен до закрытия браузера; сами файлы остаются в папке загрузок.";
      footer.append(note);
    }
  }
  function openInternalView(type) {
    internalViewType = type;
    window.skaiNative?.activateTab(0);
    setActiveTabUi(homeTab);
    renderInternalView(type);
  }
  function closeInternalView() {
    internalViewType = "";
    const view = $("#internalView");
    if (view) view.hidden = true;
    pageSections.forEach(section => { section.hidden = false; });
    syncBookmarkButton();
  }
  function toggleBookmark() {
    const url = $("#addressInput")?.value || "";
    if (!/^https?:\/\//i.test(url)) {
      notify("Сначала откройте веб-страницу");
      return;
    }
    const existing = bookmarks.findIndex(item => item.url === url);
    if (existing >= 0) {
      bookmarks.splice(existing, 1);
      notify("Закладка удалена");
    } else {
      const activeTab = $('.tab.active');
      const title = activeTab ? $(".tab-name", activeTab)?.textContent : url;
      bookmarks.unshift({ title: title || url, url, createdAt: Date.now() });
      notify("Страница добавлена в закладки");
    }
    persistBookmarks();
    syncBookmarkButton();
    if (internalViewType === "bookmarks") renderInternalView("bookmarks");
  }
  $("#bookmarkButton")?.addEventListener("click", toggleBookmark);
  $("#historyNav")?.addEventListener("click", () => openInternalView("history"));
  $("#downloadsNav")?.addEventListener("click", () => openInternalView("downloads"));
  $("#downloadsButton")?.addEventListener("click", () => openInternalView("downloads"));
  $("#homeNav")?.addEventListener("click", () => {
    closeInternalView();
    activateTab(homeTab);
  });
  $("#internalViewClose")?.addEventListener("click", () => closeInternalView());
  $("#internalViewSearch")?.addEventListener("input", () => {
    if (internalViewType) renderInternalView(internalViewType);
  });
  $$(".shortcut-card[data-url]").forEach(card => card.addEventListener("click", () => {
    const route = card.dataset.url;
    if (route === "skai://bookmarks") {
      openInternalView("bookmarks");
      return;
    }
    $("#addressInput").value = route;
    notify("Этот раздел будет подключён на следующем этапе");
  }));
  syncBookmarkButton();

  window.skaiNative = window.skaiNative || {};
  window.skaiNative.addressChanged = url => {
    $("#addressInput").value = url;
    syncBookmarkButton();
  };
  window.skaiNative.navigationState = (id, canGoBack, canGoForward, isLoading) => {
    const tab = $('.tab[data-tab-id="' + id + '"]');
    const selected = id === 0 || tab?.classList.contains("active");
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
  function setActiveTabUi(tab) {
    $$(".tab").forEach(item => {
      const active = item === tab;
      item.classList.toggle("active", active);
      item.setAttribute("aria-selected", String(active));
    });
    const id = Number(tab.dataset.tabId || 0);
    $("#pageArea")?.classList.toggle("site-active", id !== 0);
    updateContentBounds();
  }
  function activateTab(tab) {
    setActiveTabUi(tab);
    const id = Number(tab.dataset.tabId || 0);
    window.skaiNative?.activateTab(id);
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
  function createTabElement(id, title = "Новая вкладка") {
    const tab = document.createElement("button");
    tab.className = "tab";
    tab.dataset.tabId = String(id);
    tab.setAttribute("role", "tab");
    tab.setAttribute("aria-selected", "false");
    tab.innerHTML = '<span class="tab-favicon">◉</span><span class="tab-name"></span><span class="tab-close">×</span>';
    $(".tab-name", tab).textContent = title;
    $("#tabs").append(tab);
    bindTab(tab);
    return tab;
  }
  function createTab(url = "about:blank") {
    if (internalViewType) closeInternalView();
    createTabElement(tabSequence++);
    // Native CEF allocates the authoritative ID and publishes a tab snapshot.
    window.skaiNative?.newTab(url);
  }
  const homeTab = $(".tab");
  if (homeTab) { homeTab.dataset.tabId = "0"; bindTab(homeTab); }
  $("#newTab")?.addEventListener("click", () => createTab("about:blank"));
  window.skaiNative = window.skaiNative || {};
  window.skaiNative.addressChanged = url => { $("#addressInput").value = url; syncBookmarkButton(); };
  function persistBrowserSession() {
    const tabs = $(".tab")
      .filter(tab => Number(tab.dataset.tabId || 0) !== 0)
      .map(tab => ({
        title: $(".tab-name", tab)?.textContent || tab.title || "",
        url: tab.title || "",
        active: tab.classList.contains("active")
      }))
      .filter(item => /^https?:\/\//i.test(item.url));
    tabs.sort((a, b) => Number(a.active) - Number(b.active));
    try { localStorage.setItem(sessionKey, JSON.stringify(tabs.slice(-50))); }
    catch (_) {}
  }

  window.skaiNative.syncTabs = tabs => {
    const nativeIds = new Set(tabs.map(info => Number(info.id)));
    tabs.forEach(info => {
      const id = Number(info.id);
      let tab = $('.tab[data-tab-id="' + id + '"]');
      if (!tab && id !== 0) tab = createTabElement(id, info.title || "Новая вкладка");
      if (!tab) return;
      $(".tab-name", tab).textContent = info.title || "Новая вкладка";
      tab.title = info.url || "";
      tab.classList.toggle("loading", !!info.loading);
      if (info.active && info.url) {
        $("#addressInput").value = info.url;
        syncBookmarkButton();
      }
      tabSequence = Math.max(tabSequence, id + 1);
    });
    $$(".tab").forEach(tab => {
      const id = Number(tab.dataset.tabId || 0);
      if (id !== 0 && !nativeIds.has(id)) tab.remove();
    });
    const activeInfo = tabs.find(info => info.active);
    const activeTab = activeInfo
      ? $('.tab[data-tab-id="' + Number(activeInfo.id) + '"]')
      : homeTab;
    if (activeTab) {
      setActiveTabUi(activeTab);
      syncBookmarkButton();
    }

    persistBrowserSession();
  };
  window.skaiNative.downloadUpdated = item => {
    if (!item || !Number.isFinite(Number(item.id))) return;
    const id = Number(item.id);
    const existing = downloads.findIndex(entry => entry.id === id);
    const normalized = {
      ...item,
      id,
      percent: Number(item.percent),
      receivedBytes: Number(item.receivedBytes),
      totalBytes: Number(item.totalBytes),
      speedBytesPerSec: Number(item.speedBytesPerSec)
    };
    if (existing >= 0) downloads[existing] = normalized;
    else downloads.unshift(normalized);
    downloads = downloads.slice(0, 200);
    if (internalViewType === "downloads") renderInternalView("downloads");
  };
  window.skaiNative.updateTab = (id, title, url, loading) => {
    const tab = $('.tab[data-tab-id="' + id + '"]');
    if (!tab) return;
    $(".tab-name", tab).textContent = title || "Новая вкладка";
    tab.title = url || "";
    tab.classList.toggle("loading", !!loading);
    persistBrowserSession();
    if (tab.classList.contains("active")) {
      $("#addressInput").value = url || "";
      syncBookmarkButton();
    }
    if (!loading) addHistoryVisit(id, title, url);
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

  if (sessionToRestore.length) {
    const restore = sessionToRestore.slice();
    restore.forEach(item => createTab(item.url));
  }
})();