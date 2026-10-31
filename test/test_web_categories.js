// Exercise the embedded SPA with simulated DOM and API responses.
// Run: node test/test_web_categories.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const html = fs.readFileSync(path.join(__dirname, '../include/vault_web.h'), 'utf8');
const script = html.split('<script>')[1].split('</script>')[0];
const elements = new Map();
function element(id) {
  if (!elements.has(id)) elements.set(id, {
    value: '', textContent: '', innerHTML: '', style: {}, children: [],
    setAttribute() {}, replaceChildren() { this.children = []; }, appendChild(e) { this.children.push(e); }
  });
  return elements.get(id);
}
const rows = [1, 2, 3, 4, 0].map((category, id) => ({
  id, category, name: 'Conta <' + id + '>', user: 'u&' + id, pass: 'p|&+', favorite: id === 0
}));
const requests = [];
const catalog = JSON.parse(fs.readFileSync(path.join(__dirname, '../locales/translations.json'), 'utf8'));
let language = 0, languageFail = false;
const marked = [];
for (const match of html.split('<script>')[0].matchAll(/<[^>]+data-i18n(?:-placeholder)?="[^"]+"[^>]*>/g)) {
  const tag = match[0], id = tag.match(/\bid="([^"]+)"/);
  const e = element(id ? id[1] : 'static-' + marked.length);
  e.dataset = {};
  const placeholder = tag.match(/data-i18n-placeholder="([^"]+)"/);
  const label = tag.match(/data-i18n="([^"]+)"/);
  if (placeholder) e.dataset.i18nPlaceholder = placeholder[1];
  if (label) e.dataset.i18n = label[1];
  marked.push(e);
}
function languageData() {
  return {language, code: catalog.languages[language].code, languages: catalog.languages,
    strings: Object.fromEntries(Object.entries(catalog.strings).map(([key, texts]) => [key, texts[language]]))};
}
let saveOk = true;
const context = vm.createContext({
  document: {
    documentElement: {lang: 'pt-BR'},
    querySelectorAll(selector) {
      const key = selector.includes('placeholder') ? 'i18nPlaceholder' : 'i18n';
      return marked.filter(e => Object.hasOwn(e.dataset, key));
    },
    getElementById: element,
    createElement() {
      return {
        textContent: '',
        get innerHTML() {
          return this.textContent.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;');
        }
      };
    }
  },
  setTimeout() {},
  fetch: async (url, options) => {
    requests.push({url, options});
    let data;
    if (url === '/api/language') {
      if (options) {
        if (languageFail) return {ok: false};
        language = Number(new URLSearchParams(options.body).get('language'));
      }
      data = languageData();
    }
    else if (url === '/api/sort') data = {mode: 3};
    else if (url === '/api/list') data = rows;
    else if (url.startsWith('/api/get?id=')) data = rows[Number(url.split('=')[1])];
    else if (url === '/api/save') data = {ok: saveOk};
    else if (url === '/api/import') data = {ok: true, added: 1, skipped: 0};
    else throw new Error('Unexpected route: ' + url);
    return {ok: true, json: async () => data};
  }
});
const settle = () => new Promise(resolve => setImmediate(resolve));
(async () => {
  vm.runInContext(script, context);
  await settle();
  const grouped = element('list').innerHTML;
  for (const label of ['Redes Sociais', 'Email', 'Banco', 'Trabalho', 'Sem tipo'])
    assert(grouped.includes('<h2 class="group">' + label + '</h2>'));
  assert.equal((grouped.match(/border-left:4px solid/g) || []).length, 5);
  assert(grouped.includes('Conta &lt;0&gt;') && grouped.includes('u&amp;0'));
  assert(grouped.includes('showDetail(3)'));
  element('sort').value = '0';
  context.load();await settle();
  assert(!element('list').innerHTML.includes('<h2'));

  context.showDetail(3);await settle();
  assert.equal(element('d-category').textContent, 'Trabalho');
  context.editCred();
  assert.equal(Number(element('f-category').value), 4);
  element('f-category').value = '3';
  context.saveCred();await settle();
  const save = requests.find(r => r.url === '/api/save');
  const body = new URLSearchParams(save.options.body);
  assert.equal(body.get('id'), '3');
  assert.equal(body.get('category'), '3');
  assert.equal(body.get('pass'), 'p|&+');

  context.openAdd();
  assert.equal(element('f-category').value, '0');
  element('f-name').value = 'New';element('f-user').value = 'user';element('f-pass').value = 'pass';
  saveOk = false;context.saveCred();await settle();
  assert.equal(element('form-area').style.display, 'block');
  assert.equal(element('f-name').value, 'New');

  element('b-pw').value = 'backup-password';
  for (const version of [1, 2, 3]) {
    const input = {value: 'backup.json', files: [{text: async () => JSON.stringify({
      app: 'PassVault', v: version, creds: ['ciphertext'], iter: 20000,
      salt: 'salt', ver: 'verifier', mac: 'mac'
    })}]};
    context.doImport(input);await settle();
    const imported = requests.filter(r => r.url === '/api/import').at(-1);
    assert.equal(new URLSearchParams(imported.options.body).get('v'), String(version));
  }
  // Switch every language without losing in-progress credentials or changing IDs.
  context.openAdd();element('f-name').value = 'Unchanged name';
  element('f-user').value = 'unchanged user';element('f-pass').value = 'secret|&+';
  element('f-category').value = '3';
  for (let id = 0; id < catalog.languages.length; id++) {
    element('language').value = String(id);
    await context.saveLanguage();await settle();
    assert.equal(context.document.documentElement.lang, catalog.languages[id].code);
    assert.equal(element('language').children.length, 8);
    for (const e of marked) {
      if (e.dataset.i18n) {
        // Favorite is a dynamic action if a detail remains selected.
        if (e === element('d-favorite')) continue;
        assert.equal(e.textContent, catalog.strings[e.dataset.i18n][id]);
      }
      if (e.dataset.i18nPlaceholder)
        assert.equal(e.placeholder, catalog.strings[e.dataset.i18nPlaceholder][id]);
    }
    assert(element('list').innerHTML.includes(catalog.strings.cat_work[id]));
    assert.equal(element('f-name').value, 'Unchanged name');
    assert.equal(element('f-user').value, 'unchanged user');
    assert.equal(element('f-pass').value, 'secret|&+');
    assert.equal(element('f-category').value, '3');
  }
  languageFail = true;element('language').value = '0';
  await context.saveLanguage();
  assert.equal(context.document.documentElement.lang, 'ru');
  assert.equal(element('language').value, '7');
  assert.equal(element('language').disabled, false);
  assert.equal(element('msg').textContent, catalog.strings.language_error[7]);
  console.log('PASS: all eight web languages, live labels/placeholders, preserved form values, failure rollback; web grouping, colors, escaping, stable IDs, type editing, save failures and backup versions');
})().catch(error => { console.error(error);process.exitCode = 1; });
