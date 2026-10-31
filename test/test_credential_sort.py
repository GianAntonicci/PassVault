"""Host tests for production sorting, categories, persistence and backup formats.
Crypto is mocked; tests exercise the production serialization and import paths.
Run: python3 test/test_credential_sort.py
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'include/vault_core.h').read_text()
struct = source[source.index('enum CredentialCategory'):source.index('// ---- Globals ----')]
settings = source[source.index('enum CredentialSort'):source.index('bool validAutoLock')]
backup = (source[source.index('#define BACKUP_ITERS_MIN'):source.index('void deriveBackupKeys')]
          + source[source.index('// Gera o JSON do backup'):source.index('void clearMemory()')])
web = (root / 'include/vault_web.h').read_text()
api = (web[web.index('void handleSave() {'):web.index('void handleFavorite() {')]
       + web[web.index('void handleSortSave() {'):web.index('void handleGet() {')])
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
#include <vector>
#include <cctype>
struct String : std::string {
  using std::string::string;
  String(std::string s) : std::string(s) {}
  String(int n) : std::string(std::to_string(n)) {}
  friend String operator+(const String& a, const String& b) { return String(std::string(a)+std::string(b)); }
  friend String operator+(const String& a, const char* b) { return String(std::string(a)+b); }
  friend String operator+(const char* a, const String& b) { return String(a+std::string(b)); }
  int toInt() const { return std::atoi(c_str()); }
  bool startsWith(const char* prefix) const { return find(prefix)==0; }
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
  uint8_t getUChar(const char* k, uint8_t d=0) { return getUInt(k,d); }
  size_t putUChar(const char* k, uint8_t v) { return putUInt(k,v)?1:0; }
  size_t putBool(const char* k, bool v) { return putUInt(k,v)?1:0; }
  bool getBool(const char* k, bool d=false) { return getUInt(k,d); }
  size_t putInt(const char* k, int v) { return putUInt(k,v); }
  int getInt(const char* k, int d=0) { return getUInt(k,d); }
  size_t putString(const char* k, const String& s) { text[k]=s;return s.size(); }
  String getString(const char* k, const char* d="") { return text.count(k)?text[k]:String(d); }
  void remove(const char* k) { text.erase(k);numbers.erase(k); }
} prefs;
struct WebServerFake {
  std::map<std::string,String> args;
  int status=0;
  String response;
  bool hasArg(const char* name) { return args.count(name); }
  String arg(const char* name) { return args[name]; }
  void send(int code, const char*, const String& text) { status=code;response=text; }
} server;
void webTouch() {}
constexpr int MAX_CREDS=30;
std::vector<String> encrypted;
String encryptData(const char* s, const uint8_t* = nullptr) { encrypted.emplace_back(s);return String(s); }
String decryptData(const String& s, const uint8_t* = nullptr) { return s; }
constexpr int KDF_ITERS=20000;
void esp_fill_random(void* p, size_t n) { memset(p,42,n); }
bool ctEqual(const uint8_t* a, const uint8_t* b, size_t n) { return memcmp(a,b,n)==0; }
void deriveBackupKeys(const char*, const uint8_t*, int, uint8_t* key, uint8_t* ver, uint8_t* mac) {
  memset(key,1,32);memset(ver,2,32);memset(mac,3,32);
}
// Deterministic stand-in; tests version/entry binding, not cryptography.
void backupMac(const uint8_t*, const String& seed, const String* entries, int n, uint8_t* out) {
  std::string input=seed;
  for(int i=0;i<n;i++) input += "," + entries[i];
  uint32_t h=2166136261u;
  for(unsigned char c:input) h=(h^c)*16777619u;
  for(int i=0;i<32;i++) out[i]=uint8_t(h >> ((i%4)*8));
}
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
  // Category groups precede favorites; within each group favorites lead.
  prefs = Preferences{};credCount=0;
  assert(addCredential("legacy","u","password|with|pipes",true));
  assert(addCredential("social z","u","p",false,CATEGORY_SOCIAL));
  assert(addCredential("social a","u","p",true,CATEGORY_SOCIAL));
  assert(addCredential("email","u","p",false,CATEGORY_EMAIL));
  assert(addCredential("bank","u","p",true,CATEGORY_BANK));
  assert(addCredential("work","u","p",false,CATEGORY_WORK));
  assert(setCredentialSort(SORT_CATEGORY));order({2,1,3,4,5,0});
  assert(updateCredential(1,"social b","u","p")); // Missing type preserves it.
  assert(creds[1].category==CATEGORY_SOCIAL);
  assert(updateCredential(1,"bank b","u","p",CATEGORY_BANK));order({2,3,4,1,5,0});
  memset(creds,0,sizeof(creds));loadCredentials();order({2,3,4,1,5,0});
  assert(std::string(creds[0].pass)=="password|with|pipes");
  assert(deleteCredential(3));order({2,3,1,4,0});
  assert(!prefs.numbers.count("t5"));
  memset(creds,0,sizeof(creds));loadCredentials();order({2,3,1,4,0});
  prefs.remove("t1");prefs.numbers["t2"]=255;loadCredentials();
  assert(creds[1].category==CATEGORY_NONE && creds[2].category==CATEGORY_NONE);
  assert(!addCredential("invalid","u","p",false,CATEGORY_COUNT));
  assert(!updateCredential(0,"invalid","u","p",255));
  prefs.fail=true;
  assert(!updateCredential(0,"changed","u","p",CATEGORY_WORK));
  assert(creds[0].category==CATEGORY_NONE && std::string(creds[0].name)=="legacy");
  assert(!addCredential("failed","u","p",false,CATEGORY_WORK));
  // Export every type, restore all fields and authenticate the format version.
  prefs=Preferences{};credCount=0;encrypted.clear();
  for(int type=0;type<CATEGORY_COUNT;type++)
    assert(addCredential(("account"+std::to_string(type)).c_str(),"user","p|with|pipes",type%2,type));
  encrypted.clear();
  String json=exportBackup("backup-password");
  assert(json.find("\"v\":3")!=std::string::npos);
  assert(encrypted.size()==CATEGORY_COUNT);
  for(int type=0;type<CATEGORY_COUNT;type++)
    assert(encrypted[type]==std::to_string(type)+"|"+std::to_string(type%2)+"|account"+std::to_string(type)+"|user|p|with|pipes");
  String entries[MAX_CREDS];
  for(int i=0;i<CATEGORY_COUNT;i++) entries[i]=encrypted[i];
  uint8_t bsalt[16],ver[32],mac[32],key[32]={};
  memset(bsalt,42,16);memset(ver,2,32);
  String saltHex=bytesToHex(bsalt,16),verHex=bytesToHex(ver,32);
  backupMac(key,String("3,")+saltHex,entries,CATEGORY_COUNT,mac);
  String macHex=bytesToHex(mac,32);
  prefs=Preferences{};credCount=0;int skipped=0;
  assert(importBackup("pw",KDF_ITERS,saltHex,verHex,macHex,entries,CATEGORY_COUNT,&skipped,3)==CATEGORY_COUNT);
  assert(skipped==0);
  memset(creds,0,sizeof(creds));loadCredentials();
  for(int i=0;i<CATEGORY_COUNT;i++) {
    assert(creds[i].category==i && creds[i].favorite==bool(i%2));
    assert(std::string(creds[i].pass)=="p|with|pipes");
  }
  assert(importBackup("pw",KDF_ITERS,saltHex,verHex,macHex,entries,CATEGORY_COUNT,&skipped,3)==0);
  assert(skipped==CATEGORY_COUNT);
  assert(importBackup("pw",KDF_ITERS,saltHex,verHex,macHex,entries,CATEGORY_COUNT,&skipped,2)==-2);
  entries[0]="4|0|modified|user|pass";
  assert(importBackup("pw",KDF_ITERS,saltHex,verHex,macHex,entries,CATEGORY_COUNT,&skipped,3)==-2);
  assert(importBackup("pw",KDF_ITERS,saltHex,verHex,macHex,entries,MAX_CREDS+1,&skipped,3)==-2);
  // Legacy backups default to Sem tipo; v1 has no favorites, v2 retains them.
  for(int version=1;version<=2;version++) {
    prefs=Preferences{};credCount=0;
    entries[0]=version==1?"legacy|user|p|pipe":"1|legacy|user|p|pipe";
    backupMac(key,version==1?saltHex:String("2,")+saltHex,entries,1,mac);
    assert(importBackup("pw",KDF_ITERS,saltHex,verHex,bytesToHex(mac,32),entries,1,&skipped,version)==1);
    assert(skipped==0 && creds[0].category==CATEGORY_NONE && creds[0].favorite==(version==2));
    assert(std::string(creds[0].pass)=="p|pipe");
  }
  // Invalid categories are skipped without creating a partial credential.
  prefs=Preferences{};credCount=0;entries[0]="5|1|invalid|u|p";
  backupMac(key,String("3,")+saltHex,entries,1,mac);
  assert(importBackup("pw",KDF_ITERS,saltHex,verHex,bytesToHex(mac,32),entries,1,&skipped,3)==0);
  assert(skipped==1 && credCount==0);
  // Production API: accept only known types, and preserve old-client behavior.
  prefs=Preferences{};credCount=0;
  server.args={{"name","Account"},{"user","u"},{"pass","p"}};
  handleSave();assert(credCount==1 && creds[0].category==CATEGORY_NONE);
  server.args["id"]="0";server.args["category"]="4";
  handleSave();assert(credCount==1 && creds[0].category==CATEGORY_WORK);
  server.args.erase("category");handleSave();assert(creds[0].category==CATEGORY_WORK);
  for(const char* invalid:{"", "-1", "5", "255", "1x", " 1"}) {
    server.args["category"]=invalid;handleSave();
    assert(server.status==400 && credCount==1 && creds[0].category==CATEGORY_WORK);
  }
  for(int type=0;type<CATEGORY_COUNT;type++) {
    server.args["category"]=String(type);handleSave();
    assert(server.status==200 && creds[0].category==type);
  }
  server.args={{"mode","3"}};handleSortSave();
  assert(server.status==200 && credentialSort==SORT_CATEGORY);
  server.args["mode"]="4";handleSortSave();assert(server.status==400);
  std::cout << "PASS: API category validation and legacy clients; backup v1/v2/v3 compatibility, version binding, all categories, favorites, duplicates, passwords with pipes, malformed input; categories, groups, edits, deletion, missing/invalid types, write failures;  sorting, favorites, stable IDs, edits, usage persistence, legacy records, deletion, full vault and write failures\n";
}
'''
with tempfile.TemporaryDirectory(prefix='passvault-sort-test-') as temp:
    cpp = Path(temp) / 'test.cpp'
    binary = Path(temp) / 'test'
    cpp.write_text(support + struct + '\nCredential creds[MAX_CREDS]; int credCount=0;\n' + settings + crud + backup + api + checks)
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('A C++17 compiler is required')
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
