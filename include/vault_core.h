#ifndef VAULT_CORE_H
#define VAULT_CORE_H

#include <Preferences.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>
#include "config.h"
#include "localization.h"

// ---- Credential struct ----
enum CredentialCategory : uint8_t {
  CATEGORY_NONE, CATEGORY_SOCIAL, CATEGORY_EMAIL, CATEGORY_BANK, CATEGORY_WORK, CATEGORY_COUNT
};
const char* const CATEGORY_LABELS[] = { "Sem tipo", "Redes Sociais", "Email", "Banco", "Trabalho" };

struct Credential {
  char name[33];
  char user[65];
  char pass[65];
  bool favorite;
  uint32_t useCount;
  uint8_t category;
};

// ---- Globals ----
Preferences prefs;
Credential  creds[MAX_CREDS];
int         credCount = 0;
uint8_t     aesKey[32];
uint8_t     salt[16];
unsigned long autoLockMs = AUTOLOCK_MS;
uint8_t displayBrightness = BRIGHTNESS_DEFAULT;
bool storageReady = false;

enum CredentialSort : uint8_t { SORT_ALPHA, SORT_RECENT, SORT_USED, SORT_CATEGORY, SORT_COUNT };
uint8_t credentialSort = SORT_ALPHA;
const char* const SORT_LABELS[] = { "Alfabetica", "Mais recente", "Mais usada", "Por tipo" };

bool setCredentialSort(uint8_t mode) {
  if (mode >= SORT_COUNT) return false;
  if (mode == credentialSort) return true;
  if (prefs.putUChar("sort", mode) != sizeof(uint8_t)) return false;
  credentialSort = mode;
  return true;
}

// Um uso por envio (Tudo/Login/Senha), sem contar visualizacoes.
bool recordCredentialUse(int idx) {
  if (idx < 0 || idx >= credCount) return false;
  if (creds[idx].useCount == UINT32_MAX) return true;
  char key[4]; snprintf(key, sizeof(key), "u%d", idx);
  uint32_t count = creds[idx].useCount + 1;
  if (prefs.putUInt(key, count) != sizeof(uint32_t)) return false;
  creds[idx].useCount = count;
  return true;
}

bool validAutoLock(unsigned long ms) {
  for (size_t i = 0; i < AUTOLOCK_OPTION_COUNT; i++)
    if (ms == AUTOLOCK_OPTIONS_MS[i]) return true;
  return false;
}

bool setAutoLock(unsigned long ms) {
  if (!validAutoLock(ms) || prefs.putULong("autolock", ms) != sizeof(uint32_t)) return false;
  autoLockMs = ms;
  return true;
}

bool setTheme(uint8_t theme) {
  if (theme >= THEME_COUNT) return false;
  if (theme == activeTheme) return true;
  if (prefs.putUChar("theme", theme) != sizeof(uint8_t)) return false;
  activeTheme = theme;
  return true;
}

bool setBrightness(uint8_t percent) {
  if (percent < BRIGHTNESS_MIN || percent > BRIGHTNESS_MAX) return false;
  if (percent == displayBrightness) return true;
  if (prefs.putUChar("brightness", percent) != sizeof(uint8_t)) return false;
  displayBrightness = percent;
  return true;
}

// O idioma e compartilhado pela tela e web; nao integra backups de credenciais.
bool setLanguage(uint8_t language) {
  if (language >= LANGUAGE_COUNT) return false;
  if (language == activeLanguage) return true;
  if (prefs.putUChar("language", language) != sizeof(uint8_t)) return false;
  activeLanguage = language;
  return true;
}

// ---- Crypto helpers ----

// v1 (legado): chave = SHA-256(salt || PIN). Usado so para migrar.
void deriveKeyV1(const char* pin) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, salt, 16);
  mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));
  mbedtls_sha256_finish(&ctx, aesKey);
  mbedtls_sha256_free(&ctx);
}

// v1 (legado): verificador = SHA-256(SHA-256(PIN)). Usado so para migrar.
String hashPinV1(const char* pin) {
  uint8_t h1[32], h2[32];
  mbedtls_sha256_context ctx;

  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));
  mbedtls_sha256_finish(&ctx, h1);
  mbedtls_sha256_free(&ctx);

  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, h1, 32);
  mbedtls_sha256_finish(&ctx, h2);
  mbedtls_sha256_free(&ctx);

  String hex = "";
  for (int i = 0; i < 32; i++) {
    char tmp[3];
    sprintf(tmp, "%02x", h2[i]);
    hex += tmp;
  }
  return hex;
}

String encryptData(const char* plain, const uint8_t* key = aesKey) {
  size_t pLen = strlen(plain);
  uint8_t pad = 16 - (pLen % 16);
  size_t  bLen = pLen + pad;
  uint8_t* buf = (uint8_t*)malloc(bLen);
  memcpy(buf, plain, pLen);
  memset(buf + pLen, pad, pad);

  uint8_t iv[16];
  esp_fill_random(iv, 16);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 256);
  uint8_t ivCopy[16];
  memcpy(ivCopy, iv, 16);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, bLen, ivCopy, buf, buf);
  mbedtls_aes_free(&aes);

  String hex = "";
  for (int i = 0; i < 16; i++) {
    char tmp[3]; sprintf(tmp, "%02x", iv[i]); hex += tmp;
  }
  for (size_t i = 0; i < bLen; i++) {
    char tmp[3]; sprintf(tmp, "%02x", buf[i]); hex += tmp;
  }
  mbedtls_platform_zeroize(buf, bLen);
  free(buf);
  return hex;
}

String decryptData(const String& hexData, const uint8_t* key = aesKey) {
  if (hexData.length() < 34) return "";
  size_t totalBytes = hexData.length() / 2;
  uint8_t* raw = (uint8_t*)malloc(totalBytes);
  for (size_t i = 0; i < totalBytes; i++) {
    char h[3] = { hexData[i*2], hexData[i*2+1], 0 };
    raw[i] = strtoul(h, NULL, 16);
  }
  uint8_t iv[16];
  memcpy(iv, raw, 16);
  size_t cLen = totalBytes - 16;
  uint8_t* cipher = raw + 16;

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, key, 256);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cLen, iv, cipher, cipher);
  mbedtls_aes_free(&aes);

  // PKCS#7: 1..16 bytes, todos iguais — senao a chave/dado esta errado
  uint8_t pad = cLen ? cipher[cLen - 1] : 0;
  bool padOk = cLen > 0 && (cLen % 16) == 0 && pad >= 1 && pad <= 16;
  for (size_t i = 0; padOk && i < pad; i++) padOk = cipher[cLen - 1 - i] == pad;
  if (!padOk) {
    mbedtls_platform_zeroize(raw, totalBytes);
    free(raw);
    return "";
  }
  size_t plainLen = cLen - pad;
  char* plain = (char*)malloc(plainLen + 1);
  memcpy(plain, cipher, plainLen);
  plain[plainLen] = 0;
  String result(plain);
  mbedtls_platform_zeroize(plain, plainLen);
  free(plain);
  mbedtls_platform_zeroize(raw, totalBytes);
  free(raw);
  return result;
}

// ---- KDF v2 ----
// master = PBKDF2-HMAC-SHA256(PIN, salt, KDF_ITERS)
// chave AES   = SHA-256(master || "key")
// verificador = SHA-256(master || "ver")  (o PIN nunca e salvo)

void sha256Tagged(const uint8_t* in, size_t len, const char* tag, uint8_t* out) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, in, len);
  mbedtls_sha256_update(&ctx, (const uint8_t*)tag, strlen(tag));
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
}

// Permite preparar um novo PIN sem substituir a chave da sessao atual.
bool derivePinKey(const char* pin, const uint8_t* pinSalt, uint8_t* key, uint8_t* verifier) {
  uint8_t master[32];
  int result = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
                                (const uint8_t*)pin, strlen(pin),
                                pinSalt, 16, KDF_ITERS,
                                sizeof(master), master);
  if (result == 0) {
    sha256Tagged(master, 32, "key", key);
    sha256Tagged(master, 32, "ver", verifier);
  }
  mbedtls_platform_zeroize(master, sizeof(master));
  return result == 0;
}

// Deriva chave (aesKey) e verificador a partir do PIN e do salt atual
void deriveKeyV2(const char* pin, uint8_t* verifier) {
  derivePinKey(pin, salt, aesKey, verifier);
}

// Comparacao em tempo constante
bool ctEqual(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++) d |= a[i] ^ b[i];
  return d == 0;
}

// ---- Storage ----
// Chaves NVS: ver (formato), salt2 + pv2 (salt e verificador v2),
//             plen (tamanho do PIN), fails (tentativas erradas),
//             cnt, c0..cN (credenciais cifradas), m0..mN (temporarias da migracao)
//             f0..fN (favoritas; ausente = false), u0..uN (usos; ausente = 0),
//             t0..tN (tipo; ausente/invalido = Sem tipo),
//             sort (ordenacao; ausente = alfabetica),
//             salt + pin (formato v1, so em vaults antigos)

#define VAULT_FORMAT  2

void loadCredentials();
void saveSingleCred(int idx);

// Conclui uma migracao interrompida (m_i ja cifradas com a chave v2)
void finishMigration() {
  if (!prefs.isKey("pv2")) return;
  int n = prefs.getInt("cnt", 0);
  for (int i = 0; i < n; i++) {
    char mk[5], ck[5];
    sprintf(mk, "m%d", i);
    sprintf(ck, "c%d", i);
    if (!prefs.isKey(mk)) continue;
    prefs.putString(ck, prefs.getString(mk, ""));
    prefs.remove(mk);
  }
  prefs.remove("pin");
  prefs.remove("salt");
}

// Transacao de troca de PIN: pvnext guarda uma copia completa recifrada.
// pinchange e o ponto de commit. Depois dele, a copia e reaplicada ate terminar,
// inclusive se faltar energia durante a substituicao de c_i/salt2/pv2/plen.
bool finishPinChange() {
  if (!prefs.getBool("pinchange", false)) return true;
  Preferences next;
  if (!next.begin("pvnext", true)) return false;
  uint8_t newSalt[16], verifier[32];
  int count = next.getInt("cnt", -1);
  uint8_t length = next.getUChar("plen", 0);
  bool ok = count >= 0 && count <= MAX_CREDS && length > 0 && length <= PIN_LEN_MAX &&
            next.getBytes("salt2", newSalt, sizeof(newSalt)) == sizeof(newSalt) &&
            next.getBytes("pv2", verifier, sizeof(verifier)) == sizeof(verifier);
  // Confere todos os registros antes de comecar a substituir os dados ativos.
  for (int i = 0; ok && i < count; i++) {
    char key[5]; snprintf(key, sizeof(key), "c%d", i);
    String enc = next.getString(key, "");
    ok = enc.length() >= 64 && enc.length() % 32 == 0;
  }
  for (int i = 0; ok && i < count; i++) {
    char key[5]; snprintf(key, sizeof(key), "c%d", i);
    String enc = next.getString(key, "");
    ok = prefs.putString(key, enc) == enc.length();
  }
  if (ok) ok = prefs.putBytes("salt2", newSalt, sizeof(newSalt)) == sizeof(newSalt);
  if (ok) ok = prefs.putBytes("pv2", verifier, sizeof(verifier)) == sizeof(verifier);
  if (ok) ok = prefs.putUChar("plen", length) == sizeof(uint8_t);
  if (ok) ok = prefs.putUChar("ver", VAULT_FORMAT) == sizeof(uint8_t);
  if (ok) ok = prefs.remove("pinchange");
  next.end();
  mbedtls_platform_zeroize(newSalt, sizeof(newSalt));
  mbedtls_platform_zeroize(verifier, sizeof(verifier));
  // A copia temporaria so e descartada depois da remocao do marcador.
  if (ok && next.begin("pvnext", false)) { next.clear(); next.end(); }
  return ok;
}

enum PinChangeResult { PIN_CHANGE_FAILED, PIN_CHANGE_OK, PIN_CHANGE_PENDING };

// Chamar apenas depois de validar o PIN atual. Nao altera favoritas/configuracoes.
PinChangeResult changePin(const char* pin) {
  size_t length = strlen(pin);
  if (length != PIN_LEN || prefs.getBool("pinchange", false)) return PIN_CHANGE_FAILED;
  for (size_t i = 0; i < length; i++)
    if (pin[i] < '0' || pin[i] > '9') return PIN_CHANGE_FAILED;
  if (credCount < 0 || credCount > MAX_CREDS || credCount != prefs.getInt("cnt", 0))
    return PIN_CHANGE_FAILED;

  Preferences next;
  if (!next.begin("pvnext", false)) return PIN_CHANGE_FAILED;
  uint8_t newSalt[16], key[32], verifier[32];
  esp_fill_random(newSalt, sizeof(newSalt));
  bool ok = next.clear() && derivePinKey(pin, newSalt, key, verifier);
  for (int i = 0; ok && i < credCount; i++) {
    char name[5]; snprintf(name, sizeof(name), "c%d", i);
    // Recifra o registro armazenado; nao depende de um carregamento parcial em RAM.
    String enc = prefs.getString(name, "");
    if (enc.length() < 64 || enc.length() % 32 != 0) { ok = false; break; }
    String plain = decryptData(enc);
    int p1 = plain.indexOf('|'), p2 = plain.indexOf('|', p1 + 1);
    ok = p1 > 0 && p1 <= 32 && p2 > p1 && p2 - p1 - 1 <= 64 &&
         plain.length() - p2 - 1 <= 64;
    if (ok) {
      String encrypted = encryptData(plain.c_str(), key);
      ok = encrypted.length() >= 64 && encrypted.length() % 32 == 0 &&
           next.putString(name, encrypted) == encrypted.length();
    }
    mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
  }
  if (ok) ok = next.putInt("cnt", credCount) == sizeof(int32_t);
  if (ok) ok = next.putUChar("plen", length) == sizeof(uint8_t);
  if (ok) ok = next.putBytes("salt2", newSalt, sizeof(newSalt)) == sizeof(newSalt);
  if (ok) ok = next.putBytes("pv2", verifier, sizeof(verifier)) == sizeof(verifier);
  next.end();
  mbedtls_platform_zeroize(newSalt, sizeof(newSalt));
  mbedtls_platform_zeroize(key, sizeof(key));
  mbedtls_platform_zeroize(verifier, sizeof(verifier));
  if (!ok || prefs.putBool("pinchange", true) != sizeof(bool)) return PIN_CHANGE_FAILED;
  return finishPinChange() ? PIN_CHANGE_OK : PIN_CHANGE_PENDING;
}

void loadLanguagePreference() {
  uint8_t language = prefs.getUChar("language", 0);
  activeLanguage = language < LANGUAGE_COUNT ? language : 0;
}

bool storageBegin() {
  storageReady = false;
  if (!prefs.begin("pv", false) || !finishPinChange()) return false;
  finishMigration();
  unsigned long saved = prefs.getULong("autolock", AUTOLOCK_MS);
  autoLockMs = validAutoLock(saved) ? saved : AUTOLOCK_MS;
  uint8_t theme = prefs.getUChar("theme", 0);
  activeTheme = theme < THEME_COUNT ? theme : 0;
  uint8_t brightness = prefs.getUChar("brightness", BRIGHTNESS_DEFAULT);
  displayBrightness = brightness >= BRIGHTNESS_MIN && brightness <= BRIGHTNESS_MAX
                        ? brightness : BRIGHTNESS_DEFAULT;
  uint8_t mode = prefs.getUChar("sort", SORT_ALPHA);
  credentialSort = mode < SORT_COUNT ? mode : uint8_t(SORT_ALPHA);
  loadLanguagePreference();
  storageReady = true;
  return true;
}

bool storageHasPin() {
  return prefs.isKey("pv2") || prefs.isKey("pin");
}

// Tamanho do PIN cadastrado (vaults v1 usavam 4 digitos)
int storagePinLen() {
  return prefs.getUChar("plen", prefs.isKey("pin") ? 4 : PIN_LEN);
}

// ---- Contador persistente de tentativas (sobrevive a reboot) ----
int storageFails() {
  return prefs.getUChar("fails", 0);
}

// Incrementa ANTES de verificar: cortar a energia no meio nao zera o contador
int storageAddFail() {
  int f = storageFails() + 1;
  prefs.putUChar("fails", f > 255 ? 255 : f);
  return f;
}

void storageResetFails() {
  if (storageFails()) prefs.putUChar("fails", 0);
}

// Apaga TODO o vault (PIN, salt e credenciais)
void storageWipe() {
  prefs.clear();
  // Inclui qualquer copia temporaria deixada por uma troca interrompida.
  Preferences next;
  if (next.begin("pvnext", false)) { next.clear(); next.end(); }
  mbedtls_platform_zeroize(aesKey, sizeof(aesKey));
  mbedtls_platform_zeroize(creds, sizeof(creds));
  credCount = 0;
}

// Novo salt + chave v2 em RAM; "pv2" gravado por ultimo = ponto de commit
void setupPin(const char* pin) {
  uint8_t verifier[32];
  esp_fill_random(salt, 16);
  deriveKeyV2(pin, verifier);
  prefs.putUChar("plen", strlen(pin));
  prefs.putUChar("ver", VAULT_FORMAT);
  prefs.putBytes("salt2", salt, 16);
  prefs.putBytes("pv2", verifier, 32);
  mbedtls_platform_zeroize(verifier, sizeof(verifier));
}

// Migra vault v1 -> v2 mantendo o PIN. Seguro contra queda de energia:
// recifra tudo em m_i, faz o commit (pv2) e so entao substitui c_i.
void migrateV1(const char* pin) {
  loadCredentials();                     // decifra com a chave v1
  uint8_t verifier[32];
  esp_fill_random(salt, 16);
  deriveKeyV2(pin, verifier);
  for (int i = 0; i < credCount; i++) {
    String plain = String(creds[i].name) + "|" + creds[i].user + "|" + creds[i].pass;
    char mk[5];
    sprintf(mk, "m%d", i);
    prefs.putString(mk, encryptData(plain.c_str()));
    mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
  }
  prefs.putUChar("plen", strlen(pin));
  prefs.putUChar("ver", VAULT_FORMAT);
  prefs.putBytes("salt2", salt, 16);
  prefs.putBytes("pv2", verifier, 32);   // commit
  mbedtls_platform_zeroize(verifier, sizeof(verifier));
  finishMigration();
}

bool verifyPin(const char* pin) {
  if (prefs.isKey("pv2")) {
    prefs.getBytes("salt2", salt, 16);
    uint8_t stored[32], verifier[32];
    prefs.getBytes("pv2", stored, 32);
    deriveKeyV2(pin, verifier);
    bool ok = ctEqual(stored, verifier, 32);
    mbedtls_platform_zeroize(verifier, sizeof(verifier));
    if (!ok) mbedtls_platform_zeroize(aesKey, sizeof(aesKey));
    return ok;
  }

  // Vault v1
  prefs.getBytes("salt", salt, 16);
  String stored = prefs.getString("pin", "");
  if (stored.length() == 0) return false;
  String h = hashPinV1(pin);
  if (h.length() != stored.length() ||
      !ctEqual((const uint8_t*)h.c_str(), (const uint8_t*)stored.c_str(), h.length()))
    return false;
  deriveKeyV1(pin);
  migrateV1(pin);
  return true;
}

void saveSingleCred(int idx) {
  String plain = String(creds[idx].name) + "|" +
                 String(creds[idx].user) + "|" +
                 String(creds[idx].pass);
  String enc = encryptData(plain.c_str());
  mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
  char key[4];
  sprintf(key, "c%d", idx);
  prefs.putString(key, enc);
  sprintf(key, "f%d", idx);
  prefs.putBool(key, creds[idx].favorite);
  sprintf(key, "u%d", idx);
  prefs.putUInt(key, creds[idx].useCount);
  sprintf(key, "t%d", idx);
  prefs.putUChar(key, creds[idx].category);
}

void loadCredentials() {
  credCount = prefs.getInt("cnt", 0);
  for (int i = 0; i < credCount; i++) {
    char key[4];
    sprintf(key, "f%d", i);
    creds[i].favorite = prefs.getBool(key, false);
    sprintf(key, "u%d", i);
    creds[i].useCount = prefs.getUInt(key, 0);
    sprintf(key, "t%d", i);
    uint8_t category = prefs.getUChar(key, CATEGORY_NONE);
    creds[i].category = category < CATEGORY_COUNT ? category : uint8_t(CATEGORY_NONE);
    sprintf(key, "c%d", i);
    String enc = prefs.getString(key, "");
    if (enc.length() == 0) continue;
    String plain = decryptData(enc);
    int p1 = plain.indexOf('|');
    int p2 = plain.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) continue;
    strncpy(creds[i].name, plain.substring(0, p1).c_str(), 32);
    creds[i].name[32] = 0;
    strncpy(creds[i].user, plain.substring(p1+1, p2).c_str(), 64);
    creds[i].user[64] = 0;
    strncpy(creds[i].pass, plain.substring(p2+1).c_str(), 64);
    creds[i].pass[64] = 0;
    mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
  }
}

bool setCredentialCategory(int idx, uint8_t category) {
  if (idx < 0 || idx >= MAX_CREDS || category >= CATEGORY_COUNT) return false;
  char key[4]; snprintf(key, sizeof(key), "t%d", idx);
  if (prefs.putUChar(key, category) != sizeof(uint8_t)) return false;
  creds[idx].category = category;
  return true;
}

bool addCredential(const char* name, const char* user, const char* pass, bool favorite = false,
                   uint8_t category = CATEGORY_NONE) {
  if (credCount >= MAX_CREDS || !setCredentialCategory(credCount, category)) return false;
  strncpy(creds[credCount].name, name, 32); creds[credCount].name[32] = 0;
  strncpy(creds[credCount].user, user, 64); creds[credCount].user[64] = 0;
  strncpy(creds[credCount].pass, pass, 64); creds[credCount].pass[64] = 0;
  creds[credCount].favorite = favorite;
  creds[credCount].useCount = 0;
  saveSingleCred(credCount);
  credCount++;
  prefs.putInt("cnt", credCount);
  return true;
}

// O sentinela preserva o tipo para clientes antigos que nao enviam esse campo.
bool updateCredential(int idx, const char* name, const char* user, const char* pass,
                      uint8_t category = CATEGORY_COUNT) {
  if (idx < 0 || idx >= credCount) return false;
  if (category != CATEGORY_COUNT && !setCredentialCategory(idx, category)) return false;
  strncpy(creds[idx].name, name, 32); creds[idx].name[32] = 0;
  strncpy(creds[idx].user, user, 64); creds[idx].user[64] = 0;
  strncpy(creds[idx].pass, pass, 64); creds[idx].pass[64] = 0;
  saveSingleCred(idx);
  return true;
}

// Atualiza apenas a marca, sem recifrar a credencial ou mudar seu ID.
bool setCredentialFavorite(int idx, bool favorite) {
  if (idx < 0 || idx >= credCount) return false;
  char key[4];
  sprintf(key, "f%d", idx);
  if (prefs.putBool(key, favorite) == 0) return false;
  creds[idx].favorite = favorite;
  return true;
}

// Ordenacao apenas para exibicao: IDs de armazenamento continuam estaveis.
// No modo Por tipo, favoritas primeiro dentro de cada grupo; Sem tipo por ultimo.
// Nos demais modos, favoritas primeiro. O indice preserva a ordem de cadastro,
// inclusive apos exclusoes; editar nao transforma uma conta em cadastro recente.
bool credentialComesBefore(int a, int b) {
  if (credentialSort == SORT_CATEGORY && creds[a].category != creds[b].category) {
    int ca = creds[a].category ? creds[a].category : CATEGORY_COUNT;
    int cb = creds[b].category ? creds[b].category : CATEGORY_COUNT;
    return ca < cb;
  }
  if (creds[a].favorite != creds[b].favorite) return creds[a].favorite;
  if (credentialSort == SORT_RECENT) return a > b;
  if (credentialSort == SORT_USED && creds[a].useCount != creds[b].useCount)
    return creds[a].useCount > creds[b].useCount;
  int byName = strcasecmp(creds[a].name, creds[b].name);
  return byName != 0 ? byName < 0 : a < b;
}

int credentialDisplayIndex(int position) {
  if (position < 0 || position >= credCount) return -1;
  int order[MAX_CREDS];
  for (int i = 0; i < credCount; i++) {
    int j = i;
    while (j > 0 && credentialComesBefore(i, order[j - 1])) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = i;
  }
  return order[position];
}

bool deleteCredential(int idx) {
  if (idx < 0 || idx >= credCount) return false;
  for (int i = idx; i < credCount - 1; i++) {
    creds[i] = creds[i + 1];
    saveSingleCred(i);
  }
  credCount--;
  prefs.putInt("cnt", credCount);
  char key[4]; sprintf(key, "c%d", credCount);
  prefs.remove(key);
  sprintf(key, "f%d", credCount);
  prefs.remove(key);
  sprintf(key, "u%d", credCount);
  prefs.remove(key);
  sprintf(key, "t%d", credCount);
  prefs.remove(key);
  mbedtls_platform_zeroize(&creds[credCount], sizeof(Credential));
  return true;
}

// ---- Backup (exportar/importar) ----
// Arquivo JSON independente do PIN e do aparelho:
//   bmaster = PBKDF2-HMAC-SHA256(senha do backup, bsalt, iter)
//   chave = SHA-256(bmaster||"bkey"), verificador = SHA-256(bmaster||"bver"),
//   chave do MAC = SHA-256(bmaster||"bmac"); MAC = HMAC(salt + "," + c0 + "," + c1 ...)
// v2/v3 incluem a versao antes do salt no MAC para autentica-la.
// v1: name|user|pass; v2: favorite|name|user|pass; v3: category|favorite|name|user|pass
// -> AES-256-CBC (IV aleatorio) -> hex

#define BACKUP_ITERS_MIN  1000
#define BACKUP_ITERS_MAX  200000

String bytesToHex(const uint8_t* b, size_t n) {
  String hex = "";
  for (size_t i = 0; i < n; i++) { char t[3]; sprintf(t, "%02x", b[i]); hex += t; }
  return hex;
}

bool hexToBytes(const String& hex, uint8_t* out, size_t n) {
  if (hex.length() != n * 2) return false;
  for (size_t i = 0; i < n; i++) {
    char h[3] = { hex[i*2], hex[i*2+1], 0 };
    if (!isxdigit(h[0]) || !isxdigit(h[1])) return false;
    out[i] = strtoul(h, NULL, 16);
  }
  return true;
}

void deriveBackupKeys(const char* pw, const uint8_t* bsalt, int iters,
                      uint8_t* key, uint8_t* ver, uint8_t* macKey) {
  uint8_t master[32];
  mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
                                (const uint8_t*)pw, strlen(pw),
                                bsalt, 16, iters, sizeof(master), master);
  sha256Tagged(master, 32, "bkey", key);
  sha256Tagged(master, 32, "bver", ver);
  sha256Tagged(master, 32, "bmac", macKey);
  mbedtls_platform_zeroize(master, sizeof(master));
}

void backupMac(const uint8_t* macKey, const String& saltHex,
               const String* entries, int n, uint8_t* out) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, macKey, 32);
  mbedtls_md_hmac_update(&ctx, (const uint8_t*)saltHex.c_str(), saltHex.length());
  for (int i = 0; i < n; i++) {
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)",", 1);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)entries[i].c_str(), entries[i].length());
  }
  mbedtls_md_hmac_finish(&ctx, out);
  mbedtls_md_free(&ctx);
}

// Gera o JSON do backup com todas as credenciais
String exportBackup(const char* pw) {
  uint8_t bsalt[16], key[32], ver[32], macKey[32], mac[32];
  esp_fill_random(bsalt, 16);
  deriveBackupKeys(pw, bsalt, KDF_ITERS, key, ver, macKey);
  String saltHex = bytesToHex(bsalt, 16);

  static String entries[MAX_CREDS];
  for (int i = 0; i < credCount; i++) {
    String plain = String(creds[i].category) + "|" + (creds[i].favorite ? "1|" : "0|") + creds[i].name +
                   "|" + creds[i].user + "|" + creds[i].pass;
    entries[i] = encryptData(plain.c_str(), key);
    mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
  }
  backupMac(macKey, String("3,") + saltHex, entries, credCount, mac);

  String json = "{\"app\":\"PassVault\",\"v\":3,\"iter\":" + String(KDF_ITERS) +
                ",\"salt\":\"" + saltHex + "\",\"ver\":\"" + bytesToHex(ver, 32) +
                "\",\"mac\":\"" + bytesToHex(mac, 32) + "\",\"creds\":[";
  for (int i = 0; i < credCount; i++) {
    if (i) json += ",";
    json += "\"" + entries[i] + "\"";
    entries[i] = "";
  }
  json += "]}";
  mbedtls_platform_zeroize(key, 32);
  mbedtls_platform_zeroize(macKey, 32);
  return json;
}

// Importa credenciais de um backup, ignorando duplicadas (mesmo nome+usuario).
// Retorno: >=0 adicionadas, -1 senha errada, -2 arquivo invalido/alterado
int importBackup(const char* pw, int iters, const String& saltHex, const String& verHex,
                 const String& macHex, const String* entries, int n, int* skipped, int version = 1) {
  uint8_t bsalt[16], fver[32], fmac[32], key[32], ver[32], macKey[32], mac[32];
  *skipped = 0;
  if ((version < 1 || version > 3) || iters < BACKUP_ITERS_MIN || iters > BACKUP_ITERS_MAX || n < 0 || n > MAX_CREDS ||
      !hexToBytes(saltHex, bsalt, 16) || !hexToBytes(verHex, fver, 32) ||
      !hexToBytes(macHex, fmac, 32)) return -2;

  deriveBackupKeys(pw, bsalt, iters, key, ver, macKey);
  int result = 0;
  if (!ctEqual(ver, fver, 32)) { result = -1; goto done; }
  backupMac(macKey, version >= 2 ? String(version) + "," + saltHex : saltHex, entries, n, mac);
  if (!ctEqual(mac, fmac, 32)) { result = -2; goto done; }

  for (int i = 0; i < n; i++) {
    String plain = decryptData(entries[i], key);
    bool favorite = false;
    uint8_t category = CATEGORY_NONE;
    if (version == 3) {
      if (plain.length() < 2 || plain[0] < '0' || plain[0] >= '0' + CATEGORY_COUNT || plain[1] != '|') {
        mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
        (*skipped)++;
        continue;
      }
      category = plain[0] - '0';
      String payload = plain.substring(2);
      mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
      plain = payload;
      mbedtls_platform_zeroize((void*)payload.c_str(), payload.length());
    }
    if (version >= 2) {
      if (!plain.startsWith("0|") && !plain.startsWith("1|")) {
        mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
        (*skipped)++;
        continue;
      }
      favorite = plain[0] == '1';
      String payload = plain.substring(2);
      mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
      plain = payload;
      mbedtls_platform_zeroize((void*)payload.c_str(), payload.length());
    }
    int p1 = plain.indexOf('|');
    int p2 = plain.indexOf('|', p1 + 1);
    if (p1 > 0 && p2 > p1) {
      String name = plain.substring(0, p1), user = plain.substring(p1 + 1, p2);
      String pass = plain.substring(p2 + 1);
      bool dup = false;
      for (int j = 0; j < credCount && !dup; j++)
        dup = name == creds[j].name && user == creds[j].user;
      if (dup || !addCredential(name.c_str(), user.c_str(), pass.c_str(), favorite, category)) (*skipped)++;
      else result++;
      mbedtls_platform_zeroize((void*)pass.c_str(), pass.length());
    } else {
      (*skipped)++;
    }
    mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
  }
done:
  mbedtls_platform_zeroize(key, 32);
  mbedtls_platform_zeroize(macKey, 32);
  return result;
}

void clearMemory() {
  mbedtls_platform_zeroize(aesKey, sizeof(aesKey));
  mbedtls_platform_zeroize(creds, sizeof(creds));
  credCount = 0;
}

#endif
