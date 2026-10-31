"""Host tests for production sorting, usage and credential persistence.
Run: python3 test/test_credential_sort.py
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'include/vault_core.h').read_text()
struct = source[source.index('struct Credential {'):source.index('// ---- Globals ----')]
settings = source[source.index('enum CredentialSort'):source.index('bool validAutoLock')]
crud = source[source.index('void saveSingleCred(int idx) {'):source.index('// ---- Backup')]
support = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <strings.h>
#include <cstdio>
#include <string>
#include <map>
#include <iostream>
struct String : std::string {
  using std::string::string;
  String(std::string s) : std::string(s) {}
  int indexOf(char c, int start = 0) const {
    auto pos = find(c, start); return pos == npos ? -1 : int(pos);
  }
  String substring(int start, int end) const { return substr(start, end - start); }
  String substring(int start) const { return substr(start); }
};
struct Preferences {
  std::map<std::string, String> text;
  std::map<std::string, uint32_t> numbers;
  bool fail = false;
  size_t putUInt(const char* k, uint32_t v) { if(fail)return 0; numbers[k]=v;return 4; }
  uint32_t getUInt(const char* k, uint32_t d=0) { return numbers.count(k)?numbers[k]:d; }
  size_t putUChar(const char* k, uint8_t v) { return putUInt(k,v)?1:0; }
  size_t putBool(const char* k, bool v) { return putUInt(k,v)?1:0; }
  bool getBool(const char* k, bool d=false) { return getUInt(k,d); }
  size_t putInt(const char* k, int v) { return putUInt(k,v); }
  int getInt(const char* k, int d=0) { return getUInt(k,d); }
  size_t putString(const char* k, const String& s) { text[k]=s;return s.size(); }
  String getString(const char* k, const char* d="") { return text.count(k)?text[k]:String(d); }
  void remove(const char* k) { text.erase(k);numbers.erase(k); }
} prefs;
constexpr int MAX_CREDS=30;
String encryptData(const char* s) { return String(s); }
String decryptData(const String& s) { return s; }
void mbedtls_platform_zeroize(void* p, size_t n) { memset(p,0,n); }
'''
checks = r'''
void order(std::initializer_list<int> expected) {
  int position=0;
  for(int i:expected) assert(credentialDisplayIndex(position++)==i);
  assert(credentialDisplayIndex(-1)==-1);
  assert(credentialDisplayIndex(credCount)==-1);
}
int main() {
  order({});
  addCredential("zulu","u","p");
  addCredential("beta","u","p");
  addCredential("Alpha","u","p");
  addCredential("alpha","u","p");
  order({2,3,1,0}); // Case-insensitive, stable ties.
  assert(setCredentialSort(SORT_RECENT));order({3,2,1,0});
  updateCredential(0,"aardvark","edited","p");order({3,2,1,0});
  assert(recordCredentialUse(1));assert(recordCredentialUse(1));
  assert(recordCredentialUse(0));
  assert(setCredentialSort(SORT_USED));order({1,0,2,3});
  assert(setCredentialFavorite(3,true));order({3,1,0,2});
  assert(setCredentialSort(SORT_ALPHA));order({3,0,2,1});
  assert(setCredentialSort(SORT_RECENT));order({3,2,1,0});
  memset(creds,0,sizeof(creds));credCount=0;loadCredentials();
  assert(creds[1].useCount==2 && creds[0].useCount==1 && creds[3].favorite);
  assert(setCredentialSort(SORT_USED));order({3,1,0,2});
  assert(deleteCredential(0));order({2,0,1});
  assert(creds[0].useCount==2 && creds[2].favorite);
  assert(!prefs.numbers.count("u3"));
  assert(addCredential("new","u","p"));assert(creds[3].useCount==0);
  memset(creds,0,sizeof(creds));loadCredentials();order({2,0,1,3});
  // Legacy records without usage metadata start at zero.
  prefs.remove("u0");loadCredentials();assert(creds[0].useCount==0);
  assert(!recordCredentialUse(-1) && !recordCredentialUse(credCount));
  prefs.fail=true;
  assert(!setCredentialSort(SORT_ALPHA) && credentialSort==SORT_USED);
  assert(!recordCredentialUse(0) && creds[0].useCount==0);
  prefs.fail=false;
  assert(!setCredentialSort(SORT_COUNT));
  creds[0].useCount=UINT32_MAX;assert(recordCredentialUse(0));
  assert(creds[0].useCount==UINT32_MAX);
  // Full vault: every mode is a permutation and obeys the comparator.
  while(credCount<MAX_CREDS) addCredential("same","u","p",credCount%3==0);
  for(int mode=0;mode<SORT_COUNT;mode++) {
    assert(setCredentialSort(mode));
    bool seen[MAX_CREDS]={};
    for(int p=0;p<credCount;p++) {
      int id=credentialDisplayIndex(p);assert(id>=0 && id<credCount && !seen[id]);
      seen[id]=true;
      if(p) assert(credentialComesBefore(credentialDisplayIndex(p-1),id));
    }
  }
  std::cout << "PASS: sorting, favorites, stable IDs, edits, usage persistence, legacy records, deletion, full vault and write failures\n";
}
'''
with tempfile.TemporaryDirectory(prefix='passvault-sort-test-') as temp:
    cpp = Path(temp) / 'test.cpp'
    binary = Path(temp) / 'test'
    cpp.write_text(support + struct + '\nCredential creds[MAX_CREDS]; int credCount=0;\n' + settings + crud + checks)
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('A C++17 compiler is required')
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
