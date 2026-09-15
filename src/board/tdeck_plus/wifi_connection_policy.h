#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace TpWifi {
constexpr uint32_t CONNECT_TIMEOUT_MS=20000;
constexpr int MAX_SAVED_NETWORKS=4,SSID_CAPACITY=33,PASSWORD_CAPACITY=65;
enum Phase:uint8_t { IDLE=0,CONNECTING,ASSOCIATED,GOT_IP,FAILED,TIMED_OUT,CANCELED };

struct ConnectionPolicy {
  Phase phase=IDLE; uint32_t beganAt=0; uint8_t reason=0;
  void begin(uint32_t now) { phase=CONNECTING; beganAt=now; reason=0; }
  void connected() { if (phase==CONNECTING) phase=ASSOCIATED; }
  void gotIp() { if (phase==CONNECTING||phase==ASSOCIATED) phase=GOT_IP; }
  void disconnected(uint8_t why,bool terminal,bool local) {
    reason=why; if (local) { if (active()) phase=CANCELED; }
    else if (active()&&terminal) phase=FAILED;
  }
  void poll(uint32_t now) { if (active()&&now-beganAt>=CONNECT_TIMEOUT_MS) phase=TIMED_OUT; }
  bool active() const { return phase==CONNECTING||phase==ASSOCIATED; }
  bool success() const { return phase==GOT_IP; }
  bool failed() const { return phase==FAILED||phase==TIMED_OUT||phase==CANCELED; }
  bool cleanupAllowed() const { return success()||failed(); }
  bool credentialsProven() const { return phase==GOT_IP; }
};

inline int findCredential(const char ssids[MAX_SAVED_NETWORKS][SSID_CAPACITY],uint8_t count,const char *ssid) {
  if (!ssid) return -1;
  for (int i=0;i<count&&i<MAX_SAVED_NETWORKS;++i) if (!strcmp(ssids[i],ssid)) return i;
  return -1;
}

inline int upsertCredential(char ssids[MAX_SAVED_NETWORKS][SSID_CAPACITY],
                            char passwords[MAX_SAVED_NETWORKS][PASSWORD_CAPACITY],
                            uint8_t &count,const char *ssid,const char *password) {
  if (!ssid||!ssid[0]) return -1;
  int slot=findCredential(ssids,count,ssid);
  if (slot<0) {
    if (count<MAX_SAVED_NETWORKS) slot=count++;
    else {
      for (int i=1;i<MAX_SAVED_NETWORKS;++i) {
        memcpy(ssids[i-1],ssids[i],SSID_CAPACITY);
        memcpy(passwords[i-1],passwords[i],PASSWORD_CAPACITY);
      }
      slot=MAX_SAVED_NETWORKS-1;
    }
  }
  snprintf(ssids[slot],SSID_CAPACITY,"%s",ssid);
  snprintf(passwords[slot],PASSWORD_CAPACITY,"%s",password?password:"");
  return slot;
}

inline bool removeCredential(char ssids[MAX_SAVED_NETWORKS][SSID_CAPACITY],
                             char passwords[MAX_SAVED_NETWORKS][PASSWORD_CAPACITY],
                             uint8_t &count,uint8_t &current) {
  if (!count||current>=count) return false;
  for (int i=current+1;i<count;++i) {
    memcpy(ssids[i-1],ssids[i],SSID_CAPACITY);
    memcpy(passwords[i-1],passwords[i],PASSWORD_CAPACITY);
  }
  --count; memset(ssids[count],0,SSID_CAPACITY); memset(passwords[count],0,PASSWORD_CAPACITY);
  current=count?(current<count?current:count-1):0; return true;
}

inline bool appendCredentialKey(char *buffer,size_t capacity,uint8_t key,size_t &length) {
  if (!buffer||capacity<1) return false;
  if (key=='\r'||key=='\n'||key==27) return false;
  if (key==8||key==127) { if (length) buffer[--length]=0; return true; }
  if (key<32||key>126||length>=capacity-1) return false;
  buffer[length++]=(char)key; buffer[length]=0; return true;
}
}
