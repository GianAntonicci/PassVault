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
extern bool addCredential(const char*, const char*, const char*, bool, uint8_t);
extern bool updateCredential(int, const char*, const char*, const char*, uint8_t);
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
.type{font-size:.8em;margin-top:5px;color:var(--type-color)}
.group{margin:16px 0 8px;font-size:1em;color:#eee}
.card .user{color:#aaa;font-size:.85em;margin-top:2px}
#form-area{display:none;background:#1a1a2e;border:1px solid #333;
           border-radius:10px;padding:16px;margin-bottom:14px}
select{width:100%;padding:10px;margin:6px 0;background:#222;color:#eee;border:1px solid #444;border-radius:6px;font-size:1em}
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
.backup{margin-top:18px;padding-top:12px;border-top:1px solid #333}
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
  <div class="field"><div class="label">Tipo</div><div class="value" id="d-category"></div></div>
  <div class="field"><div class="label">Usu&aacute;rio</div><div class="value" id="d-user"></div></div>
  <div class="field"><div class="label">Senha</div><div class="value" id="d-pass"></div></div>
  <div style="margin-top:12px;text-align:center">
    <button id="d-favorite" class="btn btn-cancel" onclick="favoriteCred()">Favoritar</button>
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
  <label for="f-category">Tipo</label>
  <select id="f-category">
    <option value="0">Sem tipo</option>
    <option value="1">Redes Sociais</option>
    <option value="2">Email</option>
    <option value="3">Banco</option>
    <option value="4">Trabalho</option>
  </select>
  <input type="hidden" id="f-id" value="-1">
  <div style="text-align:center;margin-top:8px">
    <button class="btn btn-ok" onclick="saveCred()">Salvar</button>
    <button class="btn btn-cancel" onclick="closeForm()">Cancelar</button>
  </div>
</div>
<label for="sort">Ordenar por</label>
<select id="sort" onchange="saveSort()" style="width:100%;padding:10px;margin:8px 0;background:#222;color:#eee;border:1px solid #555;border-radius:6px">
  <option value="0">Alfab&eacute;tica</option>
  <option value="1">Mais recente</option>
  <option value="2">Mais usada</option>
  <option value="3">Por tipo</option>
</select>
<div id="list"></div>
<button class="btn-add" onclick="openAdd()">+ Nova Credencial</button>
<div class="backup">
  <input type="password" id="b-pw" placeholder="Senha do backup (m&iacute;n. 8)" maxlength="64">
  <div style="text-align:center">
    <button class="btn btn-cancel" onclick="doExport()">Exportar</button>
    <button class="btn btn-cancel" onclick="document.getElementById('b-file').click()">Importar</button>
  </div>
  <input type="file" id="b-file" accept=".json,application/json" style="display:none" onchange="doImport(this)">
</div>
<script>
let selId=-1,selFavorite=false,selCategory=0;
const types=['Sem tipo','Redes Sociais','Email','Banco','Trabalho'];
const typeColors=['#bdbdbd','#c080ff','#00ffff','#00ff00','#ffff00'];
function msg(t,ok){const m=document.getElementById('msg');m.textContent=t;
  m.className=ok?'ok-msg':'err-msg';m.style.display='block';
  setTimeout(()=>m.style.display='none',2500)}
function load(){
  fetch('/api/list').then(r=>r.json()).then(d=>{
    let h='',previous=-1;d.forEach(c=>{
      const type=Number.isInteger(c.category)&&c.category>=0&&c.category<types.length?c.category:0;
      if(document.getElementById('sort').value==='3'&&previous!==type){
        h+='<h2 class="group">'+types[type]+'</h2>';previous=type;
      }
      h+='<div class="card" style="border-left:4px solid '+typeColors[type]+
        ';--type-color:'+typeColors[type]+'" onclick="showDetail('+c.id+')"><div class="name">'+
        (c.favorite?'&#9733; ':'')+esc(c.name)+'</div><div class="user">'+esc(c.user)+
        '</div><div class="type">'+types[type]+'</div></div>';
    });
    document.getElementById('list').innerHTML=h||'<p style="text-align:center;color:#666">Nenhuma credencial</p>';
  }).catch(()=>msg('Erro ao carregar',false));
}
function loadSort(){
  return fetch('/api/sort').then(r=>{if(!r.ok)throw 0;return r.json()}).then(d=>{
    document.getElementById('sort').value=String(d.mode);
  });
}
function saveSort(){
  const select=document.getElementById('sort');select.disabled=true;
  fetch('/api/sort',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'mode='+select.value}).then(r=>{if(!r.ok)throw 0;return r.json()}).then(()=>load())
    .catch(()=>{msg('Erro ao salvar ordem',false);return loadSort()})
    .catch(()=>msg('Erro ao carregar ordem',false)).finally(()=>{select.disabled=false});
}
function esc(s){const d=document.createElement('div');d.textContent=s;return d.innerHTML}
function showDetail(id){
  fetch('/api/get?id='+id).then(r=>r.json()).then(c=>{
    selId=id;selFavorite=c.favorite;selCategory=c.category;
    document.getElementById('d-category').textContent=types[selCategory];
    document.getElementById('d-category').style.color=typeColors[selCategory];
    document.getElementById('d-favorite').textContent=c.favorite?'Desmarcar favorita':'Favoritar';
    document.getElementById('d-favorite').setAttribute('aria-pressed',String(c.favorite));
    document.getElementById('d-name').textContent=c.name;
    document.getElementById('d-user').textContent=c.user;
    document.getElementById('d-pass').textContent=c.pass;
    document.getElementById('detail').style.display='block';
    document.getElementById('form-area').style.display='none';
  }).catch(()=>msg('Erro',false));
}
function favoriteCred(){
  const id=selId;
  fetch('/api/favorite',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'id='+id+'&favorite='+(selFavorite?'0':'1')}).then(r=>r.json()).then(d=>{
      if(!d.ok)throw 0;
      load();if(selId===id)showDetail(id);
    }).catch(()=>msg('Erro ao favoritar',false));
}
function closeDetail(){document.getElementById('detail').style.display='none';selId=-1}
function editCred(){
  document.getElementById('f-name').value=document.getElementById('d-name').textContent;
  document.getElementById('f-user').value=document.getElementById('d-user').textContent;
  document.getElementById('f-pass').value=document.getElementById('d-pass').textContent;
  document.getElementById('f-id').value=selId;
  document.getElementById('f-category').value=selCategory;
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
  document.getElementById('f-category').value='0';
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
    encodeURIComponent(u)+'&pass='+encodeURIComponent(p)+'&category='+document.getElementById('f-category').value;
  fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
    .then(r=>r.json()).then(d=>{if(!d.ok)throw 0;msg('Salvo',true);closeForm();load();
  }).catch(()=>msg('Erro',false));
}
function backupPw(){const p=document.getElementById('b-pw').value;
  if(p.length<8){msg('Senha do backup: m\u00ednimo 8 caracteres',false);return null}return p}
function doExport(){const p=backupPw();if(!p)return;msg('Gerando...',true);
  fetch('/api/export',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'pw='+encodeURIComponent(p)}).then(r=>{if(!r.ok)throw 0;return r.blob()}).then(b=>{
    const a=document.createElement('a');a.href=URL.createObjectURL(b);
    a.download='passvault-backup-'+new Date().toISOString().slice(0,10)+'.json';
    document.body.appendChild(a);a.click();a.remove();URL.revokeObjectURL(a.href);
    msg('Backup exportado',true)}).catch(()=>msg('Erro ao exportar',false));
}
function doImport(inp){const f=inp.files[0];inp.value='';if(!f)return;
  const p=backupPw();if(!p)return;
  f.text().then(t=>{const j=JSON.parse(t);
    if(j.app!=='PassVault'||(j.v!==1&&j.v!==2&&j.v!==3)||!Array.isArray(j.creds))throw 0;
    let body='pw='+encodeURIComponent(p)+'&v='+j.v+'&iter='+j.iter+'&salt='+j.salt+'&ver='+j.ver+
      '&mac='+j.mac+'&n='+j.creds.length;
    j.creds.forEach((c,i)=>body+='&c'+i+'='+encodeURIComponent(c));
    msg('Importando...',true);
    return fetch('/api/import',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
  }).then(r=>r.json()).then(d=>{
    if(d.ok){msg(d.added+' importada(s), '+d.skipped+' ignorada(s)',true);load()}
    else msg(d.err==='pw'?'Senha do backup incorreta':'Arquivo inv\u00e1lido',false);
  }).catch(()=>msg('Arquivo inv\u00e1lido',false));
}
loadSort().then(()=>load()).catch(()=>msg('Erro ao carregar ordem',false));
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
  for (int position = 0; position < credCount; position++) {
    int i = credentialDisplayIndex(position);
    if (position) json += ",";
    json += "{\"id\":" + String(i) + ",\"favorite\":" +
            (creds[i].favorite ? "true" : "false") +
            ",\"category\":" + String(creds[i].category) +
            ",\"name\":\"" + jsonEsc(creds[i].name) +
            "\",\"user\":\"" + jsonEsc(creds[i].user) + "\"}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleSortGet() {
  webTouch();
  server.send(200, "application/json", "{\"mode\":" + String(credentialSort) + "}");
}

void handleSortSave() {
  webTouch();
  String mode = server.arg("mode");
  if (mode.length() != 1 || mode[0] < '0' || mode[0] >= '0' + SORT_COUNT) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  bool ok = setCredentialSort(mode[0] - '0');
  server.send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleGet() {
  webTouch();
  if (!server.hasArg("id")) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= credCount) { server.send(404, "application/json", "{\"ok\":false}"); return; }
  String json = "{\"name\":\"" + jsonEsc(creds[id].name) +
                "\",\"user\":\"" + jsonEsc(creds[id].user) +
                "\",\"pass\":\"" + jsonEsc(creds[id].pass) + "\",\"favorite\":" +
                (creds[id].favorite ? "true" : "false") +
                ",\"category\":" + String(creds[id].category) + "}";
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

  uint8_t category = CATEGORY_COUNT;
  if (server.hasArg("category")) {
    String value = server.arg("category");
    if (value.length() != 1 || value[0] < '0' || value[0] >= '0' + CATEGORY_COUNT) {
      server.send(400, "application/json", "{\"ok\":false}");
      return;
    }
    category = value[0] - '0';
  }
  bool ok;
  if (id >= 0 && id < credCount) {
    ok = updateCredential(id, name.c_str(), user.c_str(), pass.c_str(), category);
  } else {
    ok = addCredential(name.c_str(), user.c_str(), pass.c_str(), false,
                       category == CATEGORY_COUNT ? uint8_t(CATEGORY_NONE) : category);
  }
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleFavorite() {
  webTouch();
  String idArg = server.arg("id");
  String value = server.arg("favorite");
  bool valid = idArg.length() > 0 && (value == "0" || value == "1");
  for (size_t i = 0; i < idArg.length(); i++)
    if (!isDigit(idArg[i])) valid = false;
  // IDs validos tem no maximo dois digitos (MAX_CREDS = 30).
  if (!valid || idArg.length() > String(MAX_CREDS - 1).length()) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  int id = idArg.toInt();
  if (id < 0 || id >= credCount) {
    server.send(404, "application/json", "{\"ok\":false}");
    return;
  }
  bool ok = setCredentialFavorite(id, value == "1");
  server.send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleDel() {
  webTouch();
  if (!server.hasArg("id")) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  int id = server.arg("id").toInt();
  bool ok = deleteCredential(id);
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleExport() {
  webTouch();
  String pw = server.arg("pw");
  if (pw.length() < 8) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  String json = exportBackup(pw.c_str());
  mbedtls_platform_zeroize((void*)pw.c_str(), pw.length());
  server.sendHeader("Content-Disposition", "attachment; filename=\"passvault-backup.json\"");
  server.send(200, "application/json", json);
}

void handleImport() {
  webTouch();
  String pw = server.arg("pw");
  int n = server.arg("n").toInt();
  if (pw.length() < 8 || n < 0 || n > MAX_CREDS) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"file\"}");
    return;
  }
  static String entries[MAX_CREDS];
  for (int i = 0; i < n; i++) entries[i] = server.arg("c" + String(i));
  int skipped = 0;
  int r = importBackup(pw.c_str(), server.arg("iter").toInt(), server.arg("salt"),
                       server.arg("ver"), server.arg("mac"), entries, n, &skipped,
                       server.hasArg("v") ? server.arg("v").toInt() : 1);
  for (int i = 0; i < n; i++) entries[i] = "";
  mbedtls_platform_zeroize((void*)pw.c_str(), pw.length());
  if (r < 0) {
    server.send(200, "application/json", r == -1 ? "{\"ok\":false,\"err\":\"pw\"}"
                                                 : "{\"ok\":false,\"err\":\"file\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true,\"added\":" + String(r) +
                                       ",\"skipped\":" + String(skipped) + "}");
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
  server.on("/api/sort", HTTP_GET, handleSortGet);
  server.on("/api/sort", HTTP_POST, handleSortSave);
  server.on("/api/get",   handleGet);
  server.on("/api/save",  HTTP_POST, handleSave);
  server.on("/api/favorite", HTTP_POST, handleFavorite);
  server.on("/api/del",   HTTP_POST, handleDel);
  server.on("/api/export", HTTP_POST, handleExport);
  server.on("/api/import", HTTP_POST, handleImport);
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
