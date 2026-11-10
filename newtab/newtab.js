(() => {
  "use strict";
  const engines={yandex:{name:"Яндекс",url:"https://yandex.ru/search/?text="},google:{name:"Google",url:"https://www.google.com/search?q="},bing:{name:"Bing",url:"https://www.bing.com/search?q="},duckduckgo:{name:"DuckDuckGo",url:"https://duckduckgo.com/?q="},brave:{name:"Brave Search",url:"https://search.brave.com/search?q="},startpage:{name:"Startpage",url:"https://www.startpage.com/sp/search?query="}};
  const $=id=>document.getElementById(id);
  const read=(k,d)=>{try{return localStorage.getItem("yb."+k)||d}catch{return d}};
  const write=(k,v)=>{try{localStorage.setItem("yb."+k,v)}catch{}};
  let engine=engines[read("engine","yandex")]?read("engine","yandex"):"yandex",theme=read("theme","system"),scene=read("scene","aurora"),motion=read("motion","on")!=="off";
  let selectedFile=null,fileText="",busy=false,aiOnline=false,assistantMode=read("assistant-mode","local")==="cloud"?"cloud":"local";
  const conversation=[];
  const applyPrivacyControls=async()=>{
    const filtersOn=read("filters","on")!=="off";
    const thirdPartyOn=read("third-party-cookies","on")!=="off";
    $("filter-toggle").checked=filtersOn;
    $("third-party-cookie-toggle").checked=thirdPartyOn;
    try{
      if(chrome?.declarativeNetRequest){
        if(filtersOn) await chrome.declarativeNetRequest.updateEnabledRulesets({enableRulesetIds:["privacy_filters"],disableRulesetIds:[]});
        else await chrome.declarativeNetRequest.updateEnabledRulesets({enableRulesetIds:[],disableRulesetIds:["privacy_filters"]});
      }
      if(chrome?.privacy?.websites?.thirdPartyCookiesAllowed){
        await chrome.privacy.websites.thirdPartyCookiesAllowed.set({value:!thirdPartyOn});
      }
      $("filter-status").textContent="Фильтры: "+(filtersOn?"включены":"выключены")+" · сторонние cookie: "+(thirdPartyOn?"ограничены":"разрешены");
      const indicator=$("filter-indicator"),detail=$("filter-detail");
      if(indicator)indicator.textContent=filtersOn?"Базовая фильтрация включена":"Фильтрация отключена";
      if(detail)detail.textContent=filtersOn?"Локальные правила для известных рекламных и трекинговых доменов":"Сетевые фильтры не применяются";
    }catch(err){$("filter-status").textContent="Не удалось применить системную настройку: "+err.message;}
  };
  $("filter-toggle").addEventListener("change",e=>{write("filters",e.target.checked?"on":"off");applyPrivacyControls()});
  $("third-party-cookie-toggle").addEventListener("change",e=>{write("third-party-cookies",e.target.checked?"on":"off");applyPrivacyControls()});
  const systemTheme=matchMedia("(prefers-color-scheme: light)");const applyTheme=()=>{document.body.classList.toggle("light",theme==="light"||(theme==="system"&&systemTheme.matches));$("theme-select").value=theme};systemTheme.addEventListener?.("change",()=>{if(theme==="system")applyTheme()});
  const applyMotion=()=>{document.body.classList.toggle("no-motion",!motion);$("motion-toggle").checked=motion};
  const applyScene=()=>{document.body.dataset.scene=scene;$("scene-select").value=scene};
  const updateEngine=()=>{$("engine-quick").innerHTML=engines[engine].name+" <span>⌄</span>";$("engine-name").textContent=engines[engine].name.toUpperCase();$("engine-select").value=engine};
  const show=(id)=>{$(id).hidden=false;document.body.style.overflow="hidden"};
  const hide=(id)=>{$(id).hidden=true;if($("ai-scrim").hidden&&$("settings-scrim").hidden)document.body.style.overflow=""};
  const toast=(s)=>{const el=$("toast");el.textContent=s;el.classList.add("show");setTimeout(()=>el.classList.remove("show"),2800)};
  const openSettings=()=>{show("settings-scrim");$("theme-select").value=theme};
  $("settings-open").addEventListener("click",openSettings);$("settings-close").addEventListener("click",()=>hide("settings-scrim"));
  $("settings-scrim").addEventListener("click",e=>{if(e.target===$("settings-scrim"))hide("settings-scrim")});
  const openAssistant=()=>{
    if(!chrome.sidePanel?.open){toast("В этой сборке Chromium-Gost не обнаружена поддержка встроенной боковой панели.");return;}
    chrome.sidePanel.open({windowId:chrome.windows.WINDOW_ID_CURRENT}).catch(err=>toast("Sk/Ai Assistant: "+err.message));
  };
  $("ai-close").addEventListener("click",()=>hide("ai-scrim"));
  $("ai-scrim").addEventListener("click",e=>{if(e.target===$("ai-scrim"))hide("ai-scrim")});
  $("privacy-open").addEventListener("click",openSettings);
  $("appearance-open").addEventListener("click",openSettings);
  $("engine-quick").addEventListener("click",openSettings);
  $("bookmarks-open").addEventListener("click",()=>chrome.tabs.create({url:"chrome://bookmarks/"}));
  $("extensions-open").addEventListener("click",()=>chrome.tabs.create({url:"chrome://extensions/"}));
  document.querySelectorAll(".rail-btn").forEach(button=>button.addEventListener("click",()=>{
    const action=button.dataset.action;
    if(action==="assistant"){openAssistant();return;}
    if(action==="history"){chrome.tabs.create({url:"chrome://history/"});return;}
    if(action==="downloads"){chrome.tabs.create({url:"chrome://downloads/"});return;}
    if(action==="bookmarks"){chrome.tabs.create({url:"chrome://bookmarks/"});return;}
    if(action==="extensions"){chrome.tabs.create({url:"chrome://extensions/"});return;}
    if(action==="privacy"||action==="settings"||action==="appearance"){openSettings();return;}
    if(action==="home"){window.scrollTo({top:0,behavior:motion?"smooth":"auto"});}
  }));
  document.addEventListener("keydown",e=>{if(e.key==="Escape"){hide("ai-scrim");hide("settings-scrim")}if((e.ctrlKey||e.metaKey)&&e.key===","){e.preventDefault();openSettings()}if(e.key==="/"&&!["INPUT","TEXTAREA"].includes(document.activeElement.tagName)){e.preventDefault();$("query").focus()}if((e.ctrlKey||e.metaKey)&&e.key==="Enter"&&!$("ai-scrim").hidden)sendMessage()});
  $("search-form").addEventListener("submit",e=>{e.preventDefault();const q=$("query").value.trim();if(q)location.href=engines[engine].url+encodeURIComponent(q);else $("query").focus()});
  $("engine-select").addEventListener("change",e=>{engine=e.target.value;write("engine",engine);updateEngine()});
  $("theme-select").addEventListener("change",e=>{theme=e.target.value;write("theme",theme);applyTheme()});
  $("motion-toggle").addEventListener("change",e=>{motion=e.target.checked;write("motion",motion?"on":"off");applyMotion()});
  $("scene-select").addEventListener("change",e=>{scene=e.target.value;write("scene",scene);applyScene()});
  const setAssistantMode=(next)=>{
    assistantMode=next;write("assistant-mode",next);
    const local=next==="local";
    $("mode-local").classList.toggle("active",local);$("mode-cloud").classList.toggle("active",!local);
    $("mode-local").setAttribute("aria-pressed",String(local));$("mode-cloud").setAttribute("aria-pressed",String(!local));
    $("ai-scrim").classList.toggle("cloud-mode",!local);
    $("ai-scrim").querySelector(".model-strip").classList.toggle("local-mode",local);
    $("ai-scrim").querySelector(".model-strip").classList.toggle("cloud-mode",!local);
    $("ai-title").innerHTML=local?'Qwen <span>Assistant</span>':'Cloud <span>Assistant</span>';
    $("ai-scrim").querySelector(".model-strip b").textContent=local?"Qwen3.5 · 2B":"Облачная модель";
    $("ai-scrim").querySelector(".model-strip small").textContent=local?"Работает на этом устройстве":"Провайдер API ещё не подключён";
    $("mode-badge").textContent=local?"LOCAL":"CLOUD";
    $("mode-explainer").textContent=local?"Локальная модель продолжит диалог с общей историей. Сообщения остаются на устройстве.":"История диалога сохранена и будет передана облачной модели после подключения провайдера. Перед отправкой будет показан состав передаваемых данных.";
    $("ai-prompt").placeholder=local?"Продолжить диалог с Qwen…":"Продолжить диалог в облаке…";
    $("privacy-caption").innerHTML=local?'<i></i> ОБРАБОТКА НА УСТРОЙСТВЕ':'<i></i> ПЕРЕДАЧА ПОСЛЕ ПОДТВЕРЖДЕНИЯ';
    $("attach-label").title=local?"Добавить текстовый файл":"Вложения пока доступны только локально";
  };
  $("mode-local").addEventListener("click",()=>setAssistantMode("local"));
  $("mode-cloud").addEventListener("click",()=>setAssistantMode("cloud"));
  const health=async()=>{try{const r=await fetch("http://127.0.0.1:8765/health",{cache:"no-store",signal:AbortSignal.timeout(1200)});aiOnline=r.ok}catch{aiOnline=false}const label=aiOnline?"Локальное ядро готово":"Локальное ядро запускается";if($("ai-status"))$("ai-status").textContent=label;if($("ai-dot"))$("ai-dot").classList.toggle("offline",!aiOnline);if($("settings-ai-status"))$("settings-ai-status").textContent=aiOnline?"Qwen готов к работе":"Qwen не отвечает";if($("settings-ai-dot"))$("settings-ai-dot").classList.toggle("green",aiOnline)};
  const addMessage=(role,text,extra="")=>{const area=$("chat-area");$("welcome").hidden=true;const el=document.createElement("div");el.className="message "+role+(extra?" "+extra:"");const label=document.createElement("span");label.className="message-label";label.textContent=role==="user"?"ВЫ":"QWEN";el.append(label,document.createTextNode(text));area.append(el);area.scrollTop=area.scrollHeight;return el};
  const sendMessage=async()=>{
    if(busy)return;
    const prompt=$("ai-prompt").value.trim();
    if(!prompt&&!fileText)return;
    if(assistantMode==="cloud"){
      toast("История сохранена. Для облачного режима сначала подключи провайдера API — ничего не отправлено.");
      return;
    }
    $("ai-prompt").value="";
    const userText=(fileText?"Контекст из файла ("+(selectedFile?.name||"документ")+"):\\n"+fileText.slice(0,4500)+"\\n\\n":"")+prompt;
    addMessage("user",prompt||(selectedFile?"Проанализируй прикреплённый файл: "+selectedFile.name:""));
    conversation.push({role:"user",content:userText});
    const placeholder=addMessage("assistant","Подготавливаю ответ…");
    busy=true;$("ai-send").disabled=true;
    try{
      const messages=[{role:"system",content:"Ты локальный помощник внутри браузера Sk/Ai. Отвечай по-русски, ясно и по делу. Сохраняй контекст всего диалога. Не выдумывай факты. Учитывай ограничение контекста."},...conversation.slice(-18)];
      const response=await fetch("http://127.0.0.1:8765/v1/chat/completions",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({model:"qwen-local",messages,temperature:0.65,max_tokens:480,stream:false}),signal:AbortSignal.timeout(180000)});
      if(!response.ok)throw new Error("Локальный движок вернул HTTP "+response.status);
      const data=await response.json();const answer=data.choices?.[0]?.message?.content||"Модель не вернула текстовый ответ.";
      placeholder.lastChild.textContent=answer;conversation.push({role:"assistant",content:answer});
    }catch(err){
      conversation.pop();placeholder.classList.add("error");
      placeholder.lastChild.textContent=err.name==="TimeoutError"?"Модель отвечает слишком долго. Попробуйте сократить запрос.":"Не удалось связаться с локальным Qwen. ("+err.message+")";
    }finally{busy=false;$("ai-send").disabled=false;$("chat-area").scrollTop=$("chat-area").scrollHeight;health()}
  };
  $("ai-send").addEventListener("click",sendMessage);$("ai-prompt").addEventListener("keydown",e=>{if(e.key==="Enter"&&!e.shiftKey){e.preventDefault();sendMessage()}});
  document.querySelectorAll(".suggestions button").forEach(b=>b.addEventListener("click",()=>{$("ai-prompt").value=b.dataset.prompt;$("ai-prompt").focus()}));
  $("file-input").addEventListener("change",async e=>{const file=e.target.files?.[0];if(!file)return;if(file.size>300000){toast("Файл больше 300 КБ. Разделите его на части.");e.target.value="";return}try{fileText=await file.text();selectedFile=file;$("file-name").textContent=file.name;$("attachment").hidden=false;toast("Файл добавлен в локальный контекст.")}catch{toast("Не удалось прочитать файл.")}});
  const removeFile=()=>{selectedFile=null;fileText="";$("file-input").value="";$("attachment").hidden=true};$("file-remove").addEventListener("click",removeFile);
  updateEngine();applyTheme();applyScene();applyMotion();applyPrivacyControls();setAssistantMode(assistantMode);$("query").focus({preventScroll:true});health();setInterval(health,3500);
})();