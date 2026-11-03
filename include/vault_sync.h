#ifndef VAULT_SYNC_H
#define VAULT_SYNC_H

#include <WiFi.h>
#include <mbedtls/gcm.h>
#include "vault_sync_data.h"

// Independent TCP service: web management is never available during pairing.
constexpr uint16_t SYNC_PORT = 43821;
constexpr unsigned long SYNC_SESSION_MS = 120000;
constexpr unsigned long SYNC_IO_MS = 15000;
constexpr size_t SYNC_HEADER_SIZE = 52; // magic/version 8, salt 16, IV 12, GCM tag 16
constexpr size_t SYNC_PACKET_SIZE = SYNC_HEADER_SIZE + SYNC_DATA_SIZE;
const uint8_t SYNC_MAGIC[8] = {'P','V','S','Y','N','C',0,1};

enum SyncPhase { SYNC_OFF, SYNC_CHOOSE, SYNC_SCANNING, SYNC_SELECT, SYNC_CODE,
                 SYNC_AP_WAIT, SYNC_CONNECTING, SYNC_WAIT_CONFIRM,
                 SYNC_RECEIVING, SYNC_SENDING, SYNC_REPLY_DRAIN, SYNC_ACK, SYNC_FINISHED, SYNC_FAILED };
SyncPhase syncPhase = SYNC_OFF;
WiFiServer syncServer(SYNC_PORT);
WiFiClient syncClient;
char syncCode[13] = "";
char syncSsid[33] = "";
char syncNetworks[6][33] = {};
uint8_t syncNetworkCount = 0;
uint8_t syncNetworkIndex = 0;
uint8_t syncChallenge[16] = {};
uint8_t* syncPacket = nullptr;
uint8_t syncAck[35] = {}; // status, added, skipped, HMAC-SHA256
size_t syncOffset = 0;
unsigned long syncStarted = 0, syncProgress = 0;
int syncAdded = 0, syncSkipped = 0;
const char* syncMessage = "";

void syncWipePacket() {
  if (syncPacket) {
    mbedtls_platform_zeroize(syncPacket, SYNC_PACKET_SIZE);
    free(syncPacket);
    syncPacket = nullptr;
  }
}

void syncRadioOff() {
  syncClient.stop();
  syncServer.end();
  WiFi.scanDelete();
  WiFi.disconnect(false, false);
  WiFi.softAPdisconnect(false);
  WiFi.mode(WIFI_OFF);
  syncWipePacket();
  mbedtls_platform_zeroize(syncCode, sizeof(syncCode));
  mbedtls_platform_zeroize(syncChallenge, sizeof(syncChallenge));
}

void syncStop() {
  syncRadioOff();
  memset(syncNetworks, 0, sizeof(syncNetworks));
  memset(syncSsid, 0, sizeof(syncSsid));
  syncNetworkCount = 0;
  syncAdded = syncSkipped = 0;
  memset(syncAck, 0, sizeof(syncAck));
  syncPhase = SYNC_OFF;
}

void syncFinish(bool ok, const char* message) {
  syncRadioOff();
  syncMessage = message;
  syncPhase = ok ? SYNC_FINISHED : SYNC_FAILED;
}

bool syncAllocate() {
  syncWipePacket();
  syncPacket = (uint8_t*)calloc(1, SYNC_PACKET_SIZE);
  if (!syncPacket) { syncFinish(false, tr(TXT_SYNC_NO_MEMORY)); return false; }
  syncOffset = 0;
  return true;
}

// A fresh receiver challenge is authenticated as AAD, preventing old packet replay.
// Both AP authentication and the independent payload key use the temporary code.
bool syncCrypt(bool encrypt) {
  uint8_t key[32];
  int r = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
      (const uint8_t*)syncCode, 12, syncPacket + 8, 16, KDF_ITERS, sizeof(key), key);
  uint8_t aad[24];
  memcpy(aad, SYNC_MAGIC, 8);
  memcpy(aad + 8, syncChallenge, 16);
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  if (!r) r = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
  uint8_t* body = syncPacket + SYNC_HEADER_SIZE;
  // mbedTLS GCM decryption does not support identical input/output buffers.
  uint8_t* output = (uint8_t*)malloc(SYNC_DATA_SIZE);
  if (!output) r = -1;
  if (!r && encrypt)
    r = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, SYNC_DATA_SIZE,
        syncPacket + 24, 12, aad, sizeof(aad), body, output, 16, syncPacket + 36);
  else if (!r)
    r = mbedtls_gcm_auth_decrypt(&ctx, SYNC_DATA_SIZE, syncPacket + 24, 12,
        aad, sizeof(aad), syncPacket + 36, 16, body, output);
  if (!r) memcpy(body, output, SYNC_DATA_SIZE);
  if (output) { mbedtls_platform_zeroize(output, SYNC_DATA_SIZE); free(output); }
  mbedtls_gcm_free(&ctx);
  mbedtls_platform_zeroize(key, sizeof(key));
  return r == 0;
}

// Bind the delivery receipt to this challenge and encrypted packet.
// A TCP write alone never means credentials were saved.
bool syncReceipt(bool verify) {
  uint8_t key[32], mac[32], message[35];
  memcpy(message, syncChallenge, 16);
  memcpy(message + 16, syncPacket + 36, 16);
  memcpy(message + 32, syncAck, 3);
  int r = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
      (const uint8_t*)syncCode, 12, syncPacket + 8, 16, KDF_ITERS, 32, key);
  const uint8_t domain[] = "PassVault sync receipt v1";
  if (!r) r = mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
      key, sizeof(key), domain, sizeof(domain), mac);
  if (!r) r = mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
      mac, sizeof(mac), message, sizeof(message), key);
  uint8_t diff = 0;
  if (!r && verify) for (int i = 0; i < 32; i++) diff |= key[i] ^ syncAck[3 + i];
  if (!r && !verify) memcpy(syncAck + 3, key, 32);
  mbedtls_platform_zeroize(key, sizeof(key));
  mbedtls_platform_zeroize(mac, sizeof(mac));
  return !r && !diff;
}

void syncStartSender() {
  syncStop();
  syncStarted = syncProgress = millis();
  // Rejection sampling avoids modulo bias in the 12-digit (about 40-bit) code.
  for (int i = 0; i < 12; ) {
    uint8_t n = (uint8_t)esp_random();
    if (n < 250) syncCode[i++] = '0' + n % 10;
  }
  snprintf(syncSsid, sizeof(syncSsid), "PassVault-Sync-%06lX", (unsigned long)(esp_random() & 0xffffff));
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(syncSsid, syncCode, 1, 0, 1)) {
    syncFinish(false, tr(TXT_SYNC_AP_FAILED)); return;
  }
  syncServer.begin();
  syncPhase = SYNC_AP_WAIT;
}

void syncStartScan() {
  syncStop();
  syncStarted = syncProgress = millis();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  int r = WiFi.scanNetworks(true);
  if (r == WIFI_SCAN_FAILED) { syncFinish(false, tr(TXT_SYNC_SCAN_FAILED)); return; }
  syncPhase = SYNC_SCANNING;
}

void syncConnect() {
  if (strlen(syncCode) != 12) return;
  if (!syncAllocate()) return;
  esp_fill_random(syncChallenge, sizeof(syncChallenge));
  WiFi.begin(syncSsid, syncCode);
  syncProgress = millis();
  syncPhase = SYNC_CONNECTING;
}

void syncConfirmSend() {
  if (syncPhase != SYNC_WAIT_CONFIRM || syncOffset != sizeof(syncChallenge)) return;
  if (!syncClient.connected()) { syncFinish(false, tr(TXT_SYNC_DESTINATION_LOST)); return; }
  if (!syncAllocate()) return;
  memcpy(syncPacket, SYNC_MAGIC, 8);
  esp_fill_random(syncPacket + 8, 28); // salt and IV
  if (!syncEncode(syncPacket + SYNC_HEADER_SIZE) || !syncValidData(syncPacket + SYNC_HEADER_SIZE, SYNC_DATA_SIZE) || !syncCrypt(true)) {
    syncFinish(false, tr(TXT_SYNC_ENCRYPT_FAILED)); return;
  }
  syncProgress = millis();
  syncPhase = SYNC_SENDING;
}

void syncTick() {
  if (syncPhase == SYNC_OFF || syncPhase == SYNC_CHOOSE ||
      syncPhase == SYNC_FINISHED || syncPhase == SYNC_FAILED) return;
  if (millis() - syncStarted >= SYNC_SESSION_MS) {
    syncFinish(false, tr(TXT_SYNC_TIMEOUT)); return;
  }
  if (syncPhase == SYNC_SCANNING) {
    int count = WiFi.scanComplete();
    if (count == WIFI_SCAN_RUNNING) return;
    if (count < 0) { syncFinish(false, tr(TXT_SYNC_SCAN_FAILED)); return; }
    for (int i = 0; i < count && syncNetworkCount < 6; i++) {
      String ssid = WiFi.SSID(i);
      if (!ssid.startsWith("PassVault-Sync-") || ssid.length() != 21 ||
          WiFi.encryptionType(i) != WIFI_AUTH_WPA2_PSK) continue;
      bool duplicate = false;
      for (int j = 0; j < syncNetworkCount; j++) duplicate |= ssid == syncNetworks[j];
      if (!duplicate) strlcpy(syncNetworks[syncNetworkCount++], ssid.c_str(), 33);
    }
    WiFi.scanDelete();
    if (!syncNetworkCount) { syncFinish(false, tr(TXT_SYNC_NONE_FOUND)); return; }
    syncNetworkIndex = 0;
    strlcpy(syncSsid, syncNetworks[0], sizeof(syncSsid));
    syncPhase = SYNC_SELECT;
  } else if (syncPhase == SYNC_AP_WAIT) {
    syncClient = syncServer.accept();
    if (!syncClient) return;
    syncOffset = 0;
    syncProgress = millis();
    syncPhase = SYNC_WAIT_CONFIRM;
  } else if (syncPhase == SYNC_WAIT_CONFIRM) {
    // Exactly one connection per session; a failed attempt requires a fresh code.
    if (syncOffset < sizeof(syncChallenge) && syncClient.available()) {
      int n = syncClient.read(syncChallenge + syncOffset, sizeof(syncChallenge) - syncOffset);
      if (n > 0) { syncOffset += n; syncProgress = millis(); }
    }
    if (!syncClient.connected() || millis() - syncProgress >= (syncOffset == sizeof(syncChallenge) ? 60000UL : SYNC_IO_MS))
      syncFinish(false, tr(TXT_SYNC_DESTINATION_SILENT));
  } else if (syncPhase == SYNC_CONNECTING) {
    if (millis() - syncProgress >= SYNC_IO_MS) { syncFinish(false, tr(TXT_SYNC_CHECK_CODE)); return; }
    if (WiFi.status() != WL_CONNECTED) return;
    if (!syncClient.connect(WiFi.gatewayIP(), SYNC_PORT, 1000) ||
        syncClient.write(syncChallenge, sizeof(syncChallenge)) != sizeof(syncChallenge)) {
      syncFinish(false, tr(TXT_SYNC_CONNECTION_FAILED)); return;
    }
    syncProgress = millis();
    syncPhase = SYNC_RECEIVING;
  } else if (syncPhase == SYNC_RECEIVING) {
    if (syncClient.available()) {
      size_t remaining = SYNC_PACKET_SIZE - syncOffset;
      int n = syncClient.read(syncPacket + syncOffset, min(remaining, size_t(256)));
      if (n > 0) { syncOffset += n; syncProgress = millis(); }
    }
    if (syncOffset == SYNC_PACKET_SIZE) {
      if (memcmp(syncPacket, SYNC_MAGIC, 8) || !syncCrypt(false)) {
        syncFinish(false, tr(TXT_SYNC_AUTH_FAILED)); return;
      }
      int r = syncMerge(syncPacket + SYNC_HEADER_SIZE, SYNC_DATA_SIZE, syncAdded, syncSkipped);
      syncAck[0] = r == 0 ? 0 : r == -2 ? 2 : r == -3 ? 3 : 1;
      syncAck[1] = syncAdded; syncAck[2] = syncSkipped;
      if (!syncReceipt(false) || syncClient.write(syncAck, sizeof(syncAck)) != sizeof(syncAck)) {
        syncFinish(false, r == 0 ? tr(TXT_SYNC_SAVED_REPLY_FAILED) : tr(TXT_SYNC_FAILED_REPLY_LOST)); return;
      }
      // Leave the radio alive so the receipt can reach the source before shutdown.
      syncMessage = r == 0 ? tr(TXT_SYNC_RECEIVED) : r == -2 ? tr(TXT_SYNC_NO_SPACE) :
          r == -3 ? tr(TXT_SAVE_FAILED) : tr(TXT_SYNC_INVALID_DATA);
      syncWipePacket();
      syncProgress = millis();
      syncPhase = SYNC_REPLY_DRAIN;
    } else if ((!syncClient.connected() && !syncClient.available()) || millis() - syncProgress >= 60000)
      syncFinish(false, tr(TXT_SYNC_INCOMPLETE));
  } else if (syncPhase == SYNC_REPLY_DRAIN) {
    if (millis() - syncProgress >= 500) syncFinish(syncAck[0] == 0, syncMessage);
  } else if (syncPhase == SYNC_SENDING) {
    size_t n = min(SYNC_PACKET_SIZE - syncOffset, size_t(256));
    if (!syncClient.connected() || syncClient.write(syncPacket + syncOffset, n) != n) {
      syncFinish(false, tr(TXT_SYNC_SEND_FAILED)); return;
    }
    syncOffset += n;
    syncProgress = millis();
    if (syncOffset == SYNC_PACKET_SIZE) {
      syncOffset = 0;
      syncPhase = SYNC_ACK;
    }
  } else if (syncPhase == SYNC_ACK) {
    if (syncClient.available()) {
      int n = syncClient.read(syncAck + syncOffset, sizeof(syncAck) - syncOffset);
      if (n > 0) { syncOffset += n; syncProgress = millis(); }
    }
    if (syncOffset == sizeof(syncAck)) {
      if (!syncReceipt(true) || syncAck[0] > 3 || syncAck[1] > MAX_CREDS || syncAck[2] > MAX_CREDS) {
        syncFinish(false, tr(TXT_SYNC_INVALID_REPLY)); return;
      }
      syncAdded = syncAck[1]; syncSkipped = syncAck[2];
      syncFinish(syncAck[0] == 0, syncAck[0] == 0 ? tr(TXT_SYNC_DESTINATION_CONFIRMED) :
          syncAck[0] == 2 ? tr(TXT_SYNC_DESTINATION_FULL) : tr(TXT_SYNC_DESTINATION_FAILED));
    } else if ((!syncClient.connected() && !syncClient.available()) || millis() - syncProgress >= SYNC_IO_MS) {
      syncFinish(false, tr(TXT_SYNC_UNCONFIRMED));
    }
  }
}

#endif
