(() => {
  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
  const browser = $("#browserWindow");
  const panel = $("#assistantPanel");
  const toast = $("#toast");
  let mode = "local";
  let scene = "aurora";
  let engine = "Яндекс";
  let toastTimer;

  function notify(message) {
    toast.textContent = message;
    toast.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toast.classList.remove("show"), 2200);
  }

  function showAssistant() {
    browser.classList.remove("assistant-collapsed", "assistant-floating");
    panel.classList.remove("mobile-hidden");
  }

  $("#assistantRail").addEventListener("click", showAssistant);
  $("#openAssistant").addEventListener("click", showAssistant);
  $("#reopenAssistant").addEventListener("click", showAssistant);
  $("#hideAssistant").addEventListener("click", () => {
    browser.classList.add("assistant-collapsed");
    browser.classList.remove("assistant-floating");
  });
  $("#collapseAssistant").addEventListener("click", () => {
    browser.classList.toggle("assistant-collapsed");
    browser.classList.remove("assistant-floating");
  });
  $("#floatAssistant").addEventListener("click", () => {
    browser.classList.toggle("assistant-floating");
    browser.classList.remove("assistant-collapsed");
    notify(browser.classList.contains("assistant-floating")
      ? "Демонстрация плавающего режима помощника"
      : "Помощник закреплён справа");
  });

  $$(".mode-option").forEach(button => button.addEventListener("click", () => {
    mode = button.dataset.mode;
    $$(".mode-option").forEach(item => item.classList.toggle("active", item === button));
    $("#modelFootnote").textContent = mode === "local"
      ? "Qwen · работает на этом устройстве"
      : "Облачный режим · перед отправкой потребуется подтверждение";
    notify(mode === "local" ? "Выбран локальный режим" : "Выбран облачный режим");
  }));

  function sendPrompt(value) {
    const prompt = value.trim();
    if (!prompt) return;
    const chat = $("#chatArea");
    const intro = $(".assistant-intro", chat);
    const suggestions = $(".suggestion-list", chat);
    if (intro) intro.remove();
    if (suggestions) suggestions.remove();

    const userMessage = document.createElement("div");
    userMessage.className = "chat-message user";
    userMessage.textContent = prompt;
    chat.append(userMessage);

    const reply = document.createElement("div");
    reply.className = "chat-message assistant";
    reply.textContent = mode === "local"
      ? "Это демонстрационный интерфейс Sk/Ai. На следующем этапе подключим локальную модель Qwen и передачу контекста с явного разрешения."
      : "Облачный режим выбран. В рабочей версии перед передачей запроса появится окно согласия с перечнем данных, которые покинут устройство.";
    chat.append(reply);
    chat.scrollTop = chat.scrollHeight;
    $("#promptInput").value = "";
  }

  $("#sendPrompt").addEventListener("click", () => sendPrompt($("#promptInput").value));
  $("#promptInput").addEventListener("keydown", event => {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      sendPrompt(event.currentTarget.value);
    }
  });
  $$(".suggestion").forEach(button => button.addEventListener("click", () => {
    $("#promptInput").value = button.dataset.prompt;
    $("#promptInput").focus();
  }));

  $("#sharePage").addEventListener("click", () => {
    $(".assistant-context small").textContent = "Демо-контекст · не подключён";
    notify("Подключение реальной вкладки появится в интеграции с Chromium");
  });

  $("#themeButton").addEventListener("click", () => {
    const isLight = document.body.dataset.theme !== "light";
    document.body.dataset.theme = isLight ? "light" : "dark";
    notify(isLight ? "Светлая тема" : "Тёмная тема");
  });

  $("#editShortcuts").addEventListener("click", () => notify("Редактор быстрых ссылок — следующий этап"));
  $("#addShortcut").addEventListener("click", () => notify("Добавление пользовательских ссылок — следующий этап"));
  $("#bookmarkButton").addEventListener("click", event => {
    event.currentTarget.textContent = event.currentTarget.textContent === "☆" ? "★" : "☆";
    notify(event.currentTarget.textContent === "★" ? "Добавлено в избранное (демо)" : "Убрано из избранного (демо)");
  });

  $("#engineButton").addEventListener("click", () => {
    const engines = ["Яндекс", "Google", "Bing", "DuckDuckGo", "Brave", "Startpage"];
    engine = engines[(engines.indexOf(engine) + 1) % engines.length];
    $("#engineButton").innerHTML = engine + " <span>⌄</span>";
    $("#currentEngine").textContent = engine;
    notify("Поисковая система: " + engine);
  });

  function navigate(value) {
    const query = value.trim();
    if (!query) return;
    const url = /^(https?:\/\/|skai:\/\/)/i.test(query)
      ? query
      : /\.[a-z]{2,}(\/|$)/i.test(query)
        ? "https://" + query
        : "https://www.google.com/search?q=" + encodeURIComponent(query);
    $("#addressInput").value = url;
    notify("В прототипе навигация не подключена к браузерному движку");
  }
  $("#homeSearch").addEventListener("keydown", event => {
    if (event.key === "Enter") navigate(event.currentTarget.value);
  });
  $("#addressInput").addEventListener("keydown", event => {
    if (event.key === "Enter") navigate(event.currentTarget.value);
  });
  $$(".shortcut-card[data-url]").forEach(card => card.addEventListener("click", () => {
    $("#addressInput").value = card.dataset.url;
    notify("Переход будет работать после подключения Chromium");
  }));

  $("#newTab").addEventListener("click", () => {
    const tab = document.createElement("button");
    tab.className = "tab active";
    tab.setAttribute("role", "tab");
    tab.setAttribute("aria-selected", "true");
    tab.dataset.title = "Новая вкладка";
    tab.innerHTML = '<span class="tab-favicon home-favicon">✳</span><span class="tab-name">Новая вкладка</span><span class="tab-close">×</span>';
    $$(".tab").forEach(item => item.classList.remove("active"));
    $("#tabs").append(tab);
    tab.addEventListener("click", () => activateTab(tab));
    $(".tab-close", tab).addEventListener("click", event => {
      event.stopPropagation();
      tab.remove();
      notify("Вкладка закрыта");
    });
    notify("Создана демонстрационная вкладка");
  });

  function activateTab(tab) {
    $$(".tab").forEach(item => {
      item.classList.toggle("active", item === tab);
      item.setAttribute("aria-selected", String(item === tab));
    });
  }
  $$(".tab").forEach(tab => {
    tab.addEventListener("click", () => activateTab(tab));
    $(".tab-close", tab)?.addEventListener("click", event => {
      event.stopPropagation();
      tab.remove();
      notify("Вкладка закрыта");
    });
  });

  const now = new Date();
  const months = ["ЯНВАРЯ","ФЕВРАЛЯ","МАРТА","АПРЕЛЯ","МАЯ","ИЮНЯ","ИЮЛЯ","АВГУСТА","СЕНТЯБРЯ","ОКТЯБРЯ","НОЯБРЯ","ДЕКАБРЯ"];
  const weekdays = ["ВОСКРЕСЕНЬЕ","ПОНЕДЕЛЬНИК","ВТОРНИК","СРЕДА","ЧЕТВЕРГ","ПЯТНИЦА","СУББОТА"];
  $("#dateLabel").textContent = weekdays[now.getDay()] + " · " + now.getDate() + " " + months[now.getMonth()];
})();
