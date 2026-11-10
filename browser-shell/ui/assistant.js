(() => {
 "use strict";
 const $=s=>document.querySelector(s);
 const requestedProfile=new URLSearchParams(window.location.search).get("profile")||"default";
 const activeProfile=/^[a-z0-9_-]{1,24}$/.test(requestedProfile)?requestedProfile:"default";
 const conversationKey="skai.conversation.v1."+activeProfile;
 const modeKey="skai.assistant.mode.v1."+activeProfile;
 let mode="local", messages=[];
 try { mode=localStorage.getItem(modeKey)||"local"; } catch (_) {}
 try { messages=JSON.parse(localStorage.getItem(conversationKey)||"[]"); } catch (_) {}
 const chat=$("#chat");
 function render(){
  chat.querySelectorAll(".chat-message").forEach(n=>n.remove());
  messages.forEach(item=>{
   const node=document.createElement("div");
   node.className="chat-message "+(item.role==="user"?"user":"assistant");
   node.textContent=item.text||"";
   chat.append(node);
  });
  chat.scrollTop=chat.scrollHeight;
 }
 function setMode(value){
  mode=value;
  document.querySelectorAll("[data-mode]").forEach(b=>b.classList.toggle("active",b.dataset.mode===mode));
  $("#note").textContent=mode==="local"?"Локальная модель не подключена; запросы не отправляются":"Облачная модель не подключена; передача отключена";
  try { localStorage.setItem(modeKey,mode); } catch (_) {}
 }
 setMode(mode); render();
 document.querySelectorAll("[data-mode]").forEach(b=>b.onclick=()=>setMode(b.dataset.mode));
 $("#dock").onclick=()=>window.skaiNative?.close();$("#min").onclick=()=>window.skaiNative?.minimize();
 $("#max").onclick=()=>window.skaiNative?.toggleMaximize();
 $("#close").onclick=()=>window.skaiNative?.close();
 function send(){
  const value=$("#prompt").value.trim(); if(!value)return;
  messages.push({role:"user",text:value});
  messages.push({role:"assistant",text:mode==="local"
   ?"Локальная модель Qwen ещё не подключена. Запрос не отправлен."
   :"Облачный провайдер не подключён. Данные не отправлены."});
  messages=messages.slice(-200);
  try { localStorage.setItem(conversationKey,JSON.stringify(messages)); } catch (_) {}
  $("#prompt").value=""; render();
 }
 $("#send").onclick=send;
 $("#prompt").addEventListener("keydown",e=>{if(e.key==="Enter"&&!e.shiftKey){e.preventDefault();send();}});
 window.addEventListener("storage",e=>{
  if(e.key===conversationKey){try{messages=JSON.parse(e.newValue||"[]");}catch(_){messages=[];}render();}
  if(e.key===modeKey)setMode(e.newValue||"local");
 });
})();