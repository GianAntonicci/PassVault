"""Host fault-injection tests for the production PIN transaction (crypto mocked).

Run: python3 test/test_pin_change.py
Requires a C++17 compiler. No device, packages or real credentials are used.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile


root = Path(__file__).resolve().parents[1]
source = (root / "include/vault_core.h").read_text()
# Compile the actual transaction code, with deterministic crypto and an NVS fake.
transaction = source[source.index("bool finishPinChange() {"):source.index("bool storageBegin() {")]
wipe = source[source.index("void storageWipe() {"):source.index("// Novo salt + chave v2")]
support = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include <iostream>
struct String : std::string {
  using std::string::string;
  String(std::string s) : std::string(s) {}
  int indexOf(char c, int start = 0) const {
    auto pos = find(c, start); return pos == npos ? -1 : int(pos);
  }
};
using Bytes = std::vector<uint8_t>;
using Store = std::map<std::string, std::map<std::string, Bytes>>;
Store disk;
struct PowerCut {};
int writes = 0, cut = -1, fail = -1;
bool gate() {
  int n = writes++;
  if (n == cut) throw PowerCut{};
  return fail < 0 || n < fail;
}
struct Preferences {
  std::string ns;
  bool begin(const char* name, bool = false) { ns = name; return true; }
  void end() {}
  bool clear() { if (!gate()) return false; disk[ns].clear(); return true; }
  bool remove(const char* k) { if (!gate()) return false; return disk[ns].erase(k); }
  size_t putBytes(const char* k, const void* p, size_t n) {
    if (!gate()) return 0;
    disk[ns][k] = Bytes((const uint8_t*)p, (const uint8_t*)p + n); return n;
  }
  size_t getBytes(const char* k, void* p, size_t n) {
    auto it = disk[ns].find(k);
    if (it == disk[ns].end() || it->second.size() != n) return 0;
    memcpy(p, it->second.data(), n); return n;
  }
  int getInt(const char* k, int d = 0) { int32_t v; return getBytes(k, &v, 4) ? v : d; }
  size_t putInt(const char* k, int v) { int32_t n = v; return putBytes(k, &n, 4); }
  uint8_t getUChar(const char* k, uint8_t d = 0) { uint8_t v; return getBytes(k, &v, 1) ? v : d; }
  size_t putUChar(const char* k, uint8_t v) { return putBytes(k, &v, 1); }
  bool getBool(const char* k, bool d = false) { return getUChar(k, d); }
  size_t putBool(const char* k, bool v) { return putUChar(k, v); }
  String getString(const char* k, const char* d = "") {
    auto it = disk[ns].find(k);
    if (it == disk[ns].end()) return String(d);
    return String(std::string(it->second.begin(), it->second.end()));
  }
  size_t putString(const char* k, const String& s) { return putBytes(k, s.data(), s.size()); }
};
Preferences prefs;
constexpr int MAX_CREDS = 30, PIN_LEN = 6, PIN_LEN_MAX = 8, VAULT_FORMAT = 2;
int credCount = 0;
uint8_t aesKey[32], salt[16];
uint8_t creds[128];
void mbedtls_platform_zeroize(void* p, size_t n) { memset(p, 0, n); }
void esp_fill_random(void* p, size_t n) { memset(p, 42, n); }
bool derivePinKey(const char* pin, const uint8_t* s, uint8_t* key, uint8_t* ver) {
  for (int i = 0; i < 32; ++i) { key[i] = pin[i % strlen(pin)] ^ s[i % 16]; ver[i] = key[i] ^ 123; }
  return true;
}
// Deterministic reversible stand-in: same block/hex sizing as encryptData.
String encryptData(const char* plain, const uint8_t* key = aesKey) {
  std::string data(plain), enc(32, '0');
  int pad = 16 - data.size() % 16; data.append(pad, char(pad));
  for (size_t i = 0; i < data.size(); ++i) {
    char h[3]; snprintf(h, sizeof(h), "%02x", uint8_t(data[i]) ^ key[i % 32]); enc += h;
  }
  return String(enc);
}
String decryptData(const String& enc, const uint8_t* key = aesKey) {
  std::string data;
  for (size_t i = 32; i < enc.size(); i += 2)
    data += char(std::stoul(enc.substr(i, 2), nullptr, 16) ^ key[((i - 32) / 2) % 32]);
  int pad = uint8_t(data.back());
  if (pad < 1 || pad > 16 || size_t(pad) > data.size()) return String("");
  for (int i = 0; i < pad; ++i) if (uint8_t(data[data.size() - 1 - i]) != pad) return String("");
  return String(data.substr(0, data.size() - pad));
}
'''
checks = r'''
void seed(int n, int oldLength) {
  disk.clear(); writes = 0; cut = fail = -1; credCount = n;
  prefs.begin("pv"); memset(salt, 7, 16);
  uint8_t ver[32]; derivePinKey(oldLength == 4 ? "1234" : "123456", salt, aesKey, ver);
  prefs.putBytes("salt2", salt, 16); prefs.putBytes("pv2", ver, 32);
  prefs.putUChar("plen", oldLength); prefs.putInt("cnt", n);
  prefs.putUChar("theme", 2); prefs.putInt("autolock", 120000);
  for (int i = 0; i < n; ++i) {
    prefs.putString(("c" + std::to_string(i)).c_str(), encryptData(("Conta" + std::to_string(i) + "|user|secret").c_str()));
    prefs.putBool(("f" + std::to_string(i)).c_str(), i % 2);
    prefs.putUChar(("t" + std::to_string(i)).c_str(), i % 5);
  }
  writes = 0;
}
void assertVault(int n, int oldLength) {
  uint8_t s[16], k[32], ver[32], stored[32];
  assert(prefs.getBytes("salt2", s, 16) == 16);
  bool changed = s[0] == 42;
  derivePinKey(changed ? "654321" : oldLength == 4 ? "1234" : "123456", s, k, ver);
  assert(prefs.getBytes("pv2", stored, 32) == 32 && memcmp(ver, stored, 32) == 0);
  assert(prefs.getUChar("plen") == (changed ? 6 : oldLength));
  assert(prefs.getInt("cnt") == n);
  assert(prefs.getUChar("theme") == 2 && prefs.getInt("autolock") == 120000);
  for (int i = 0; i < n; ++i) {
    auto id = std::to_string(i);
    assert(decryptData(prefs.getString(("c" + id).c_str()), k) == "Conta" + id + "|user|secret");
    assert(prefs.getBool(("f" + id).c_str()) == bool(i % 2));
    assert(prefs.getUChar(("t" + id).c_str()) == i % 5);
  }
}
int main() {
  int scenarios = 0;
  for (int n : {0, 2, 30}) for (int oldLength : {4, 6}) {
    seed(n, oldLength);
    assert(changePin("654321") == PIN_CHANGE_OK);
    int total = writes; assertVault(n, oldLength);
    for (int at = 0; at <= total; ++at) {
      seed(n, oldLength); cut = at;
      try { changePin("654321"); } catch (const PowerCut&) {}
      // Reboot with all NVS writes succeeding, using only persistent state.
      cut = -1; memset(aesKey, 0, 32); memset(salt, 0, 16);
      assert(finishPinChange()); assertVault(n, oldLength); ++scenarios;

      seed(n, oldLength); fail = at;
      auto result = changePin("654321");
      if (result == PIN_CHANGE_PENDING) {
        assert(!finishPinChange()); // Persistent write failures must fail closed.
        // Also cut power at each write of recovery, then recover again.
        Store pending = disk; fail = -1; writes = 0;
        assert(finishPinChange()); int recoveryWrites = writes;
        for (int retry = 0; retry <= recoveryWrites; ++retry) {
          disk = pending; writes = 0; cut = retry;
          try { finishPinChange(); } catch (const PowerCut&) {}
          cut = -1; assert(finishPinChange()); assertVault(n, oldLength); ++scenarios;
        }
      }
      fail = -1; assert(finishPinChange()); assertVault(n, oldLength); ++scenarios;
    }
  }
  seed(2, 6); Store original = disk;
  assert(changePin("12345") == PIN_CHANGE_FAILED && disk == original);
  assert(changePin("abc123") == PIN_CHANGE_FAILED && disk == original);
  // Incomplete staging must never replace the active vault.
  prefs.putBool("pinchange", true); Store beforeRecovery = disk;
  assert(!finishPinChange() && disk["pv"] == beforeRecovery["pv"]);
  seed(2, 6); prefs.putString("c1", "invalid"); original = disk;
  assert(changePin("654321") == PIN_CHANGE_FAILED && disk["pv"] == original["pv"]);
  // Full vault wipe must also erase any interrupted staging copy.
  seed(2, 6); fail = 8;
  changePin("654321"); fail = -1;
  assert(!disk["pvnext"].empty());
  storageWipe();
  assert(disk["pv"].empty() && disk["pvnext"].empty() && credCount == 0);
  std::cout << "PASS: " << scenarios << " power-cut/write-failure scenarios; invalid PIN and corrupt staging/records\n";
}
'''

with tempfile.TemporaryDirectory(prefix="passvault-pin-test-") as temp:
    cpp = Path(temp) / "test.cpp"
    binary = Path(temp) / "test"
    cpp.write_text(support + transaction + wipe + checks)
    compiler = shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise SystemExit("A C++17 compiler (clang++ or g++) is required")
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
