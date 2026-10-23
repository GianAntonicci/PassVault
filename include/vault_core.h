#ifndef VAULT_CORE_H
#define VAULT_CORE_H

#include <Preferences.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform_util.h>
#include "config.h"

// ---- Credential struct ----
struct Credential {
  char name[33];
  char user[65];
  char pass[65];
};

// ---- Globals ----
Preferences prefs;
Credential  creds[MAX_CREDS];
int         credCount = 0;
uint8_t     aesKey[32];
uint8_t     salt[16];

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

String encryptData(const char* plain) {
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
  mbedtls_aes_setkey_enc(&aes, aesKey, 256);
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

String decryptData(const String& hexData) {
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
  mbedtls_aes_setkey_dec(&aes, aesKey, 256);
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

// Deriva chave (aesKey) e verificador a partir do PIN e do salt atual
void deriveKeyV2(const char* pin, uint8_t* verifier) {
  uint8_t master[32];
  mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
                                (const uint8_t*)pin, strlen(pin),
                                salt, sizeof(salt), KDF_ITERS,
                                sizeof(master), master);
  sha256Tagged(master, 32, "key", aesKey);
  sha256Tagged(master, 32, "ver", verifier);
  mbedtls_platform_zeroize(master, sizeof(master));
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

void storageBegin() {
  prefs.begin("pv", false);
  finishMigration();
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
}

void loadCredentials() {
  credCount = prefs.getInt("cnt", 0);
  for (int i = 0; i < credCount; i++) {
    char key[4];
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

bool addCredential(const char* name, const char* user, const char* pass) {
  if (credCount >= MAX_CREDS) return false;
  strncpy(creds[credCount].name, name, 32); creds[credCount].name[32] = 0;
  strncpy(creds[credCount].user, user, 64); creds[credCount].user[64] = 0;
  strncpy(creds[credCount].pass, pass, 64); creds[credCount].pass[64] = 0;
  saveSingleCred(credCount);
  credCount++;
  prefs.putInt("cnt", credCount);
  return true;
}

bool updateCredential(int idx, const char* name, const char* user, const char* pass) {
  if (idx < 0 || idx >= credCount) return false;
  strncpy(creds[idx].name, name, 32); creds[idx].name[32] = 0;
  strncpy(creds[idx].user, user, 64); creds[idx].user[64] = 0;
  strncpy(creds[idx].pass, pass, 64); creds[idx].pass[64] = 0;
  saveSingleCred(idx);
  return true;
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
  return true;
}

void clearMemory() {
  mbedtls_platform_zeroize(aesKey, sizeof(aesKey));
  mbedtls_platform_zeroize(creds, sizeof(creds));
  credCount = 0;
}

#endif
