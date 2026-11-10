(() => {
 "use strict";

 const $ = selector => document.querySelector(selector);
 const requestedProfile = new URLSearchParams(window.location.search).get("profile") || "default";
 const activeProfile = /^[a-z0-9_-]{1,24}$/.test(requestedProfile) ? requestedProfile : "default";
 const conversationKey = "skai.conversation.v1." + activeProfile;
 const modeKey = "skai.assistant.mode.v1." + activeProfile;
 const chat = $("#chat");

 let mode = "local";
 let messages = [];
 let modelState = "checking";
 let modelDetail = "Проверяем локальную модель…";
 let modelProgress = 0;
 let requestPending = "";
 let requestSequence = 0;

 try {
  mode = localStorage.getItem(modeKey) === "cloud" ? "cloud" : "local";
 } catch (_) {}
 try {
  const saved = JSON.parse(localStorage.getItem(conversationKey) || "[]");
  if (Array.isArray(saved)) messages = saved.slice(-200);
 } catch (_) {}

 function persistMessages() {
  try { localStorage.setItem(conversationKey, JSON.stringify(messages.slice(-200))); } catch (_) {}
 }
 function render() {
  chat.querySelectorAll(".chat-message").forEach(node => node.remove());
  messages.forEach(item => {
   const node = document.createElement("div");
   node.className = "chat-message " + (item.role === "user" ? "user" : "assistant");
   if (item.pending) {
    node.classList.add("pending");
    node.textContent = "Запрос был прерван при закрытии браузера.";
   } else {
    node.textContent = item.text || "";
   }
   chat.append(node);
  });
  chat.scrollTop = chat.scrollHeight;
 }
 function renderStatus() {
  const note = $("#note");
  const install = $("#install-model");
  const dot = $("#model-status");
  if (note) {
   note.textContent = mode === "cloud"
    ? "Облачный провайдер не подключён · передача отключена"
    : modelDetail;
  }
  if (install) {
   install.hidden = mode !== "local" || modelState === "ready";
   install.disabled = ["checking", "downloading", "loading"].includes(modelState);
   install.textContent = modelState === "downloading"
    ? "Загрузка " + modelProgress + "%"
    : modelState === "checking" ? "Проверка…"
    : modelState === "loading" ? "Запуск…"
    : modelState === "error" || modelState === "invalid" || modelState === "cancelled"
      ? "Повторить" : "Установить · 1,3 ГБ";
  }
  if (dot) {
   dot.className = "";
   dot.classList.add(
    modelState === "ready" ? "ready" :
    modelState === "downloading" || modelState === "loading" ? "downloading" :
    modelState === "error" || modelState === "invalid" ? "error" : "pending"
   );
  }
 }
 function setMode(value, persist = true) {
  mode = value === "cloud" ? "cloud" : "local";
  document.querySelectorAll("[data-mode]").forEach(button => {
   button.classList.toggle("active", button.dataset.mode === mode);
  });
  if (persist) {
   try { localStorage.setItem(modeKey, mode); } catch (_) {}
  }
  renderStatus();
 }
 function addMessage(role, text, pending = false, requestId = "") {
  const item = { role, text };
  if (pending) {
   item.pending = true;
   item.requestId = requestId;
  }
  messages.push(item);
  messages = messages.slice(-200);
  persistMessages();
  render();
 }
 function makeLocalRequest() {
  return {
   model: "Qwen3.5-2B",
   messages: messages
    .filter(item => item && !item.pending && ["user", "assistant"].includes(item.role))
    .slice(-20)
    .map(item => ({ role: item.role, content: String(item.text || "") })),
   stream: false,
   temperature: 0.7,
   max_tokens: 1024
  };
 }
 function send() {
  const input = $("#prompt");
  const value = input.value.trim();
  if (!value || requestPending) return;
  input.value = "";
  addMessage("user", value);

  if (mode === "cloud") {
   addMessage("assistant", "Облачный провайдер пока не подключён. Запрос и контекст не отправлены.");
   return;
  }
  if (modelState !== "ready") {
   addMessage("assistant", "Сначала установите Qwen 3.5 2B в карточке модели. Запрос не отправлялся.");
   return;
  }

  const requestId = activeProfile + "-" + Date.now() + "-" + (++requestSequence);
  addMessage("assistant", "Обрабатываю запрос локально…", true, requestId);
  requestPending = requestId;
  $("#send").disabled = true;
  if (!window.skaiNative?.sendLocalAi) {
   window.skaiNative?.localAiResult?.(requestId, "Нативный модуль локального ИИ не подключён.", true);
   return;
  }
  window.skaiNative.sendLocalAi(requestId, JSON.stringify(makeLocalRequest()));
 }

 function updatePendingResult(requestId, text, isError) {
  const id = String(requestId || "");
  if (!id || requestPending !== id) return;
  requestPending = "";
  $("#send").disabled = false;
  const pendingIndex = messages.findIndex(item =>
   item?.role === "assistant" && item?.pending && item?.requestId === id
  );
  if (pendingIndex >= 0) {
   messages[pendingIndex].text = String(text || "Пустой ответ локальной модели.");
   delete messages[pendingIndex].pending;
   delete messages[pendingIndex].requestId;
  }
  persistMessages();
  render();
  if (isError) {
   modelDetail = String(text || "Ошибка локальной модели.");
   renderStatus();
  }
 }

 setMode(mode, false);
 render();
 document.querySelectorAll("[data-mode]").forEach(button => {
  button.addEventListener("click", () => setMode(button.dataset.mode));
 });

 $("#install-model")?.addEventListener("click", () => window.skaiNative?.installLocalModel());
 $("#dock").onclick = () => window.skaiNative?.close();
 $("#min").onclick = () => window.skaiNative?.minimize();
 $("#max").onclick = () => window.skaiNative?.toggleMaximize();
 $("#close").onclick = () => window.skaiNative?.close();
 $("#send").onclick = send;
 $("#prompt").addEventListener("keydown", event => {
  if (event.key === "Enter" && !event.shiftKey) {
   event.preventDefault();
   send();
  }
 });

 window.skaiNative = window.skaiNative || {};
 window.skaiNative.localAiStatus = (status, percent, detail) => {
  modelState = String(status || "error");
  modelProgress = Math.max(0, Math.min(100, Number(percent) || 0));
  modelDetail = String(detail || "Статус локальной модели обновлён.");
  renderStatus();
 };
 window.skaiNative.localAiResult = (requestId, text, isError) => {
  updatePendingResult(requestId, String(text || ""), !!isError);
 };

 window.addEventListener("storage", event => {
  if (event.key === conversationKey) {
   try {
    const next = JSON.parse(event.newValue || "[]");
    messages = Array.isArray(next) ? next.slice(-200) : [];
   } catch (_) { messages = []; }
   render();
  }
  if (event.key === modeKey) setMode(event.newValue, false);
 });

 window.skaiNative?.aiStatus();
 renderStatus();
})();