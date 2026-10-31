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
<label for="language" data-i18n="language">Idioma</label>
<select id="language" onchange="saveLanguage()"></select>
<div id="detail" class="detail-area">
  <div class="field"><div class="label" data-i18n="name">Nome</div><div class="value" id="d-name"></div></div>
  <div class="field"><div class="label" data-i18n="type">Tipo</div><div class="value" id="d-category"></div></div>
  <div class="field"><div class="label" data-i18n="user">Usu&aacute;rio</div><div class="value" id="d-user"></div></div>
  <div class="field"><div class="label" data-i18n="password">Senha</div><div class="value" id="d-pass"></div></div>
  <div style="margin-top:12px;text-align:center">
    <button id="d-favorite" class="btn btn-cancel" onclick="favoriteCred()" data-i18n="favorite">Favoritar</button>
    <button class="btn btn-ok" onclick="editCred()" data-i18n="edit">Editar</button>
    <button class="btn btn-del" onclick="delCred()" data-i18n="delete">Excluir</button>
    <button class="btn btn-cancel" onclick="closeDetail()" data-i18n="back">Voltar</button>
  </div>
</div>
<div id="form-area">
  <input id="f-name" data-i18n-placeholder="name_hint" placeholder="Nome (ex: Gmail)" maxlength="32">
  <input id="f-user" data-i18n-placeholder="user_hint" placeholder="Usu&aacute;rio / Email" maxlength="64">
  <div class="pass-row">
    <input id="f-pass" data-i18n-placeholder="password" placeholder="Senha" maxlength="64">
    <button class="btn btn-gen" onclick="genPass()" data-i18n="generate">Gerar</button>
  </div>
  <div class="gen-opts">
    <label><span data-i18n="length">Tamanho</span><input type="number" id="g-len" min="4" max="64" value="16"></label>
    <label><input type="checkbox" id="g-up" checked>A-Z</label>
    <label><input type="checkbox" id="g-num" checked>0-9</label>
    <label><input type="checkbox" id="g-sym" checked><span data-i18n="symbols">S&iacute;mbolos</span></label>
  </div>
  <label for="f-category" data-i18n="type">Tipo</label>
  <select id="f-category">
    <option value="0" data-i18n="cat_none">Sem tipo</option>
    <option value="1" data-i18n="cat_social">Redes Sociais</option>
    <option value="2" data-i18n="cat_email">Email</option>
    <option value="3" data-i18n="cat_bank">Banco</option>
    <option value="4" data-i18n="cat_work">Trabalho</option>
  </select>
  <input type="hidden" id="f-id" value="-1">
  <div style="text-align:center;margin-top:8px">
    <button class="btn btn-ok" onclick="saveCred()" data-i18n="save">Salvar</button>
    <button class="btn btn-cancel" onclick="closeForm()" data-i18n="cancel">Cancelar</button>
  </div>
</div>
<label for="sort" data-i18n="sort_by">Ordenar por</label>
<select id="sort" onchange="saveSort()" style="width:100%;padding:10px;margin:8px 0;background:#222;color:#eee;border:1px solid #555;border-radius:6px">
  <option value="0" data-i18n="sort_alpha">Alfab&eacute;tica</option>
  <option value="1" data-i18n="sort_recent">Mais recente</option>
  <option value="2" data-i18n="sort_used">Mais usada</option>
  <option value="3" data-i18n="sort_type">Por tipo</option>
</select>
<div id="list"></div>
<button class="btn-add" onclick="openAdd()" data-i18n="new_credential">+ Nova Credencial</button>
<div class="backup">
  <input type="password" id="b-pw" data-i18n-placeholder="backup_hint" placeholder="Senha do backup (m&iacute;n. 8)" maxlength="64">
  <div style="text-align:center">
    <button class="btn btn-cancel" onclick="doExport()" data-i18n="export">Exportar</button>
    <button class="btn btn-cancel" onclick="document.getElementById('b-file').click()" data-i18n="import">Importar</button>
  </div>
  <input type="file" id="b-file" accept=".json,application/json" style="display:none" onchange="doImport(this)">
</div>
<script>
let ui={strings:{load_error:'Erro ao carregar',language_error:'Erro ao salvar idioma'}};
let currentLanguage=0;
function tr(key){return ui.strings[key]||key}
function applyLanguage(data){
  ui=data;currentLanguage=data.language;
  document.documentElement.lang=data.code;
  const select=document.getElementById('language');
  select.replaceChildren();
  data.languages.forEach((language,id)=>{
    const option=document.createElement('option');option.value=String(id);option.textContent=language.name;
    select.appendChild(option);
  });
  select.value=String(currentLanguage);
  document.querySelectorAll('[data-i18n]').forEach(e=>e.textContent=tr(e.dataset.i18n));
  document.querySelectorAll('[data-i18n-placeholder]').forEach(e=>e.placeholder=tr(e.dataset.i18nPlaceholder));
  if(selId>=0){
    document.getElementById('d-category').textContent=tr(typeKeys[selCategory]);
    document.getElementById('d-favorite').textContent=tr(selFavorite?'unfavorite_long':'favorite');
  }
}
function loadLanguage(){
  return fetch('/api/language').then(r=>{if(!r.ok)throw 0;return r.json()}).then(applyLanguage);
}
function saveLanguage(){
  const select=document.getElementById('language');select.disabled=true;
  return fetch('/api/language',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'language='+select.value}).then(r=>{if(!r.ok)throw 0;return r.json()}).then(data=>{
      applyLanguage(data);msg(tr('language_saved'),true);load();
    }).catch(()=>{select.value=String(currentLanguage);msg(tr('language_error'),false)})
    .finally(()=>{select.disabled=false});
}

let selId=-1,selFavorite=false,selCategory=0;
const typeKeys=['cat_none','cat_social','cat_email','cat_bank','cat_work'];
const typeColors=['#bdbdbd','#c080ff','#00ffff','#00ff00','#ffff00'];
function msg(t,ok){const m=document.getElementById('msg');m.textContent=t;
  m.className=ok?'ok-msg':'err-msg';m.style.display='block';
  setTimeout(()=>m.style.display='none',2500)}
function load(){
  fetch('/api/list').then(r=>r.json()).then(d=>{
    let h='',previous=-1;d.forEach(c=>{
      const type=Number.isInteger(c.category)&&c.category>=0&&c.category<typeKeys.length?c.category:0;
      if(document.getElementById('sort').value==='3'&&previous!==type){
        h+='<h2 class="group">'+tr(typeKeys[type])+'</h2>';previous=type;
      }
      h+='<div class="card" style="border-left:4px solid '+typeColors[type]+
        ';--type-color:'+typeColors[type]+'" onclick="showDetail('+c.id+')"><div class="name">'+
        (c.favorite?'&#9733; ':'')+esc(c.name)+'</div><div class="user">'+esc(c.user)+
        '</div><div class="type">'+tr(typeKeys[type])+'</div></div>';
    });
    document.getElementById('list').innerHTML=h||'<p style="text-align:center;color:#666">'+esc(tr('no_credentials'))+'</p>';
  }).catch(()=>msg(tr('load_error'),false));
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
    .catch(()=>{msg(tr('sort_error'),false);return loadSort()})
    .catch(()=>msg(tr('sort_load_error'),false)).finally(()=>{select.disabled=false});
}
function esc(s){const d=document.createElement('div');d.textContent=s;return d.innerHTML}
function showDetail(id){
  fetch('/api/get?id='+id).then(r=>r.json()).then(c=>{
    selId=id;selFavorite=c.favorite;selCategory=c.category;
    document.getElementById('d-category').textContent=tr(typeKeys[selCategory]);
    document.getElementById('d-category').style.color=typeColors[selCategory];
    document.getElementById('d-favorite').textContent=c.favorite?tr('unfavorite_long'):tr('favorite');
    document.getElementById('d-favorite').setAttribute('aria-pressed',String(c.favorite));
    document.getElementById('d-name').textContent=c.name;
    document.getElementById('d-user').textContent=c.user;
    document.getElementById('d-pass').textContent=c.pass;
    document.getElementById('detail').style.display='block';
    document.getElementById('form-area').style.display='none';
  }).catch(()=>msg(tr('error'),false));
}
function favoriteCred(){
  const id=selId;
  fetch('/api/favorite',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'id='+id+'&favorite='+(selFavorite?'0':'1')}).then(r=>r.json()).then(d=>{
      if(!d.ok)throw 0;
      load();if(selId===id)showDetail(id);
    }).catch(()=>msg(tr('favorite_error'),false));
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
function delCred(){if(!confirm(tr('delete_confirm')))return;
  fetch('/api/del?id='+selId,{method:'POST'}).then(r=>r.json()).then(d=>{
    msg(d.ok?tr('deleted'):tr('error'),d.ok);closeDetail();load();
  }).catch(()=>msg(tr('error'),false));
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
  if(!n||!u||!p){msg(tr('fill_all'),false);return}
  const body='id='+id+'&name='+encodeURIComponent(n)+'&user='+
    encodeURIComponent(u)+'&pass='+encodeURIComponent(p)+'&category='+document.getElementById('f-category').value;
  fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
    .then(r=>r.json()).then(d=>{if(!d.ok)throw 0;msg(tr('saved'),true);closeForm();load();
  }).catch(()=>msg(tr('error'),false));
}
function backupPw(){const p=document.getElementById('b-pw').value;
  if(p.length<8){msg(tr('backup_short'),false);return null}return p}
function doExport(){const p=backupPw();if(!p)return;msg(tr('generating'),true);
  fetch('/api/export',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'pw='+encodeURIComponent(p)}).then(r=>{if(!r.ok)throw 0;return r.blob()}).then(b=>{
    const a=document.createElement('a');a.href=URL.createObjectURL(b);
    a.download='passvault-backup-'+new Date().toISOString().slice(0,10)+'.json';
    document.body.appendChild(a);a.click();a.remove();URL.revokeObjectURL(a.href);
    msg(tr('export_done'),true)}).catch(()=>msg(tr('export_error'),false));
}
function doImport(inp){const f=inp.files[0];inp.value='';if(!f)return;
  const p=backupPw();if(!p)return;
  f.text().then(t=>{const j=JSON.parse(t);
    if(j.app!=='PassVault'||(j.v!==1&&j.v!==2&&j.v!==3)||!Array.isArray(j.creds))throw 0;
    let body='pw='+encodeURIComponent(p)+'&v='+j.v+'&iter='+j.iter+'&salt='+j.salt+'&ver='+j.ver+
      '&mac='+j.mac+'&n='+j.creds.length;
    j.creds.forEach((c,i)=>body+='&c'+i+'='+encodeURIComponent(c));
    msg(tr('importing'),true);
    return fetch('/api/import',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
  }).then(r=>r.json()).then(d=>{
    if(d.ok){msg(tr('import_result').replace('{added}',d.added).replace('{skipped}',d.skipped),true);load()}
    else msg(d.err==='pw'?tr('backup_wrong'):tr('invalid_file'),false);
  }).catch(()=>msg(tr('invalid_file'),false));
}
loadLanguage().then(()=>loadSort()).then(()=>load()).catch(()=>msg(tr('load_error'),false));
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

void handleLanguageGet() {
  webTouch();
  server.send(200, "application/json", LANGUAGE_WEB_JSON[activeLanguage]);
}

void handleLanguageSave() {
  webTouch();
  String value = server.arg("language");
  if (value.length() != 1 || value[0] < '0' || value[0] >= '0' + LANGUAGE_COUNT) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  if (!setLanguage(value[0] - '0')) {
    server.send(500, "application/json", "{\"ok\":false}");
    return;
  }
  server.send(200, "application/json", LANGUAGE_WEB_JSON[activeLanguage]);
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
  server.on("/api/language", HTTP_GET, handleLanguageGet);
  server.on("/api/language", HTTP_POST, handleLanguageSave);
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
