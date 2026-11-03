#ifndef VAULT_SYNC_DATA_H
#define VAULT_SYNC_DATA_H

#include "vault_core.h"

// Wire v1: explicit byte fields, no compiler padding, PIN, key or usage history.
constexpr size_t SYNC_ENTRY_SIZE = 165;
constexpr size_t SYNC_DATA_SIZE = 1 + MAX_CREDS * SYNC_ENTRY_SIZE;

void syncEncode(uint8_t* data) {
  memset(data, 0, SYNC_DATA_SIZE);
  data[0] = credCount;
  for (int i = 0; i < credCount; i++) {
    uint8_t* p = data + 1 + i * SYNC_ENTRY_SIZE;
    memcpy(p, creds[i].name, 33);
    memcpy(p + 33, creds[i].user, 65);
    memcpy(p + 98, creds[i].pass, 65);
    p[163] = creds[i].favorite ? 1 : 0;
    p[164] = creds[i].category;
  }
}

bool syncValidField(const uint8_t* p, size_t n, bool required, bool allowPipe = false) {
  const uint8_t* end = (const uint8_t*)memchr(p, 0, n);
  if (!end || (required && end == p)) return false;
  // The local storage format uses | as a separator.
  for (const uint8_t* c = p; c < end; c++)
    if (*c == '|' && !allowPipe) return false;
  return true;
}

bool syncValidData(const uint8_t* data, size_t size) {
  if (size != SYNC_DATA_SIZE || data[0] > MAX_CREDS) return false;
  for (int i = 0; i < data[0]; i++) {
    const uint8_t* p = data + 1 + i * SYNC_ENTRY_SIZE;
    if (!syncValidField(p, 33, true) || !syncValidField(p + 33, 65, false) ||
        !syncValidField(p + 98, 65, false, true) || p[163] > 1 || p[164] >= CATEGORY_COUNT)
      return false;
  }
  return true;
}

bool syncSameAccount(const uint8_t* a, const uint8_t* b) {
  return strcmp((const char*)a, (const char*)b) == 0 &&
         strcmp((const char*)a + 33, (const char*)b + 33) == 0;
}

// Append only, with the count written last. Existing records are never touched.
// On power loss the incomplete tail is invisible and overwritten on retry.
bool syncAppend(const uint8_t* p) {
  int idx = credCount;
  if (idx >= MAX_CREDS) return false;
  String plain = String((const char*)p) + "|" + (const char*)(p + 33) + "|" + (const char*)(p + 98);
  String enc = encryptData(plain.c_str());
  mbedtls_platform_zeroize((void*)plain.c_str(), plain.length());
  if (!enc.length()) return false;
  char key[8];
  snprintf(key, sizeof(key), "c%d", idx);
  if (prefs.putString(key, enc) != enc.length()) return false;
  snprintf(key, sizeof(key), "f%d", idx);
  if (prefs.putBool(key, p[163]) != sizeof(bool)) return false;
  snprintf(key, sizeof(key), "t%d", idx);
  if (prefs.putUChar(key, p[164]) != sizeof(uint8_t)) return false;
  snprintf(key, sizeof(key), "u%d", idx);
  if (prefs.putUInt(key, 0) != sizeof(uint32_t)) return false;
  if (prefs.putInt("cnt", idx + 1) != sizeof(int32_t)) return false;
  Credential& c = creds[idx];
  memset(&c, 0, sizeof(c));
  memcpy(c.name, p, 33); memcpy(c.user, p + 33, 65); memcpy(c.pass, p + 98, 65);
  c.favorite = p[163]; c.category = p[164];
  credCount++;
  return true;
}

// Validate and check capacity before the first write. Retry is idempotent.
// -1 invalid, -2 capacity, -3 storage failure (added may be nonzero).
int syncMerge(const uint8_t* data, size_t size, int& added, int& skipped) {
  added = skipped = 0;
  if (!syncValidData(data, size)) return -1;
  bool append[MAX_CREDS] = {};
  int needed = 0;
  for (int i = 0; i < data[0]; i++) {
    const uint8_t* p = data + 1 + i * SYNC_ENTRY_SIZE;
    bool duplicate = false;
    for (int j = 0; j < credCount && !duplicate; j++)
      duplicate = strcmp((const char*)p, creds[j].name) == 0 &&
                  strcmp((const char*)p + 33, creds[j].user) == 0;
    for (int j = 0; j < i && !duplicate; j++)
      duplicate = syncSameAccount(p, data + 1 + j * SYNC_ENTRY_SIZE);
    append[i] = !duplicate;
    if (duplicate) skipped++; else needed++;
  }
  if (credCount + needed > MAX_CREDS) return -2;
  for (int i = 0; i < data[0]; i++) {
    if (!append[i]) continue;
    if (!syncAppend(data + 1 + i * SYNC_ENTRY_SIZE)) return -3;
    added++;
  }
  return 0;
}

#endif
