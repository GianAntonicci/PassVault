"""Exercise production sync state machine with fake WiFi, clock and crypto.
These tests verify framing/lifecycle/auth-failure handling, not AES-GCM itself.
Run: python3 test/test_vault_sync_transport.py
"""
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
module = ast.parse((root / 'test/test_credential_sort.py').read_text())
support = next(ast.literal_eval(n.value) for n in module.body
               if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'support' for t in n.targets))
core = (root / 'include/vault_core.h').read_text()
struct = core[core.index('enum CredentialCategory'):core.index('// ---- Globals ----')]
data = (root / 'include/vault_sync_data.h').read_text().replace('#include "vault_core.h"', '')
transport = '\n'.join(line for line in (root / 'include/vault_sync.h').read_text().splitlines()
                      if not line.startswith('#include'))
fakes = r'''
#include <algorithm>
using std::min;
unsigned long now=100;
unsigned long millis() { return now; }
uint32_t esp_random() { static uint32_t x=1234; x=x*1664525+1013904223;return x; }
constexpr int WIFI_OFF=0,WIFI_AP=1,WIFI_STA=2,WIFI_SCAN_FAILED=-2,WIFI_SCAN_RUNNING=-1;
constexpr int WIFI_AUTH_WPA2_PSK=3,WL_CONNECTED=3;
struct FakeWifi {
  int modeValue=0,statusValue=0,scanResult=0;
  bool apOk=true,reconnect=true,persist=true;
  std::vector<String> networks;
  std::vector<int> security;
  void mode(int v) { modeValue=v; }
  void persistent(bool b) { persist=b; }
  void setAutoReconnect(bool b) { reconnect=b; }
  void scanDelete() {}
  void disconnect(bool,bool) { statusValue=0; }
  void softAPdisconnect(bool) {}
  bool softAP(const char*,const char*,int,int,int) { return apOk; }
  int scanNetworks(bool) { return WIFI_SCAN_RUNNING; }
  int scanComplete() { return scanResult; }
  String SSID(int i) { return networks[i]; }
  int encryptionType(int i) { return security[i]; }
  void begin(const char*,const char*) {}
  int status() { return statusValue; }
  int gatewayIP() { return 1; }
} WiFi;
std::vector<uint8_t> incoming,outgoing;
size_t inputOffset=0, readLimit=256;
bool link=false,offered=false,connectOk=true,writeOk=true;
struct WiFiClient {
  bool active=false;
  void stop() { active=false;link=false; }
  void flush() {}
  bool connected() { return active && link; }
  explicit operator bool() { return active; }
  int available() { return incoming.size()-inputOffset; }
  int read(uint8_t* p,size_t n) {
    n=min(n,min(readLimit,incoming.size()-inputOffset));
    memcpy(p,incoming.data()+inputOffset,n);inputOffset+=n;return n;
  }
  size_t write(const uint8_t* p,size_t n) {
    if (!writeOk) return 0;
    outgoing.insert(outgoing.end(),p,p+n);return n;
  }
  bool connect(int,int,int) { active=link=connectOk;return active; }
};
struct WiFiServer {
  WiFiServer(int) {}
  void end() {}
  void begin() {}
  WiFiClient accept() { WiFiClient c;c.active=offered;offered=false;return c; }
};
// Deterministic stand-ins that bind key, IV, challenge, version and body.
constexpr int MBEDTLS_MD_SHA256=0,MBEDTLS_CIPHER_ID_AES=0,MBEDTLS_GCM_ENCRYPT=1;
int mbedtls_pkcs5_pbkdf2_hmac_ext(int,const uint8_t* p,size_t n,const uint8_t*,size_t,int,size_t k,uint8_t* out) {
  for(size_t i=0;i<k;i++)out[i]=p[i%n];return 0;
}
const void* mbedtls_md_info_from_type(int) { return nullptr; }
int mbedtls_md_hmac(const void*, const uint8_t* key,size_t k,const uint8_t* p,size_t n,uint8_t* out) {
  uint8_t tmp[32]={};
  for(size_t i=0;i<k;i++)tmp[i%32]^=key[i];
  for(size_t i=0;i<n;i++)tmp[i%32]^=p[i];
  memcpy(out,tmp,32);return 0;
}
struct mbedtls_gcm_context { uint8_t key[32]; };
void mbedtls_gcm_init(mbedtls_gcm_context*) {}
void mbedtls_gcm_free(mbedtls_gcm_context*) {}
int mbedtls_gcm_setkey(mbedtls_gcm_context* c,int,const uint8_t* key,int) { memcpy(c->key,key,32);return 0; }
void fakeTag(mbedtls_gcm_context* c,const uint8_t* iv,const uint8_t* aad,size_t a,const uint8_t* body,size_t n,uint8_t* tag) {
  memset(tag,0,16);
  for(size_t i=0;i<32;i++)tag[i%16]^=c->key[i];
  for(size_t i=0;i<12;i++)tag[i%16]^=iv[i];
  for(size_t i=0;i<a;i++)tag[i%16]^=aad[i];
  for(size_t i=0;i<n;i++)tag[i%16]^=body[i];
}
int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context* c,int,size_t n,const uint8_t* iv,size_t,const uint8_t* aad,size_t a,const uint8_t* in,uint8_t* out,size_t,uint8_t* tag) {
  for(size_t i=0;i<n;i++)out[i]=in[i]^c->key[i%32];fakeTag(c,iv,aad,a,out,n,tag);return 0;
}
int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context* c,size_t n,const uint8_t* iv,size_t,const uint8_t* aad,size_t a,const uint8_t* tag,size_t,const uint8_t* in,uint8_t* out) {
  uint8_t actual[16];fakeTag(c,iv,aad,a,in,n,actual);
  if(memcmp(actual,tag,16))return -1;
  for(size_t i=0;i<n;i++)out[i]=in[i]^c->key[i%32];return 0;
}
'''
checks = r'''
void clearIo() { incoming.clear();outgoing.clear();inputOffset=0;readLimit=256;writeOk=true; }
void assertOff() {
  assert(WiFi.modeValue==WIFI_OFF && !syncPacket && !link);
  for(char c:syncCode)assert(c==0);
  for(uint8_t c:syncChallenge)assert(c==0);
}
int main() {
  syncStartSender();assert(strlen(syncCode)==12 && strlen(syncSsid)==21 && !WiFi.persist);
  std::string code=syncCode;
  // A TCP connection alone cannot release credentials.
  offered=link=true;syncTick();assert(syncPhase==SYNC_WAIT_CONFIRM && outgoing.empty());
  incoming.assign(16,42);readLimit=3;
  for(int i=0;i<6;i++)syncTick();
  assert(syncOffset==16 && syncPhase==SYNC_WAIT_CONFIRM && outgoing.empty());
  credCount=1;memset(creds,0,sizeof(creds));
  strcpy(creds[0].name,"mail");strcpy(creds[0].user,"me");strcpy(creds[0].pass,"p|secret");
  syncConfirmSend();assert(syncPhase==SYNC_SENDING);
  while(syncPhase==SYNC_SENDING)syncTick();
  assert(syncPhase==SYNC_ACK && outgoing.size()==SYNC_PACKET_SIZE && syncPacket);
  auto packet=outgoing;
  // A valid authenticated receipt is required before the source reports success.
  memset(syncAck,0,sizeof(syncAck));syncAck[1]=1;assert(syncReceipt(false));
  incoming.assign(syncAck,syncAck+sizeof(syncAck));inputOffset=0;
  while(syncPhase==SYNC_ACK)syncTick();
  assert(syncPhase==SYNC_FINISHED && syncAdded==1);assertOff();
  // Receive in short fragments, never persist before complete authentication.
  for(int variant=0;variant<5;variant++) {
    syncStop();clearIo();prefs=Preferences();credCount=0;
    syncStarted=syncProgress=now;strcpy(syncCode,code.c_str());
    assert(syncAllocate());memset(syncChallenge,42,16);
    syncClient.active=link=true;syncPhase=SYNC_RECEIVING;incoming=packet;readLimit=37;
    if(variant==1) incoming[SYNC_HEADER_SIZE+5]^=1;
    if(variant==2) syncChallenge[0]^=1;
    if(variant==3) incoming[7]++;
    if(variant==4) incoming.resize(120);
    int ticks=0;
    while((syncPhase==SYNC_RECEIVING || syncPhase==SYNC_REPLY_DRAIN) && ticks++<1000) {
      now++;syncTick();if(inputOffset<incoming.size())assert(credCount==0);
      if(variant==4 && inputOffset==incoming.size()) link=false;
    }
    assert(syncPhase==(variant==0?SYNC_FINISHED:SYNC_FAILED));assertOff();
    assert(credCount==(variant==0?1:0));
    if(!variant)assert(strcmp(creds[0].pass,"p|secret")==0);
  }
  // Lost/tampered receipts cannot claim success, even after a full TCP write.
  for(int variant=0;variant<3;variant++) {
    syncStop();clearIo();syncStarted=syncProgress=now;
    strcpy(syncCode,code.c_str());memset(syncChallenge,42,16);syncAllocate();
    memcpy(syncPacket,packet.data(),packet.size());
    syncClient.active=link=true;syncPhase=SYNC_ACK;
    syncAck[0]=0;syncAck[1]=1;syncAck[2]=0;syncReceipt(false);
    incoming.assign(syncAck,syncAck+sizeof(syncAck));
    if(variant==0)incoming[3]^=1;
    if(variant==1)incoming[1]^=1;
    if(variant==2) { incoming.clear();now+=SYNC_IO_MS; }
    while(syncPhase==SYNC_ACK)syncTick();
    assert(syncPhase==SYNC_FAILED);assertOff();
  }
  // Reject forged or missing delivery receipts without reporting success.
  for(int variant=0;variant<2;variant++) {
    syncStop();clearIo();syncStarted=syncProgress=now;
    strcpy(syncCode,code.c_str());assert(syncAllocate());
    memcpy(syncPacket,packet.data(),packet.size());memset(syncChallenge,42,16);
    syncClient.active=link=true;syncPhase=SYNC_ACK;
    if(!variant) {
      memset(syncAck,0,sizeof(syncAck));assert(syncReceipt(false));syncAck[3]^=1;
      incoming.assign(syncAck,syncAck+sizeof(syncAck));
    } else now+=SYNC_IO_MS;
    while(syncPhase==SYNC_ACK)syncTick();
    assert(syncPhase==SYNC_FAILED);assertOff();
  }
  syncStartSender();now+=SYNC_SESSION_MS;syncTick();assert(syncPhase==SYNC_FAILED);assertOff();
  syncStartSender();offered=link=true;syncTick();now+=SYNC_IO_MS;syncTick();
  assert(syncPhase==SYNC_FAILED);assertOff();
  // Only WPA2 PassVault sync networks, with duplicate SSIDs removed.
  syncStartScan();WiFi.networks={"home","PassVault-Sync-ABC123","PassVault-Sync-ABC123","PassVault-Sync-123ABC"};
  WiFi.security={3,3,3,0};WiFi.scanResult=4;syncTick();
  assert(syncPhase==SYNC_SELECT && syncNetworkCount==1 && !WiFi.reconnect);
  strcpy(syncCode,"123456789012");syncConnect();assert(syncPhase==SYNC_CONNECTING);
  now+=SYNC_IO_MS;syncTick();assert(syncPhase==SYNC_FAILED);assertOff();
  syncStartSender();syncStop();assert(syncPhase==SYNC_OFF);assertOff();
  WiFi.apOk=false;
  for(int language=0;language<LANGUAGE_COUNT;language++) {
    activeLanguage=language;
    syncStartSender();assert(syncPhase==SYNC_FAILED);assertOff();
    assert(strcmp(syncMessage,tr(TXT_SYNC_AP_FAILED))==0);
    WiFi.apOk=true;syncStartSender();now+=SYNC_SESSION_MS;syncTick();
    assert(syncPhase==SYNC_FAILED && strcmp(syncMessage,tr(TXT_SYNC_TIMEOUT))==0);
    WiFi.apOk=false;
  }
  std::cout << "Sync lifecycle, confirmation, framing, auth failures, replay challenge, truncation and cleanup passed\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / 'transport.cpp'
    src.write_text(support + '\n#include "localization.h"\n' + struct + '\nCredential creds[MAX_CREDS]; int credCount=0;\n' + data + fakes + transport + checks)
    exe = Path(tmp) / 'transport'
    subprocess.run(['c++', '-std=c++17', '-I', str(root / 'include'), str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
