#ifndef VAULT_WEB_H
#define VAULT_WEB_H

#include <FS.h>
using namespace fs;
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"

// ---- extern from vault_core.h ----
extern Credential creds[];
extern int        credCount;
extern bool addCredential(const char*, const char*, const char*);
extern bool updateCredential(int, const char*, const char*, const char*);
extern bool deleteCredential(int);

WebServer server(80);
bool      webRunning = false;
char      webPass[AP_PASS_LEN + 1] = "";   // senha do AP, nova a cada ativacao
unsigned long webLastRequest = 0;          // para o desligamento por inatividade

// Marca atividade e impede cache de respostas com dados sensiveis
void webTouch() {
  webLastRequest = millis();
  server.sendHeader("Cache-Control", "no-store");
}

// ---- JSON escape ----
String jsonEsc(const char* s) {
  String out;
  while (*s) {
    switch (*s) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:   out += *s;
    }
    s++;
  }
  return out;
}

// ---- HTML SPA (PROGMEM) ----
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PassVault</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#111;color:#e0e0e0;
     max-width:420px;margin:0 auto;padding:16px}
h1{text-align:center;color:#0ff;margin-bottom:18px;font-size:1.5em}
.card{background:#1a1a2e;border:1px solid #333;border-radius:10px;padding:14px;
      margin-bottom:10px;cursor:pointer;transition:background .2s}
.card:hover{background:#16213e}
.card .name{font-weight:bold;color:#0f0;font-size:1.1em}
.card .user{color:#aaa;font-size:.85em;margin-top:2px}
#form-area{display:none;background:#1a1a2e;border:1px solid #333;
           border-radius:10px;padding:16px;margin-bottom:14px}
input{width:100%;padding:10px;margin:6px 0;border:1px solid #444;
      border-radius:6px;background:#222;color:#eee;font-size:1em}
input:focus{outline:none;border-color:#0ff}
.btn{display:inline-block;padding:10px 18px;border:none;border-radius:6px;
     font-size:.95em;cursor:pointer;margin:4px;transition:background .2s}
.btn-ok{background:#0a0;color:#fff}
.btn-ok:hover{background:#0c0}
.btn-cancel{background:#555;color:#fff}
.btn-cancel:hover{background:#777}
.btn-del{background:#a00;color:#fff}
.btn-del:hover{background:#c00}
.btn-add{background:#06b;color:#fff;width:100%;padding:12px;font-size:1em;
         margin-top:8px;border-radius:8px;border:none;cursor:pointer}
.btn-add:hover{background:#08d}
#msg{text-align:center;padding:8px;margin:8px 0;border-radius:6px;display:none}
.ok-msg{background:#062;color:#0f0}
.err-msg{background:#400;color:#f44}
.detail-area{display:none;background:#1a1a2e;border:1px solid #333;
             border-radius:10px;padding:16px;margin-bottom:14px}
.detail-area .field{margin:8px 0}
.detail-area .label{color:#0ff;font-size:.8em}
.pass-row{display:flex;gap:6px;align-items:center}
.pass-row input{flex:1}
.btn-gen{background:#a60;color:#fff;margin:6px 0}
.btn-gen:hover{background:#c80}
.gen-opts{font-size:.85em;color:#aaa;margin:4px 0 6px;display:flex;flex-wrap:wrap;
          gap:4px 12px;align-items:center}
.gen-opts input[type=checkbox]{width:auto;margin:0 4px 0 0}
.gen-opts input[type=number]{width:60px;padding:4px;margin:0 0 0 4px}
.detail-area .value{color:#eee;font-size:1em;word-break:break-all;
                     background:#222;padding:8px;border-radius:6px;margin-top:2px}
</style>
</head>
<body>
<h1>&#128274; PassVault</h1>
<div id="msg"></div>
<div id="detail" class="detail-area">
  <div class="field"><div class="label">Nome</div><div class="value" id="d-name"></div></div>
  <div class="field"><div class="label">Usu&aacute;rio</div><div class="value" id="d-user"></div></div>
  <div class="field"><div class="label">Senha</div><div class="value" id="d-pass"></div></div>
  <div style="margin-top:12px;text-align:center">
    <button class="btn btn-ok" onclick="editCred()">Editar</button>
    <button class="btn btn-del" onclick="delCred()">Excluir</button>
    <button class="btn btn-cancel" onclick="closeDetail()">Voltar</button>
  </div>
</div>
<div id="form-area">
  <input id="f-name" placeholder="Nome (ex: Gmail)" maxlength="32">
  <input id="f-user" placeholder="Usu&aacute;rio / Email" maxlength="64">
  <div class="pass-row">
    <input id="f-pass" placeholder="Senha" maxlength="64">
    <button class="btn btn-gen" onclick="genPass()">Gerar</button>
  </div>
  <div class="gen-opts">
    <label>Tamanho<input type="number" id="g-len" min="4" max="64" value="16"></label>
    <label><input type="checkbox" id="g-up" checked>A-Z</label>
    <label><input type="checkbox" id="g-num" checked>0-9</label>
    <label><input type="checkbox" id="g-sym" checked>S&iacute;mbolos</label>
  </div>
  <input type="hidden" id="f-id" value="-1">
  <div style="text-align:center;margin-top:8px">
    <button class="btn btn-ok" onclick="saveCred()">Salvar</button>
    <button class="btn btn-cancel" onclick="closeForm()">Cancelar</button>
  </div>
</div>
<div id="list"></div>
<button class="btn-add" onclick="openAdd()">+ Nova Credencial</button>
<script>
let selId=-1;
function msg(t,ok){const m=document.getElementById('msg');m.textContent=t;
  m.className=ok?'ok-msg':'err-msg';m.style.display='block';
  setTimeout(()=>m.style.display='none',2500)}
function load(){
  fetch('/api/list').then(r=>r.json()).then(d=>{
    let h='';d.forEach((c,i)=>{
      h+='<div class="card" onclick="showDetail('+i+')"><div class="name">'+
        esc(c.name)+'</div><div class="user">'+esc(c.user)+'</div></div>';
    });
    document.getElementById('list').innerHTML=h||'<p style="text-align:center;color:#666">Nenhuma credencial</p>';
  }).catch(()=>msg('Erro ao carregar',false));
}
function esc(s){const d=document.createElement('div');d.textContent=s;return d.innerHTML}
function showDetail(id){
  fetch('/api/get?id='+id).then(r=>r.json()).then(c=>{
    selId=id;
    document.getElementById('d-name').textContent=c.name;
    document.getElementById('d-user').textContent=c.user;
    document.getElementById('d-pass').textContent=c.pass;
    document.getElementById('detail').style.display='block';
    document.getElementById('form-area').style.display='none';
  }).catch(()=>msg('Erro',false));
}
function closeDetail(){document.getElementById('detail').style.display='none';selId=-1}
function editCred(){
  document.getElementById('f-name').value=document.getElementById('d-name').textContent;
  document.getElementById('f-user').value=document.getElementById('d-user').textContent;
  document.getElementById('f-pass').value=document.getElementById('d-pass').textContent;
  document.getElementById('f-id').value=selId;
  document.getElementById('form-area').style.display='block';
  document.getElementById('detail').style.display='none';
}
function delCred(){if(!confirm('Excluir?'))return;
  fetch('/api/del?id='+selId,{method:'POST'}).then(r=>r.json()).then(d=>{
    msg(d.ok?'Excluido':'Erro',d.ok);closeDetail();load();
  }).catch(()=>msg('Erro',false));
}
function openAdd(){
  document.getElementById('f-name').value='';
  document.getElementById('f-user').value='';
  document.getElementById('f-pass').value='';
  document.getElementById('f-id').value=-1;
  document.getElementById('form-area').style.display='block';
  document.getElementById('detail').style.display='none';
}
function rndInt(n){const lim=Math.floor(4294967296/n)*n,b=new Uint32Array(1);
  do{crypto.getRandomValues(b)}while(b[0]>=lim);return b[0]%n}
function genPass(){
  let len=parseInt(document.getElementById('g-len').value)||16;
  len=Math.max(4,Math.min(64,len));document.getElementById('g-len').value=len;
  const sets=['abcdefghijklmnopqrstuvwxyz'];
  if(document.getElementById('g-up').checked)sets.push('ABCDEFGHIJKLMNOPQRSTUVWXYZ');
  if(document.getElementById('g-num').checked)sets.push('0123456789');
  if(document.getElementById('g-sym').checked)sets.push('!@#$%&*()-_=+[]{};:,.<>?/');
  const all=sets.join(''),out=sets.map(s=>s[rndInt(s.length)]);
  while(out.length<len)out.push(all[rndInt(all.length)]);
  for(let i=out.length-1;i>0;i--){const j=rndInt(i+1);[out[i],out[j]]=[out[j],out[i]]}
  document.getElementById('f-pass').value=out.join('');
}
function closeForm(){document.getElementById('form-area').style.display='none'}
function saveCred(){
  const n=document.getElementById('f-name').value.trim();
  const u=document.getElementById('f-user').value.trim();
  const p=document.getElementById('f-pass').value.trim();
  const id=parseInt(document.getElementById('f-id').value);
  if(!n||!u||!p){msg('Preencha todos',false);return}
  const body='id='+id+'&name='+encodeURIComponent(n)+'&user='+
    encodeURIComponent(u)+'&pass='+encodeURIComponent(p);
  fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
    .then(r=>r.json()).then(d=>{msg(d.ok?'Salvo':'Erro',d.ok);closeForm();load();
  }).catch(()=>msg('Erro',false));
}
load();
</script>
</body>
</html>
)rawliteral";

// ---- API Handlers ----

void handleRoot() {
  webTouch();
  server.send_P(200, "text/html", PAGE_HTML);
}

void handleList() {
  webTouch();
  String json = "[";
  for (int i = 0; i < credCount; i++) {
    if (i) json += ",";
    json += "{\"name\":\"" + jsonEsc(creds[i].name) +
            "\",\"user\":\"" + jsonEsc(creds[i].user) + "\"}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleGet() {
  webTouch();
  if (!server.hasArg("id")) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= credCount) { server.send(404, "application/json", "{\"ok\":false}"); return; }
  String json = "{\"name\":\"" + jsonEsc(creds[id].name) +
                "\",\"user\":\"" + jsonEsc(creds[id].user) +
                "\",\"pass\":\"" + jsonEsc(creds[id].pass) + "\"}";
  server.send(200, "application/json", json);
}

void handleSave() {
  webTouch();
  if (!server.hasArg("name") || !server.hasArg("user") || !server.hasArg("pass")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  String name = server.arg("name");
  String user = server.arg("user");
  String pass = server.arg("pass");
  int id = server.hasArg("id") ? server.arg("id").toInt() : -1;

  bool ok;
  if (id >= 0 && id < credCount) {
    ok = updateCredential(id, name.c_str(), user.c_str(), pass.c_str());
  } else {
    ok = addCredential(name.c_str(), user.c_str(), pass.c_str());
  }
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleDel() {
  webTouch();
  if (!server.hasArg("id")) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  int id = server.arg("id").toInt();
  bool ok = deleteCredential(id);
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

// ---- Start / Stop / Tick ----

// Senha aleatoria sem caracteres ambiguos (0/O, 1/l/I)
void genWebPass() {
  static const char cs[] = "abcdefghjkmnpqrstuvwxyzACDEFGHJKLMNPQRTUVWXY346789";
  for (int i = 0; i < AP_PASS_LEN; i++) webPass[i] = cs[esp_random() % (sizeof(cs) - 1)];
  webPass[AP_PASS_LEN] = 0;
}

void startWeb() {
  genWebPass();
  // canal 1, SSID visivel, no maximo 1 cliente conectado
  WiFi.softAP(AP_SSID, webPass, 1, 0, 1);
  webLastRequest = millis();
  server.on("/",          handleRoot);
  server.on("/api/list",  handleList);
  server.on("/api/get",   handleGet);
  server.on("/api/save",  HTTP_POST, handleSave);
  server.on("/api/del",   HTTP_POST, handleDel);
  server.begin();
  webRunning = true;
}

void stopWeb() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  webRunning = false;
  memset(webPass, 0, sizeof(webPass));
}

void webTick() {
  if (webRunning) server.handleClient();
}

#endif
