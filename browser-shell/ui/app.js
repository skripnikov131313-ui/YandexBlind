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
  let mode = "local";
  let toastTimer = 0;

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
    browser.classList.toggle("assistant-floating");
    browser.classList.remove("assistant-collapsed");
    notify(browser.classList.contains("assistant-floating") ? "Режим отдельной панели (демо)" : "Помощник закреплён справа");
  });

  $$(".mode-option").forEach(button => button.addEventListener("click", () => {
    mode = button.dataset.mode;
    $$(".mode-option").forEach(item => item.classList.toggle("active", item === button));
    const note = $("#modelFootnote");
    if (note) note.textContent = mode === "local"
      ? "Работает на этом устройстве"
      : "Облако · перед отправкой потребуется согласие";
    const badge = $(".local-badge");
    if (badge) {
      badge.textContent = mode === "local" ? "LOCAL" : "CLOUD";
      badge.classList.toggle("cloud-badge", mode === "cloud");
    }
    notify(mode === "local" ? "Выбран локальный ИИ" : "Выбрано облако. Передача данных пока отключена.");
  }));

  function addMessage(kind, value) {
    const chat = $("#chatArea");
    $(".assistant-intro", chat)?.remove();
    $(".suggestion-list", chat)?.remove();
    const message = document.createElement("div");
    message.className = "chat-message " + kind;
    message.textContent = value;
    chat.append(message);
    chat.scrollTop = chat.scrollHeight;
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

  function cycleProvider() {
    providerIndex = (providerIndex + 1) % providers.length;
    $("#engineButton").innerHTML = providers[providerIndex].name + " <span>⌄</span>";
    $("#currentEngine").textContent = providers[providerIndex].name;
    notify("Поисковая система: " + providers[providerIndex].name);
  }
  $("#engineButton")?.addEventListener("click", cycleProvider);
  function search(value) {
    const query = value.trim();
    if (!query) return;
    const looksLikeUrl = /^(https?:\/\/|localhost(?::\d+)?|[\w-]+\.[a-z]{2,})(\/.*)?$/i.test(query);
    const target = looksLikeUrl ? (/^https?:\/\//i.test(query) ? query : "https://" + query) : providers[providerIndex].url(query);
    $("#addressInput").value = target;
    notify("Навигация станет доступна после подключения Chromium");
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
  $("#reloadPage")?.addEventListener("click", () => window.skaiNative?.reload());

  $("#themeButton")?.addEventListener("click", () => {
    document.body.dataset.theme = document.body.dataset.theme === "light" ? "dark" : "light";
    notify(document.body.dataset.theme === "light" ? "Светлая тема" : "Тёмная тема");
  });
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

  function activateTab(tab) {
    $$(".tab").forEach(item => {
      const active = item === tab;
      item.classList.toggle("active", active);
      item.setAttribute("aria-selected", String(active));
    });
  }
  function bindTab(tab) {
    tab.addEventListener("click", event => {
      if (event.target.closest(".tab-close")) {
        event.stopPropagation();
        const wasActive = tab.classList.contains("active");
        tab.remove();
        if (wasActive) {
          const remaining = $$(".tab");
          if (remaining.length) activateTab(remaining[remaining.length - 1]);
        }
        return;
      }
      activateTab(tab);
    });
  }
  $$(".tab").forEach(bindTab);
  $("#newTab")?.addEventListener("click", () => {
    const tab = document.createElement("button");
    tab.className = "tab active";
    tab.setAttribute("role", "tab");
    tab.setAttribute("aria-selected", "true");
    tab.innerHTML = '<span class="tab-favicon">◉</span><span class="tab-name">Новая вкладка</span><span class="tab-close">×</span>';
    $$(".tab").forEach(item => item.classList.remove("active"));
    $("#tabs").append(tab);
    bindTab(tab);
    notify("Новая вкладка (демо)");
  });
  $(".rail-toggle")?.addEventListener("click", () => {
    document.body.dataset.theme = document.body.dataset.theme === "light" ? "dark" : "light";
  });
  $(".rail-item.moon")?.addEventListener("click", () => {
    document.body.dataset.theme = document.body.dataset.theme === "light" ? "dark" : "light";
  });
})();