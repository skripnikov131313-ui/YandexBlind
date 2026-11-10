(() => {
  "use strict";

  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
  const shell = $("#windowShell");
  const body = $(".browser-body");
  const panel = $("#assistantPanel");
  const workspaceContent = $("#workspaceContent");
  const homeMarkup = workspaceContent.innerHTML;
  const toastNode = $("#toast");
  let toastTimer;
  let tabSequence = 1;
  let activeTabId = 1;
  let currentPage = "home";
  let assistantMode = "local";
  const tabs = [{ id: 1, title: "Новая вкладка" }];

  const pageInfo = {
    bookmarks: ["Закладки", "Сохранённые сайты и быстрый доступ", "Здесь будут ваши избранные сайты. Синхронизация и импорт закладок появятся в полноценной версии браузера."],
    privacy: ["Приватность", "Контроль над данными и разрешениями", "Управляйте cookie, разрешениями сайтов, блокировкой трекеров и очисткой данных браузера."],
    extensions: ["Расширения", "Ваши инструменты для браузера", "Каталог расширений, установка, разрешения и управление будут частью Sk/Ai Core."],
    themes: ["Оформление", "Настройте атмосферу Sk/Ai", "Выберите светлую или тёмную тему. В этой версии доступна интерактивная демонстрация Soft Aurora."],
    history: ["История", "Недавно посещённые страницы", "История посещений будет храниться локально в профиле Sk/Ai."],
    downloads: ["Загрузки", "Файлы, загруженные через браузер", "Здесь появятся загрузки, очередь и управление файлами."],
    settings: ["Настройки Sk/Ai", "Браузер под ваши правила", "Управление внешним видом, поиском, приватностью, вкладками и локальной моделью."]
  };

  function toast(message) {
    toastNode.textContent = message;
    toastNode.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toastNode.classList.remove("show"), 2600);
  }

  function escapeHTML(value) {
    return String(value).replace(/[&<>"']/g, ch => ({
      "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
    })[ch]);
  }

  function setTheme(theme) {
    document.documentElement.dataset.theme = theme;
    const icon = $("#themeToggle use");
    icon.setAttribute("href", theme === "dark" ? "#i-moon" : "#i-sun");
    try { localStorage.setItem("skai.ui.theme", theme); } catch {}
  }

  try {
    const savedTheme = localStorage.getItem("skai.ui.theme");
    if (savedTheme === "dark" || savedTheme === "light") setTheme(savedTheme);
    else if (window.matchMedia?.("(prefers-color-scheme: light)").matches) setTheme("light");
  } catch {}

  $("#themeToggle").addEventListener("click", () => {
    setTheme(document.documentElement.dataset.theme === "dark" ? "light" : "dark");
    toast("Оформление переключено");
  });

  function renderTabs() {
    const tabsNode = $("#tabs");
    tabsNode.innerHTML = tabs.map(tab => `
      <button class="tab ${tab.id === activeTabId ? "active" : ""}" data-tab="${tab.id}" type="button">
        <span class="tab-favicon">S</span>
        <span class="tab-title">${escapeHTML(tab.title)}</span>
        <span class="tab-close" data-close-tab="${tab.id}" aria-label="Закрыть вкладку"><svg><use href="#i-close"/></svg></span>
      </button>
    `).join("");
  }

  function addTab() {
    const id = ++tabSequence;
    tabs.push({ id, title: "Новая вкладка" });
    activeTabId = id;
    renderTabs();
    showPage("home");
  }
  $("#newTab").addEventListener("click", addTab);

  $("#tabs").addEventListener("click", event => {
    const close = event.target.closest("[data-close-tab]");
    if (close) {
      const id = Number(close.dataset.closeTab);
      const index = tabs.findIndex(tab => tab.id === id);
      if (index < 0) return;
      tabs.splice(index, 1);
      if (!tabs.length) { addTab(); return; }
      if (activeTabId === id) activeTabId = tabs[Math.max(0, index - 1)].id;
      renderTabs();
      showPage("home");
      return;
    }
    const tab = event.target.closest("[data-tab]");
    if (tab) {
      activeTabId = Number(tab.dataset.tab);
      renderTabs();
      showPage("home");
    }
  });

  function pageCard(icon, title, text, action) {
    return `<button class="page-card" type="button" data-action="${action}">
      <span class="page-card-icon"><svg><use href="#${icon}"/></svg></span>
      <span class="page-card-text"><b>${title}</b><small>${text}</small></span>
      <svg class="page-card-arrow"><use href="#i-chevron"/></svg>
    </button>`;
  }

  function showPage(page, query = "") {
    currentPage = page;
    $$(".rail-button[data-page]").forEach(button => button.classList.toggle("active", button.dataset.page === page));
    if (page === "home") {
      workspaceContent.innerHTML = homeMarkup;
      bindSearchForms();
      return;
    }
    if (page === "assistant") {
      openAssistant();
      showPage("home");
      return;
    }
    const info = pageInfo[page] || pageInfo.settings;
    const isSearch = page === "search";
    const title = isSearch ? "Результаты поиска" : info[0];
    const description = isSearch ? `Поисковый запрос: «${escapeHTML(query)}»` : info[1];
    const detail = isSearch ? "В полноценном браузере результаты будут открываться в этой области через выбранную поисковую систему." : info[2];
    let cards = "";
    if (page === "themes") {
      cards = pageCard("i-moon", "Тёмная тема", "Текущий Soft Aurora", "theme-dark") +
              pageCard("i-sun", "Светлая тема", "Мягкая светлая палитра", "theme-light");
    } else if (page === "settings") {
      cards = pageCard("i-palette", "Внешний вид", "Тема и визуальная атмосфера", "themes") +
              pageCard("i-search", "Поисковая система", "Выбор сервиса поиска", "search-settings") +
              pageCard("i-shield", "Приватность", "Разрешения и данные сайтов", "privacy");
    } else if (page === "privacy") {
      cards = pageCard("i-shield", "Защита сайтов", "Статус и правила защиты", "protection") +
              pageCard("i-trash", "Очистка данных", "История, cookie и кэш", "clear-data");
    } else if (page === "bookmarks") {
      cards = pageCard("i-star", "Добавить текущую страницу", "Сохранить в закладки", "bookmark");
    } else if (page === "extensions") {
      cards = pageCard("i-puzzle", "Каталог расширений", "Управление дополнениями", "extensions-info");
    } else if (page === "history") {
      cards = pageCard("i-clock", "История пока пуста", "Посещённые страницы будут отображаться здесь", "history-info");
    } else if (page === "downloads") {
      cards = pageCard("i-download", "Загрузок пока нет", "Загруженные файлы будут отображаться здесь", "downloads-info");
    }
    workspaceContent.innerHTML = `
      <section class="subpage-view">
        <div class="subpage-heading">
          <span class="subpage-symbol"><svg><use href="#${page === "search" ? "i-search" : page === "themes" ? "i-palette" : page === "privacy" ? "i-shield" : page === "bookmarks" ? "i-star" : page === "extensions" ? "i-puzzle" : page === "history" ? "i-clock" : page === "downloads" ? "i-download" : "i-settings"}"/></svg></span>
          <div><span class="subpage-eyebrow">SK/AI WORKSPACE</span><h1>${title}</h1><p>${description}</p></div>
        </div>
        <div class="subpage-description">${detail}</div>
        <div class="subpage-cards">${cards}</div>
        <button class="back-home" type="button" data-action="home"><svg><use href="#i-back"/></svg>На главную</button>
      </section>`;
    $$(".page-card").forEach(card => card.addEventListener("click", () => handleAction(card.dataset.action)));
  }

  function openAssistant() {
    panel.classList.remove("is-hidden");
    body.classList.remove("panel-closed");
    panel.classList.add("is-visible");
    toast("Sk/Ai Assistant открыт");
  }
  function closeAssistant() {
    panel.classList.add("is-hidden");
    body.classList.add("panel-closed");
    panel.classList.remove("is-visible");
  }
  $("#closeAssistant").addEventListener("click", closeAssistant);
  $$("[data-page]").forEach(button => button.addEventListener("click", () => {
    const page = button.dataset.page;
    if (page === "assistant") { openAssistant(); return; }
    showPage(page);
  }));

  function handleAction(action) {
    switch (action) {
      case "home": showPage("home"); break;
      case "themes": showPage("themes"); break;
      case "privacy": showPage("privacy"); break;
      case "extensions": showPage("extensions"); break;
      case "theme-dark": setTheme("dark"); toast("Включена тёмная тема"); break;
      case "theme-light": setTheme("light"); toast("Включена светлая тема"); break;
      case "bookmark": toast("Закладка сохранена в прототипе"); break;
      case "protection": toast("Защита отображается в демонстрационном режиме"); break;
      case "clear-data": toast("Очистка будет доступна в полноценной версии"); break;
      default: toast("Этот раздел будет реализован в Sk/Ai Core");
    }
  }
  workspaceContent.addEventListener("click", event => {
    const action = event.target.closest("[data-action]");
    if (action) handleAction(action.dataset.action);
  });

  function runSearch(query) {
    const value = query.trim();
    if (!value) { $("#searchInput")?.focus(); return; }
    const provider = $("#searchProvider")?.value || "Яндекс";
    $("#addressInput").value = value;
    const current = tabs.find(tab => tab.id === activeTabId);
    if (current) current.title = value.length > 22 ? value.slice(0, 22) + "…" : value;
    renderTabs();
    showPage("search", value);
    toast(`Поиск через ${provider}: ${value}`);
  }

  function bindSearchForms() {
    const form = $("#searchForm");
    const input = $("#searchInput");
    if (form && input) form.addEventListener("submit", event => {
      event.preventDefault();
      runSearch(input.value);
    });
  }
  bindSearchForms();

  $("#addressForm").addEventListener("submit", event => {
    event.preventDefault();
    const input = $("#addressInput");
    const value = input.value.trim();
    if (!value) return;
    if (/^(https?:\/\/|[\w-]+\.[a-z]{2,})/i.test(value)) {
      const url = /^https?:\/\//i.test(value) ? value : "https://" + value;
      const current = tabs.find(tab => tab.id === activeTabId);
      if (current) current.title = value.replace(/^https?:\/\//, "").slice(0, 22);
      renderTabs();
      toast("Адрес принят. Загрузка сайтов появится после подключения Chromium");
      input.value = url;
    } else {
      runSearch(value);
    }
  });

  $("#localMode").addEventListener("click", () => setAssistantMode("local"));
  $("#cloudMode").addEventListener("click", () => setAssistantMode("cloud"));
  function setAssistantMode(mode) {
    assistantMode = mode;
    const local = mode === "local";
    $("#localMode").classList.toggle("selected", local);
    $("#cloudMode").classList.toggle("selected", !local);
    $("#cloudMode").classList.toggle("cloud-selected", !local);
    $("#modeBadge").textContent = local ? "LOCAL" : "CLOUD";
    $("#modeBadge").className = "mode-badge " + (local ? "local" : "cloud");
    $("#modelName").textContent = local ? "Qwen3.5 · 2B" : "Облачный режим";
    $("#modelDescription").textContent = local ? "Работает на этом устройстве" : "Провайдер ещё не подключён";
    $("#prototypeNote").textContent = local
      ? "Локальный режим выбран. В этом UI-предпросмотре модель пока не подключена."
      : "Облачный режим — демонстрация интерфейса. Передача данных отключена: провайдер не подключён.";
    toast(local ? "Выбран локальный режим" : "Cloud — режим предпросмотра, отправка отключена");
  }

  function appendMessage(text, type) {
    const node = document.createElement("div");
    node.className = "message " + (type === "user" ? "user-message" : "assistant-message");
    if (type === "user") node.textContent = text;
    else {
      const title = document.createElement("b");
      title.textContent = "Sk/Ai Preview";
      const paragraph = document.createElement("p");
      paragraph.textContent = text;
      node.append(title, paragraph);
    }
    $("#chatStream").append(node);
    $("#chatStream").scrollTop = $("#chatStream").scrollHeight;
  }

  $("#chatForm").addEventListener("submit", event => {
    event.preventDefault();
    const input = $("#chatInput");
    const value = input.value.trim();
    if (!value) return;
    appendMessage(value, "user");
    input.value = "";
    const answer = assistantMode === "cloud"
      ? "Облачный провайдер не подключён. Ничего не отправлено за пределы устройства. Подключение будет добавлено отдельно с явным подтверждением передачи данных."
      : "Это интерактивный предпросмотр интерфейса. Локальная модель Qwen будет подключена к этому окну на следующем этапе разработки.";
    appendMessage(answer, "assistant");
  });
  $("#chatInput").addEventListener("keydown", event => {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      $("#chatForm").requestSubmit();
    }
  });
  $("#chatInput").addEventListener("input", event => {
    event.target.style.height = "auto";
    event.target.style.height = Math.min(event.target.scrollHeight, 94) + "px";
  });

  $("#clearChat").addEventListener("click", () => {
    $("#chatStream").innerHTML = `<div class="welcome-row"><span class="assistant-avatar">✧</span><div class="message assistant-message"><b>Новый диалог</b><p>Контекст очищен в демонстрационном интерфейсе.</p></div></div><div class="prototype-note" id="prototypeNote">Предпросмотр UI. Модель ещё не подключена.</div>`;
    toast("Диалог очищен");
  });
  $("#attachButton").addEventListener("click", () => $("#fileInput").click());
  $("#fileInput").addEventListener("change", event => {
    const file = event.target.files?.[0];
    if (file) toast("Файл выбран: " + file.name + " (чтение пока не подключено)");
    event.target.value = "";
  });
  $("#imageButton").addEventListener("click", () => toast("Добавление изображений появится после подключения AI Core"));
  $("#codeButton").addEventListener("click", () => {
    $("#chatInput").value += "```\n\n```";
    $("#chatInput").focus();
  });

  $("#modelCard").addEventListener("click", () => toast(assistantMode === "local" ? "Qwen3.5 · 2B — выбранная модель предпросмотра" : "Облачный провайдер пока не подключён"));
  $("#protectionCard").addEventListener("click", () => toast("Защитные индикаторы сейчас демонстрационные"));
  $("#assistantSettings").addEventListener("click", () => showPage("settings"));
  $("#bookmarkCurrent").addEventListener("click", () => toast("Страница добавлена в демо-закладки"));
  $("#downloads").addEventListener("click", () => showPage("downloads"));
  $("#browserMenu").addEventListener("click", () => showPage("settings"));
  $("#profileButton").addEventListener("click", () => toast("Профили браузера появятся в Sk/Ai Core"));
  $("#goBack").addEventListener("click", () => toast("История навигации будет подключена к Chromium"));
  $("#goForward").addEventListener("click", () => toast("Вперёд — пока нет следующей страницы"));
  $("#reload").addEventListener("click", () => toast("Обновление страницы станет доступно после интеграции движка"));

  $("#minimizeWindow").addEventListener("click", () => shell.classList.add("is-minimized"));
  $("#restoreWindow").addEventListener("click", () => shell.classList.remove("is-minimized"));
  $("#maximizeWindow").addEventListener("click", () => {
    shell.classList.toggle("is-maximized");
    $("#maximizeWindow").textContent = shell.classList.contains("is-maximized") ? "❐" : "▢";
  });
  $("#closeWindow").addEventListener("click", () => toast("В браузерном окне закрытие будет обрабатываться Windows"));

  renderTabs();
})();