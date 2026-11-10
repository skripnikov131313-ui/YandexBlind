(() => {
  "use strict";
  const engines={yandex:{name:"Яндекс",url:"https://yandex.ru/search/?text="},google:{name:"Google",url:"https://www.google.com/search?q="},bing:{name:"Bing",url:"https://www.bing.com/search?q="},duckduckgo:{name:"DuckDuckGo",url:"https://duckduckgo.com/?q="},brave:{name:"Brave Search",url:"https://search.brave.com/search?q="},startpage:{name:"Startpage",url:"https://www.startpage.com/sp/search?query="}};
  const $=id=>document.getElementById(id);
  const read=(k,d)=>{try{return localStorage.getItem("yb."+k)||d}catch{return d}};
  const write=(k,v)=>{try{localStorage.setItem("yb."+k,v)}catch{}};
  let engine=engines[read("engine","yandex")]?read("engine","yandex"):"yandex",theme=read("theme","system"),scene=read("scene","aurora"),motion=read("motion","on")!=="off";
  let aiOnline=false;
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
  const hide=(id)=>{$(id).hidden=true;if($("settings-scrim").hidden)document.body.style.overflow=""};
  const toast=(s)=>{const el=$("toast");el.textContent=s;el.classList.add("show");setTimeout(()=>el.classList.remove("show"),2800)};
  const openSettings=()=>{show("settings-scrim");$("theme-select").value=theme};
  $("settings-open").addEventListener("click",openSettings);$("settings-close").addEventListener("click",()=>hide("settings-scrim"));
  $("settings-scrim").addEventListener("click",e=>{if(e.target===$("settings-scrim"))hide("settings-scrim")});
  const openAssistant=()=>{
    if(!chrome.sidePanel?.open){toast("В этой сборке Chromium-Gost не обнаружена поддержка встроенной боковой панели.");return;}
    chrome.sidePanel.open({windowId:chrome.windows.WINDOW_ID_CURRENT}).catch(err=>toast("Sk/Ai Assistant: "+err.message));
  };
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
  document.addEventListener("keydown",e=>{if(e.key==="Escape")hide("settings-scrim");if((e.ctrlKey||e.metaKey)&&e.key===","){e.preventDefault();openSettings()}if(e.key==="/"&&!["INPUT","TEXTAREA"].includes(document.activeElement.tagName)){e.preventDefault();$("query").focus()}});
  $("search-form").addEventListener("submit",e=>{e.preventDefault();const q=$("query").value.trim();if(q)location.href=engines[engine].url+encodeURIComponent(q);else $("query").focus()});
  $("engine-select").addEventListener("change",e=>{engine=e.target.value;write("engine",engine);updateEngine()});
  $("theme-select").addEventListener("change",e=>{theme=e.target.value;write("theme",theme);applyTheme()});
  $("motion-toggle").addEventListener("change",e=>{motion=e.target.checked;write("motion",motion?"on":"off");applyMotion()});
  $("scene-select").addEventListener("change",e=>{scene=e.target.value;write("scene",scene);applyScene()});
  const health=async()=>{try{const r=await fetch("http://127.0.0.1:8765/health",{cache:"no-store",signal:AbortSignal.timeout(1200)});aiOnline=r.ok}catch{aiOnline=false}if($("settings-ai-status"))$("settings-ai-status").textContent=aiOnline?"Qwen готов к работе":"Qwen не отвечает";if($("settings-ai-dot"))$("settings-ai-dot").classList.toggle("green",aiOnline)};
  updateEngine();applyTheme();applyScene();applyMotion();applyPrivacyControls();$("query").focus({preventScroll:true});health();setInterval(health,10000);
})();