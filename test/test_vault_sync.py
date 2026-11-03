"""Host regression tests for production sync wire validation and NVS merge.
Run: python3 test/test_vault_sync.py. Crypto is mocked here; firmware uses mbedTLS.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
# Reuse the existing Arduino/NVS host doubles without executing the test module.
import ast
module = ast.parse((root / 'test/test_credential_sort.py').read_text())
support = next(ast.literal_eval(n.value) for n in module.body
               if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'support' for t in n.targets))
support = support.replace('bool fail = false;', 'bool fail = false; int failAt = -1, writes = 0;')
support = support.replace('if(fail)return 0;', 'if(fail || writes++ == failAt)return 0;')
support = support.replace('text[k]=s;return s.size();', 'if(fail || writes++ == failAt)return 0;text[k]=s;return s.size();')
core = (root / 'include/vault_core.h').read_text()
struct = core[core.index('enum CredentialCategory'):core.index('// ---- Globals ----')]
production = (root / 'include/vault_sync_data.h').read_text().replace('#include "vault_core.h"', '')
checks = r'''
uint8_t data[SYNC_DATA_SIZE];
void reset() { prefs = Preferences(); credCount=0; memset(creds,0,sizeof(creds)); }
void record(int i, const char* name, const char* user="user", const char* pass="secret") {
  uint8_t* p=data+1+i*SYNC_ENTRY_SIZE;
  memset(p,0,SYNC_ENTRY_SIZE);
  strcpy((char*)p,name);strcpy((char*)p+33,user);strcpy((char*)p+98,pass);
  p[163]=1;p[164]=CATEGORY_WORK;
}
int main() {
  int added,skipped;
  credCount=-1;assert(!syncEncode(data));
  credCount=MAX_CREDS+1;assert(!syncEncode(data));
  reset(); memset(data,0,sizeof(data)); data[0]=2;record(0,"mail");record(1,"bank");
  assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==2 && skipped==0);
  assert(credCount==2 && creds[0].favorite && creds[0].category==CATEGORY_WORK);
  assert(prefs.getInt("cnt")==2 && prefs.getString("c0")=="mail|user|secret");
  strcpy(creds[0].pass,"local secret");creds[0].useCount=7;
  assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==0 && skipped==2);
  assert(strcmp(creds[0].pass,"local secret")==0 && creds[0].useCount==7);
  syncEncode(data);assert(syncValidData(data,sizeof(data)));
  assert(strcmp((char*)data+1+98,"local secret")==0);
  reset();data[0]=2;record(0,"same");record(1,"same");
  assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==1 && skipped==1);
  reset();data[0]=2;record(0,"same","one");record(1,"same","two");
  assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==2);
  reset();data[0]=2;record(0,"valid");record(1,"invalid|name");
  assert(syncMerge(data,sizeof(data),added,skipped)==-1 && credCount==0);
  record(1,"valid2");data[1+SYNC_ENTRY_SIZE+164]=CATEGORY_COUNT;
  assert(!syncValidData(data,sizeof(data)));
  record(1,"valid2");memset(data+1+SYNC_ENTRY_SIZE+98,'x',65);
  assert(!syncValidData(data,sizeof(data)));
  assert(!syncValidData(data,sizeof(data)-1));data[0]=MAX_CREDS+1;
  assert(!syncValidData(data,sizeof(data)));
  reset();data[0]=2;record(0,"new");record(1,"new2");credCount=MAX_CREDS-1;
  for(int i=0;i<credCount;i++)strcpy(creds[i].name,"existing");
  assert(syncMerge(data,sizeof(data),added,skipped)==-2 && prefs.writes==0);
  // Every write failure leaves count/RAM unchanged for the incomplete record.
  for(int step=0;step<5;step++) {
    reset();data[0]=1;record(0,"mail");prefs.failAt=step;
    assert(syncMerge(data,sizeof(data),added,skipped)==-3 && credCount==0 && added==0);
    assert(prefs.getInt("cnt")==0);
    prefs.failAt=-1;
    assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==1 && credCount==1);
  }
  reset();data[0]=2;record(0,"one");record(1,"two");prefs.failAt=5;
  assert(syncMerge(data,sizeof(data),added,skipped)==-3 && added==1 && credCount==1);
  prefs.failAt=-1;
  assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==1 && skipped==1);
  // Full vault and maximum-length fields, including a pipe in the password.
  reset();memset(data,0,sizeof(data));data[0]=MAX_CREDS;
  for(int i=0;i<MAX_CREDS;i++) {
    uint8_t* p=data+1+i*SYNC_ENTRY_SIZE;
    memset(p,'n',32);p[0]='A'+i;
    memset(p+33,'u',64);memset(p+98,'p',64);p[98+63]='|';
    p[163]=i%2;p[164]=i%CATEGORY_COUNT;
  }
  assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==MAX_CREDS);
  assert(strlen(creds[0].name)==32 && strlen(creds[0].user)==64 && strlen(creds[0].pass)==64);
  assert(creds[0].pass[63]=='|' && creds[MAX_CREDS-1].useCount==0);
  syncEncode(data);assert(syncValidData(data,sizeof(data)));
  assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==0 && skipped==MAX_CREDS);
  reset();memset(data,0,sizeof(data));
  assert(syncMerge(data,sizeof(data),added,skipped)==0 && added==0);
  std::cout << "Sync validation, merge, capacity, persistence and retries passed\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / 'sync.cpp'
    src.write_text(support + struct + '\nCredential creds[MAX_CREDS]; int credCount=0;\n' + production + checks)
    exe = Path(tmp) / 'sync'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
