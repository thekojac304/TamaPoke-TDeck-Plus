#include "../board_select.h"
#if TAMAPOKE_TDECK_PLUS
#include "../../../rtcbat.h"
#include "pins.h"
#include "time_sync_policy.h"
#include "timezone_policy.h"
#include "wifi_connection_policy.h"
#include "battery_policy.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <sys/time.h>
#include <time.h>

// GPS/NTP supply UTC. tp-clock pairs it with the game's local-wall-shaped
// epoch. NTP credentials are private to TamaPoke and are never printed.
constexpr uint32_t MIN_VALID_EPOCH=1704067200UL,MAX_VALID_EPOCH=4102444799UL;
constexpr uint32_t GPS_GRACE_MS=12000,NTP_WAIT_MS=15000;
constexpr uint32_t MANUAL_GPS_MS=15000,PERSIST_MS=5UL*60*1000;
constexpr uint32_t GPS_PRIMARY_BAUD=38400,GPS_FALLBACK_BAUD=9600,GPS_PROBE_MS=2500,GPS_STARTUP_MS=250;
constexpr int MAX_SCAN_RESULTS=6;
enum WifiAction:uint8_t { WIFI_NONE=0,WIFI_AUTO_NTP,WIFI_MANUAL_NTP,WIFI_CONNECT_SAVED,WIFI_CONNECT_CANDIDATE };
enum WifiCommand:uint8_t { WIFI_CMD_NONE=0,WIFI_CMD_ENABLE,WIFI_CMD_DISABLE,WIFI_CMD_SCAN,WIFI_CMD_CONNECT_SAVED,WIFI_CMD_CONNECT_SELECTED,WIFI_CMD_CONNECT_CANDIDATE,WIFI_CMD_FORGET,WIFI_CMD_SYNC };

static bool clockValid=false,mapped=false,trusted=false,authoritative=false,lastSyncCatchupAllowed=false;
static bool gpsSeen=false,autoStarted=false,autoFailed=false,scanning=false,wifiBeginPending=false;
static bool wifiOn=false,wifiConfigured=false;
static uint32_t savedWall=0,savedUtc=0,sourceUtc=0,sourceAtMs=0,lastPersistMs=0;
static uint32_t bootAtMs=0,wifiStartedAt=0,wifiBeginAt=0,ntpStartedAt=0,wallBeforeNtp=0,gpsManualUntil=0;
static int32_t wallOffset=0;
static TpTimezone::Preset timezonePreset=TpTimezone::UTC;
static volatile bool ntpSynced=false;
static RtcTimeSource timeSource=RTC_TIME_LOCAL;
static RtcSyncState syncState=RTC_SYNC_GPS_SEARCHING;
static WifiAction wifiAction=WIFI_NONE;
static WifiAction scanConnectAction=WIFI_NONE;
static WifiCommand wifiCommand=WIFI_CMD_NONE;
static TpTime::SyncPolicy policy;
static TpTime::ManualCatchupPolicy manualCatchup;
static TpWifi::ConnectionPolicy wifiConnection;
static char knownSsid[TpWifi::MAX_SAVED_NETWORKS][TpWifi::SSID_CAPACITY]={{0}};
static char knownPass[TpWifi::MAX_SAVED_NETWORKS][TpWifi::PASSWORD_CAPACITY]={{0}};
static uint8_t knownCount=0,currentKnown=0;
static char connectSsid[33]={0},connectPass[65]={0},candidateSsid[33]={0},candidatePass[65]={0};
static char feedback[28]={0}; static uint32_t feedbackUntil=0;
static char scanSsid[MAX_SCAN_RESULTS][33]={{0}}; static int scanRssi[MAX_SCAN_RESULTS]={0},scanCount=0;
static int lastWifiStatus=-999; static volatile uint8_t lastDisconnectReason=0;
static volatile bool eventConnected=false,eventGotIp=false,eventDisconnected=false,eventTerminal=false;
static volatile bool tamaDisconnecting=false;
static HardwareSerial gps(1);
static char nmea[112]; static uint8_t nmeaLen=0;
enum NmeaType:uint8_t { NMEA_OTHER=0,NMEA_RMC,NMEA_ZDA,NMEA_GGA,NMEA_GLL,NMEA_GSA,NMEA_GSV,NMEA_VTG,NMEA_TXT };
static uint32_t gpsBaud=GPS_PRIMARY_BAUD,gpsBaudAt=0,gpsReadyAt=0,gpsDiagAt=0,gpsBytesWindow=0,gpsBytesTotal=0;
static uint32_t gpsValidChecksum=0,gpsValidRmc=0,gpsValidZda=0,gpsLastParsedUtc=0;
static uint16_t gpsTypes=0; static uint8_t gpsProbe=0,gpsUbxState=0; static bool gpsUbxSeen=false;
static constexpr uint32_t WIFI_BREADCRUMB_MAGIC=0x54505749UL;
RTC_NOINIT_ATTR static uint32_t wifiBreadcrumbMagic;
RTC_NOINIT_ATTR static uint32_t wifiBreadcrumbAction;

static const char *wifiCommandName(WifiCommand command) {
  switch (command) {
    case WIFI_CMD_ENABLE:return "enable"; case WIFI_CMD_DISABLE:return "disable";
    case WIFI_CMD_SCAN:return "scan"; case WIFI_CMD_CONNECT_SAVED:return "connect-saved";
    case WIFI_CMD_CONNECT_SELECTED:return "connect-selected";
    case WIFI_CMD_CONNECT_CANDIDATE:return "connect-candidate"; case WIFI_CMD_FORGET:return "forget";
    case WIFI_CMD_SYNC:return "ntp-sync"; default:return "none";
  }
}
static const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:return "power-on";
    case ESP_RST_EXT:return "external";
    case ESP_RST_SW:return "software";
    case ESP_RST_PANIC:return "panic";
    case ESP_RST_INT_WDT:return "interrupt-watchdog";
    case ESP_RST_TASK_WDT:return "task-watchdog";
    case ESP_RST_WDT:return "watchdog";
    case ESP_RST_DEEPSLEEP:return "deep-sleep";
    case ESP_RST_BROWNOUT:return "brownout";
    case ESP_RST_SDIO:return "sdio";
    default:return "unknown";
  }
}
static void wifiDiag(const char *phase,const char *action) {
  Serial.printf("WIFI action=%s phase=%s heap=%lu min=%lu largest_internal=%u state=%d status=%d\n",
                action,phase,(unsigned long)ESP.getFreeHeap(),(unsigned long)ESP.getMinFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),(int)WiFi.getMode(),(int)WiFi.status());
}
static void markWifiAction(const char *action,WifiCommand command) {
  wifiBreadcrumbMagic=WIFI_BREADCRUMB_MAGIC; wifiBreadcrumbAction=(uint32_t)command;
  wifiDiag("begin",action);
}
static void clearWifiAction(const char *action) {
  wifiDiag("complete",action); wifiBreadcrumbAction=WIFI_CMD_NONE;
}

static bool validEpoch(uint32_t e) { return e>=MIN_VALID_EPOCH&&e<=MAX_VALID_EPOCH; }
static int64_t daysFromCivil(int y,unsigned m,unsigned d);
static void setFeedback(const char *text) { snprintf(feedback,sizeof(feedback),"%s",text?text:""); feedbackUntil=millis()+3500; }
static void setSystem(uint32_t e) { timeval tv{(time_t)e,0}; clockValid=settimeofday(&tv,nullptr)==0; }
static uint32_t currentUtc() { return sourceUtc?sourceUtc+(millis()-sourceAtMs)/1000:0; }
static uint32_t wallFromUtc(uint32_t utc) {
  time_t value=(time_t)utc; struct tm local{}; localtime_r(&value,&local);
  int64_t wall=daysFromCivil(local.tm_year+1900,(unsigned)local.tm_mon+1,(unsigned)local.tm_mday)*86400+
               local.tm_hour*3600+local.tm_min*60+local.tm_sec;
  return wall>=MIN_VALID_EPOCH&&wall<=MAX_VALID_EPOCH?(uint32_t)wall:0;
}
static uint32_t utcFromWall(uint32_t wall) {
  time_t value=(time_t)wall; struct tm local{}; gmtime_r(&value,&local); local.tm_isdst=-1;
  time_t utc=mktime(&local); return utc>=MIN_VALID_EPOCH&&utc<=MAX_VALID_EPOCH?(uint32_t)utc:0;
}
static void applyTimezone() {
  setenv("TZ",TpTimezone::posix(timezonePreset),1); tzset();
  Serial.printf("CLOCK timezone=%s posix=%s\n",TpTimezone::label(timezonePreset),TpTimezone::posix(timezonePreset));
}
static void persistNow(uint32_t wall,uint32_t utc) {
  if (!validEpoch(wall)) return;
  savedWall=wall;
  Preferences p; p.begin("tp-clock",false); p.putUInt("wall",wall); p.putBool("trusted",trusted);
  if (mapped&&validEpoch(utc)) {
    wallOffset=(int32_t)((int64_t)wall-utc);
    savedUtc=utc; p.putUInt("utc",utc); p.putInt("offset",wallOffset); p.putBool("mapped",true);
  } else {
    p.putBool("mapped",false); p.remove("utc"); p.remove("offset");
  }
  p.end();
  lastPersistMs=millis();
}
static void radioOff(const char *reason) {
  // disconnect(true) already deinitializes Wi-Fi. The old code immediately
  // called mode(WIFI_OFF) as well and, for scans, reinitialized it in the same
  // UI callback. That double teardown/restart races the Arduino event task.
  // Keep teardown single-shot and perform every transition from rtcPoll.
  tamaDisconnecting=true;
  esp_sntp_stop();
  if (WiFi.getMode()!=WIFI_OFF) { WiFi.disconnect(false,false); WiFi.mode(WIFI_OFF); }
  wifiStartedAt=wifiBeginAt=ntpStartedAt=0; ntpSynced=false; wifiAction=WIFI_NONE; wifiBeginPending=false;
  Serial.printf("WIFI cleanup=local radio=off reason=%s terminal=%d\n",reason?reason:"idle",wifiConnection.cleanupAllowed()?1:0);
}
static int64_t daysFromCivil(int y,unsigned m,unsigned d) {
  y-=m<=2; const int era=(y>=0?y:y-399)/400; const unsigned yoe=(unsigned)(y-era*400);
  const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
  return era*146097+(int)(yoe*365+yoe/4-yoe/100+doy)-719468;
}
static bool makeUtc(int y,int mon,int day,int hh,int mm,int ss,uint32_t &out) {
  static const uint8_t mdays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  if (y<2024||y>2099||mon<1||mon>12||day<1||hh>23||mm>59||ss>60) return false;
  int limit=mdays[mon-1]+(mon==2&&((y%4==0&&y%100!=0)||y%400==0));
  if (day>limit) return false;
  int64_t e=daysFromCivil(y,(unsigned)mon,(unsigned)day)*86400+hh*3600+mm*60+(ss==60?59:ss);
  if (e<MIN_VALID_EPOCH||e>MAX_VALID_EPOCH) return false;
  out=(uint32_t)e; return true;
}
static bool parseTime(const char *s,int &hh,int &mm,int &ss) {
  if (!s||strlen(s)<6) return false;
  for (int i=0;i<6;++i) if (s[i]<'0'||s[i]>'9') return false;
  hh=(s[0]-'0')*10+s[1]-'0'; mm=(s[2]-'0')*10+s[3]-'0'; ss=(s[4]-'0')*10+s[5]-'0';
  return hh<24&&mm<60&&ss<=60;
}
static int hexNibble(char c) {
  if (c>='0'&&c<='9') return c-'0'; if (c>='A'&&c<='F') return c-'A'+10;
  if (c>='a'&&c<='f') return c-'a'+10; return -1;
}
static NmeaType sentenceType(const char *token) {
  size_t len=strlen(token); if (len<4) return NMEA_OTHER; const char *kind=token+len-3;
  if (!strcmp(kind,"RMC")) return NMEA_RMC; if (!strcmp(kind,"ZDA")) return NMEA_ZDA;
  if (!strcmp(kind,"GGA")) return NMEA_GGA; if (!strcmp(kind,"GLL")) return NMEA_GLL;
  if (!strcmp(kind,"GSA")) return NMEA_GSA; if (!strcmp(kind,"GSV")) return NMEA_GSV;
  if (!strcmp(kind,"VTG")) return NMEA_VTG; if (!strcmp(kind,"TXT")) return NMEA_TXT;
  return NMEA_OTHER;
}
static bool parseNmea(char *line,uint32_t &utc) {
  char *star=strchr(line,'*'); if (line[0]!='$'||!star||strlen(star)<3) return false;
  uint8_t sum=0; for (char *p=line+1;p<star;++p) sum^=(uint8_t)*p;
  int hi=hexNibble(star[1]),lo=hexNibble(star[2]);
  if (hi<0||lo<0||sum!=(uint8_t)((hi<<4)|lo)) return false;
  ++gpsValidChecksum;
  *star=0; char *fields[16]; int count=0; fields[count++]=line;
  for (char *p=line;*p&&count<16;++p) if (*p==',') { *p=0; fields[count++]=p+1; }
  NmeaType type=sentenceType(fields[0]); gpsTypes|=(uint16_t)(1u<<type);
  int hh,mm,ss;
  if (type==NMEA_RMC&&count>=10) {
    if (!parseTime(fields[1],hh,mm,ss)||strlen(fields[9])<6) return false;
    for (int i=0;i<6;++i) if (fields[9][i]<'0'||fields[9][i]>'9') return false;
    int day=(fields[9][0]-'0')*10+fields[9][1]-'0',mon=(fields[9][2]-'0')*10+fields[9][3]-'0';
    int year=2000+(fields[9][4]-'0')*10+fields[9][5]-'0';
    bool ok=makeUtc(year,mon,day,hh,mm,ss,utc); if (ok) { ++gpsValidRmc; gpsLastParsedUtc=utc; } return ok;
  }
  if (type==NMEA_ZDA&&count>=5) {
    if (!parseTime(fields[1],hh,mm,ss)) return false;
    bool ok=makeUtc(atoi(fields[4]),atoi(fields[3]),atoi(fields[2]),hh,mm,ss,utc); if (ok) { ++gpsValidZda; gpsLastParsedUtc=utc; } return ok;
  }
  return false;
}
static void startGps(uint32_t baud,const char *reason) {
  gps.end(); gps.begin(baud,SERIAL_8N1,TDeckPins::gpsRx,TDeckPins::gpsTx);
  gps.write((uint8_t)0xFF); // wake edge used by working M10 integrations after backup mode
  gpsBaud=baud; gpsBaudAt=millis(); gpsReadyAt=gpsBaudAt+GPS_STARTUP_MS; gpsDiagAt=gpsBaudAt; gpsBytesWindow=0; nmeaLen=0;
  Serial.printf("GPS UART initialized uart=1 rx=%d tx=%d baud=%lu wake=FF reason=%s startup_ms=%lu\n",
                TDeckPins::gpsRx,TDeckPins::gpsTx,(unsigned long)baud,reason,(unsigned long)GPS_STARTUP_MS);
}
static void gpsDiagnostics(uint32_t now) {
  if (gpsDiagAt&&now-gpsDiagAt<1000) return;
  uint32_t interval=gpsDiagAt?now-gpsDiagAt:1000,bps=interval?gpsBytesWindow*1000/interval:gpsBytesWindow;
  char types[44]={0}; const char *names[]={"OTHER","RMC","ZDA","GGA","GLL","GSA","GSV","VTG","TXT"};
  for (unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i) if (gpsTypes&(1u<<i)) {
    if (types[0]) strncat(types,"/",sizeof(types)-strlen(types)-1); strncat(types,names[i],sizeof(types)-strlen(types)-1);
  }
  Serial.printf("GPS diag baud=%lu bytes_s=%lu bytes_total=%lu types=%s checksum_ok=%lu rmc_ok=%lu zda_ok=%lu parsed_utc=%lu ubx=%d\n",
                (unsigned long)gpsBaud,(unsigned long)bps,(unsigned long)gpsBytesTotal,types[0]?types:"none",
                (unsigned long)gpsValidChecksum,(unsigned long)gpsValidRmc,(unsigned long)gpsValidZda,
                (unsigned long)gpsLastParsedUtc,gpsUbxSeen?1:0);
  gpsBytesWindow=0; gpsTypes=0; gpsDiagAt=now;
}
static bool pollGps(uint32_t &utc) {
  uint32_t now=millis(); if ((int32_t)(now-gpsReadyAt)<0) return false;
  bool parsed=false;
  while (gps.available()) {
    uint8_t raw=(uint8_t)gps.read(); char c=(char)raw; gpsSeen=true; ++gpsBytesWindow; ++gpsBytesTotal;
    if (gpsUbxState==0) gpsUbxState=raw==0xB5?1:0;
    else { if (raw==0x62) gpsUbxSeen=true; gpsUbxState=0; }
    if (c=='\n') { nmea[nmeaLen]=0; if (parseNmea(nmea,utc)) parsed=true; nmeaLen=0; }
    else if (c!='\r') { if (nmeaLen<sizeof(nmea)-1) nmea[nmeaLen++]=c; else nmeaLen=0; }
  }
  gpsDiagnostics(now);
  if (!gpsValidChecksum&&now-gpsBaudAt>=GPS_PROBE_MS) {
    if (gpsProbe==0) { gpsProbe=1; startGps(GPS_FALLBACK_BAUD,"probe-fallback"); }
    else if (gpsProbe==1) { gpsProbe=2; startGps(GPS_PRIMARY_BAUD,"probe-primary-lock"); }
  }
  return parsed;
}
static void onNtpSync(timeval *) { ntpSynced=true; }
static const char *disconnectReasonName(uint8_t reason) {
  switch ((wifi_err_reason_t)reason) {
    case WIFI_REASON_STA_LEAVING:return "STA_LEAVING";
    case WIFI_REASON_ASSOC_LEAVE:return "ASSOC_LEAVE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:return "CONNECTION_FAIL";
    case WIFI_REASON_TIMEOUT:return "TIMEOUT";
    default:return "OTHER";
  }
}
static bool terminalDisconnectReason(uint8_t reason) {
  return reason==WIFI_REASON_NO_AP_FOUND||reason==WIFI_REASON_AUTH_FAIL||reason==WIFI_REASON_ASSOC_FAIL||
         reason==WIFI_REASON_HANDSHAKE_TIMEOUT||reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT||
         reason==WIFI_REASON_CONNECTION_FAIL;
}
static void wifiEventLog(const char *name,uint8_t reason=0) {
  uint32_t elapsed=wifiBeginAt?millis()-wifiBeginAt:0;
  bool active=wifiBeginAt&&wifiAction!=WIFI_NONE&&!tamaDisconnecting;
  bool timeoutActive=active&&wifiConnection.active();
  Serial.printf("WIFI event=%s%s%s mode=%d status=%d active=%d timeout_active=%d local_cleanup=%d elapsed_ms=%lu\n",
                name,reason?" reason=":"",reason?disconnectReasonName(reason):"",(int)WiFi.getMode(),(int)WiFi.status(),
                active?1:0,timeoutActive?1:0,tamaDisconnecting?1:0,(unsigned long)elapsed);
  if (reason) Serial.printf("WIFI disconnect_reason=%u name=%s\n",(unsigned)reason,disconnectReasonName(reason));
}
static void onWifiEvent(WiFiEvent_t event,WiFiEventInfo_t info) {
  if (event==ARDUINO_EVENT_WIFI_STA_START) wifiEventLog("STA_START");
  else if (event==ARDUINO_EVENT_WIFI_STA_STOP) { if (!tamaDisconnecting&&wifiAction!=WIFI_NONE) { eventDisconnected=true; eventTerminal=true; } wifiEventLog("STA_STOP"); }
  else if (event==ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    eventConnected=true; wifiEventLog("STA_CONNECTED");
  } else if (event==ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    eventGotIp=true; wifiEventLog("STA_GOT_IP");
    String ip=WiFi.localIP().toString(); Serial.printf("WIFI auth=success dhcp=acquired ip=%s\n",ip.c_str());
  } else if (event==ARDUINO_EVENT_WIFI_STA_LOST_IP) wifiEventLog("STA_LOST_IP");
  else if (event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastDisconnectReason=info.wifi_sta_disconnected.reason;
    eventDisconnected=!tamaDisconnecting; eventTerminal=terminalDisconnectReason(lastDisconnectReason);
    wifiEventLog("STA_DISCONNECTED",lastDisconnectReason);
  }
}
static bool beginWifi(WifiAction action,const char *ssid,const char *pass) {
  if (!wifiOn) { setFeedback("WIFI OFF"); return false; }
  if (!ssid||!ssid[0]) { setFeedback("WIFI NOT CONFIGURED"); return false; }
  if (scanning) { setFeedback("WIFI BUSY"); return false; }
  WiFi.mode(WIFI_STA);
  wifiAction=action; wifiStartedAt=millis(); ntpStartedAt=0; ntpSynced=false; syncState=RTC_SYNC_WIFI_CONNECTING;
  snprintf(connectSsid,sizeof(connectSsid),"%s",ssid); snprintf(connectPass,sizeof(connectPass),"%s",pass?pass:"");
  wifiBeginPending=true; lastWifiStatus=(int)WiFi.status(); lastDisconnectReason=0; tamaDisconnecting=false;
  eventConnected=false; eventGotIp=false; eventDisconnected=false; eventTerminal=false;
  wifiConnection.phase=TpWifi::IDLE; wifiBeginAt=0;
  Serial.printf("WIFI connect staged ssid=\"%s\" password_length=%u purpose=%s credentials=tp-wifi timeout_ms=%lu\n",connectSsid,
                (unsigned)strlen(pass?pass:""),action==WIFI_AUTO_NTP?"auto-ntp":action==WIFI_MANUAL_NTP?"manual-ntp":"setup",
                (unsigned long)TpWifi::CONNECT_TIMEOUT_MS);
  return true;
}
static bool persistKnownNetworks() {
  Preferences p; if (!p.begin("tp-wifi",false)) return false;
  bool ok=p.putUChar("count",knownCount)>0;
  for (int i=0;i<TpWifi::MAX_SAVED_NETWORKS;++i) {
    char ssidKey[8],passKey[8]; snprintf(ssidKey,sizeof(ssidKey),"ssid%d",i); snprintf(passKey,sizeof(passKey),"pass%d",i);
    if (i<knownCount) {
      ok=p.putString(ssidKey,knownSsid[i])==strlen(knownSsid[i])&&ok;
      size_t written=p.putString(passKey,knownPass[i]); ok=(knownPass[i][0]?written==strlen(knownPass[i]):true)&&ok;
    } else { p.remove(ssidKey); p.remove(passKey); }
  }
  if (ok) { p.remove("ssid"); p.remove("pass"); }
  p.end(); return ok;
}
static bool saveCandidate() {
  int slot=TpWifi::upsertCredential(knownSsid,knownPass,knownCount,candidateSsid,candidatePass);
  if (slot<0) { setFeedback("WIFI SAVE FAILED"); return false; }
  currentKnown=(uint8_t)slot; wifiConfigured=knownCount>0;
  Preferences state; bool onWritten=state.begin("tp-wifi",false); if (onWritten) { onWritten=state.putBool("on",true)>0; state.end(); }
  bool ok=onWritten&&persistKnownNetworks();
  Serial.printf("WIFI NVS save known_count=%u active_slot=%u result=%s\n",(unsigned)knownCount,(unsigned)currentKnown,ok?"ok":"failed");
  if (!ok) setFeedback("WIFI SAVE FAILED"); return ok;
}
static void failWifi(const char *message) {
  bool automatic=wifiAction==WIFI_AUTO_NTP;
  const char *action=automatic?"auto-ntp":wifiAction==WIFI_MANUAL_NTP?"ntp-sync":
                     wifiAction==WIFI_CONNECT_CANDIDATE?"connect-candidate":"connect-saved";
  Serial.printf("CLOCK Wi-Fi/NTP failed reason=%s\n",message); radioOff(message); setFeedback(message);
  if (automatic) { autoFailed=true; syncState=RTC_SYNC_FAILED; Serial.printf("CLOCK automatic sync failed gps=%s wifi=%s\n",gpsSeen?"no-time":"no-data",message); }
  else syncState=policy.delivered?RTC_SYNC_IDLE:RTC_SYNC_FAILED;
  clearWifiAction(action);
}
static int knownIndexFor(const char *ssid) {
  return TpWifi::findCredential(knownSsid,knownCount,ssid);
}
static void pollScan() {
  if (!scanning) return;
  int found=WiFi.scanComplete(); if (found==WIFI_SCAN_RUNNING) return;
  scanning=false; scanCount=0;
  if (found<0) {
    setFeedback("WIFI SCAN FAILED"); WiFi.scanDelete();
    if (scanConnectAction!=WIFI_NONE) { WifiAction action=scanConnectAction; scanConnectAction=WIFI_NONE; wifiAction=action; wifiConnection.phase=TpWifi::FAILED; failWifi("WIFI SCAN FAILED"); }
    else { radioOff("scan-failed"); clearWifiAction("scan"); }
    return;
  }
  int bestKnown=-1,bestKnownRssi=-1000;
  for (int i=0;i<found;++i) {
    String ssid=WiFi.SSID(i); if (!ssid.length()) continue;
    int rssi=WiFi.RSSI(i),known=knownIndexFor(ssid.c_str()); if (known>=0&&rssi>bestKnownRssi) { bestKnown=known; bestKnownRssi=rssi; }
    if (scanCount<MAX_SCAN_RESULTS) { snprintf(scanSsid[scanCount],sizeof(scanSsid[scanCount]),"%s",ssid.c_str()); scanRssi[scanCount++]=rssi; }
  }
  WiFi.scanDelete();
  if (scanConnectAction!=WIFI_NONE) {
    WifiAction action=scanConnectAction; scanConnectAction=WIFI_NONE;
    if (bestKnown>=0) { currentKnown=(uint8_t)bestKnown; Serial.printf("WIFI known match slot=%d ssid=\"%s\" rssi=%d\n",bestKnown,knownSsid[bestKnown],bestKnownRssi); beginWifi(action,knownSsid[bestKnown],knownPass[bestKnown]); }
    else { wifiAction=action; wifiConnection.phase=TpWifi::FAILED; lastDisconnectReason=WIFI_REASON_NO_AP_FOUND; failWifi("WIFI AP NOT FOUND"); }
    return;
  }
  radioOff("scan-complete"); setFeedback(scanCount?"SELECT NETWORK":"NO NETWORKS"); clearWifiAction("scan");
}
static bool startScanFor(WifiAction action) {
  tamaDisconnecting=false; WiFi.mode(WIFI_STA); scanCount=0; scanConnectAction=action;
  int result=WiFi.scanNetworks(true,true); scanning=result==WIFI_SCAN_RUNNING;
  Serial.printf("WIFI action=scan phase=requested result=%d followup=%u\n",result,(unsigned)action);
  if (!scanning) { scanConnectAction=WIFI_NONE; return false; } return true;
}
static void queueWifi(WifiCommand command) {
  if (wifiCommand!=WIFI_CMD_NONE) Serial.printf("WIFI action=%s phase=replaced previous=%s\n",wifiCommandName(command),wifiCommandName(wifiCommand));
  wifiCommand=command;
}
static void serviceWifiCommand() {
  if (wifiCommand==WIFI_CMD_NONE||wifiAction!=WIFI_NONE||scanning) return;
  WifiCommand command=wifiCommand; wifiCommand=WIFI_CMD_NONE; const char *name=wifiCommandName(command); markWifiAction(name,command);
  if (command==WIFI_CMD_ENABLE||command==WIFI_CMD_DISABLE) {
    Preferences p; bool ok=p.begin("tp-wifi",false); if (ok) { p.putBool("on",wifiOn); p.end(); }
    if (!wifiOn) radioOff("disabled"); setFeedback(wifiOn?"WIFI ON":"WIFI OFF");
    if (!ok) { Serial.println("WIFI NVS enabled-state write failed"); setFeedback("WIFI SAVE FAILED"); }
    clearWifiAction(name); return;
  }
  if (command==WIFI_CMD_FORGET) {
    wifiConnection.phase=TpWifi::CANCELED; radioOff("forget");
    TpWifi::removeCredential(knownSsid,knownPass,knownCount,currentKnown); wifiConfigured=knownCount>0;
    bool ok=persistKnownNetworks();
    setFeedback(ok?"WIFI FORGOTTEN":"WIFI SAVE FAILED"); clearWifiAction(name); return;
  }
  if (!wifiOn) { setFeedback("WIFI OFF"); clearWifiAction(name); return; }
  if (command==WIFI_CMD_SCAN) {
    bool started=startScanFor(WIFI_NONE);
    if (!started) { setFeedback("WIFI SCAN FAILED"); radioOff("scan-start-failed"); clearWifiAction(name); }
    else setFeedback("WIFI SCANNING");
    return;
  }
  WifiAction action=command==WIFI_CMD_CONNECT_CANDIDATE?WIFI_CONNECT_CANDIDATE:
                    command==WIFI_CMD_CONNECT_SAVED||command==WIFI_CMD_CONNECT_SELECTED?WIFI_CONNECT_SAVED:WIFI_MANUAL_NTP;
  if (command==WIFI_CMD_CONNECT_SAVED||command==WIFI_CMD_SYNC) {
    wifiAction=action; if (!startScanFor(action)) { wifiConnection.phase=TpWifi::FAILED; failWifi("WIFI SCAN FAILED"); } return;
  }
  const char *ssid=command==WIFI_CMD_CONNECT_CANDIDATE?candidateSsid:(knownCount?knownSsid[currentKnown]:"");
  const char *pass=command==WIFI_CMD_CONNECT_CANDIDATE?candidatePass:(knownCount?knownPass[currentKnown]:"");
  if (!beginWifi(action,ssid,pass)) clearWifiAction(name);
}
static void pollWifi(uint32_t now) {
  if (wifiCommand==WIFI_CMD_DISABLE&&(wifiAction!=WIFI_NONE||scanning)) {
    wifiCommand=WIFI_CMD_NONE; scanning=false; scanConnectAction=WIFI_NONE; WiFi.scanDelete(); wifiConnection.phase=TpWifi::CANCELED;
    Preferences p; if (p.begin("tp-wifi",false)) { p.putBool("on",false); p.end(); }
    radioOff("user-cancel"); setFeedback("WIFI OFF"); clearWifiAction("disable"); return;
  }
  pollScan(); if (scanning) return; serviceWifiCommand(); if (wifiAction==WIFI_NONE) return;
  if (wifiBeginPending&&now-wifiStartedAt>=200) {
    wifiBeginPending=false; wifiBeginAt=now; wifiConnection.begin(now); wl_status_t result=WiFi.begin(connectSsid,connectPass);
    Serial.printf("WIFI begin ssid=\"%s\" password_length=%u return_status=%d asynchronous=1\n",connectSsid,(unsigned)strlen(connectPass),(int)result);
  }
  if (eventConnected) { eventConnected=false; wifiConnection.connected(); }
  if (eventGotIp) { eventGotIp=false; wifiConnection.gotIp(); }
  if (eventDisconnected) { eventDisconnected=false; wifiConnection.disconnected(lastDisconnectReason,eventTerminal,false); }
  wifiConnection.poll(now);
  int status=(int)WiFi.status();
  if (status!=lastWifiStatus) { Serial.printf("WIFI state previous=%d current=%d\n",lastWifiStatus,status); lastWifiStatus=status; }
  if (!ntpStartedAt&&wifiConnection.success()) {
    if (wifiAction==WIFI_CONNECT_CANDIDATE) { bool saved=saveCandidate(); if (saved) setFeedback("WIFI SAVED"); syncState=RTC_SYNC_IDLE; radioOff(saved?"candidate-saved":"candidate-save-failed"); clearWifiAction("connect-candidate"); return; }
    if (wifiAction==WIFI_CONNECT_SAVED) { setFeedback("WIFI CONNECTED"); syncState=RTC_SYNC_IDLE; radioOff("connection-verified"); clearWifiAction("connect-saved"); return; }
    ntpStartedAt=now; wallBeforeNtp=rtcEpoch(); syncState=RTC_SYNC_NTP_WAITING;
    sntp_set_time_sync_notification_cb(onNtpSync); configTime(0,0,"pool.ntp.org","time.cloudflare.com");
    // Arduino's configTime(0,0,...) sets the process TZ to UTC0. Restore the
    // selected presentation rule immediately; the system epoch remains UTC.
    applyTimezone();
    Serial.println("CLOCK source=WIFI NTP requested");
  }
  if (!ntpStartedAt&&!wifiBeginPending&&wifiConnection.failed()) {
    Serial.printf("WIFI connect terminal phase=%u status=%d disconnect_reason=%u elapsed_ms=%lu\n",(unsigned)wifiConnection.phase,status,
                  (unsigned)lastDisconnectReason,(unsigned long)(wifiBeginAt?now-wifiBeginAt:0));
    const char *message=lastDisconnectReason==WIFI_REASON_AUTH_FAIL||lastDisconnectReason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT||lastDisconnectReason==WIFI_REASON_HANDSHAKE_TIMEOUT?
                        "WIFI AUTH FAILED":lastDisconnectReason==WIFI_REASON_NO_AP_FOUND?"WIFI AP NOT FOUND":"WIFI CONNECT FAILED";
    failWifi(message); return;
  }
  if (ntpStartedAt&&!ntpSynced&&now-ntpStartedAt>NTP_WAIT_MS) failWifi("NTP FAILED");
}
static void startAutomaticFallback(uint32_t now) {
  if (policy.delivered||autoStarted||now-bootAtMs<GPS_GRACE_MS) return;
  autoStarted=true;
  if (TpTime::SyncPolicy::wifiFallbackAllowed(wifiOn,wifiConfigured)) {
    markWifiAction("auto-ntp",WIFI_CMD_SYNC);
    wifiAction=WIFI_AUTO_NTP;
    if (!startScanFor(WIFI_AUTO_NTP)) { wifiConnection.phase=TpWifi::FAILED; failWifi("WIFI SCAN FAILED"); }
  }
  else { autoFailed=true; syncState=RTC_SYNC_FAILED; Serial.printf("CLOCK automatic sync failed gps=%s wifi=%s\n",gpsSeen?"no-time":"no-data",wifiOn?"not-configured":"disabled"); }
}
static bool acceptSource(RtcTimeSource source,uint32_t utc,uint32_t now,uint32_t &wallEpoch) {
  if (policy.delivered||!validEpoch(utc)) return false;
  const char *name=source==RTC_TIME_GPS?"GPS":"WIFI";
  uint32_t priorUtc=savedUtc,priorWall=savedWall;
  uint32_t currentWall=(source==RTC_TIME_WIFI&&ntpStartedAt&&validEpoch(wallBeforeNtp))?wallBeforeNtp+(now-ntpStartedAt)/1000:rtcEpoch();
  Serial.printf("CLOCK source=%s acquired_utc=%lu\n",name,(unsigned long)utc);
  Serial.printf("CLOCK references saved_utc=%lu saved_wall=%lu offset=%ld\n",(unsigned long)priorUtc,(unsigned long)priorWall,(long)wallOffset);
  bool hadMapping=mapped;
  // NTP setup can replace TZ with UTC0. Reapply for both sources so a GPS fix
  // arriving during NTP fallback is projected with the selected timezone too.
  applyTimezone();
  if (mapped) {
    if (!TpTime::validDelta(utc,savedUtc)) {
      Serial.printf("CLOCK source=%s rejected delta=%lu\n",name,(unsigned long)(utc>=savedUtc?utc-savedUtc:0));
      setFeedback(source==RTC_TIME_GPS?"NO GPS TIME":"NTP FAILED"); return false;
    }
    wallEpoch=wallFromUtc(utc); if (!validEpoch(wallEpoch)) return false;
  } else {
    wallEpoch=wallFromUtc(utc);
    Serial.printf("CLOCK first authoritative mapping timezone=%s\n",rtcTimezoneLabel());
  }
  if (!policy.accept(source==RTC_TIME_GPS?TpTime::GPS:TpTime::WIFI,utc)) return false;
  mapped=true; trusted=true; authoritative=true; lastSyncCatchupAllowed=hadMapping&&policy.consumeCatchup(); timeSource=source; syncState=RTC_SYNC_IDLE;
  sourceUtc=utc; sourceAtMs=now; savedUtc=utc; savedWall=wallEpoch; setSystem(utc); persistNow(wallEpoch,utc);
  if (wifiAction!=WIFI_NONE) { radioOff("time-synced"); clearWifiAction("ntp-sync"); }
  setFeedback(source==RTC_TIME_GPS?"GPS TIME OK":"WIFI TIME OK");
  if (source==RTC_TIME_GPS) Serial.printf("GPS authoritative accepted=1 utc=%lu catchup=%d\n",(unsigned long)utc,lastSyncCatchupAllowed?1:0);
  Serial.printf("CLOCK mapping current_wall=%lu synced_wall=%lu offset=%ld\n",(unsigned long)currentWall,(unsigned long)wallEpoch,(long)wallOffset);
  return true;
}

bool rtcBegin(uint32_t petSavedWall) {
  Preferences p; p.begin("tp-clock",true); savedWall=p.getUInt("wall",0); savedUtc=p.getUInt("utc",0);
  mapped=p.getBool("mapped",false)&&validEpoch(savedWall)&&validEpoch(savedUtc);
  trusted=p.getBool("trusted",mapped)&&validEpoch(savedWall); wallOffset=p.getInt("offset",0);
  uint8_t oldTimezone=p.getUChar("tz",TpTimezone::UTC); char timezoneId[28]={0};
  p.getString("tzid",timezoneId,sizeof(timezoneId)); p.end();
  bool timezoneMigrated=false,timezoneFallback=false;
  if (timezoneId[0]) { timezoneFallback=!TpTimezone::findId(timezoneId,timezonePreset); timezoneMigrated=timezoneFallback; }
  else if (oldTimezone<TpTimezone::COUNT) { timezonePreset=TpTimezone::valid(oldTimezone); timezoneMigrated=true; }
  else { timezonePreset=TpTimezone::UTC; timezoneMigrated=timezoneFallback=true; }
  applyTimezone();
  if (timezoneMigrated) {
    Preferences migration; if (migration.begin("tp-clock",false)) {
      migration.putUChar("tz",(uint8_t)timezonePreset); migration.putString("tzid",TpTimezone::id(timezonePreset)); migration.end();
    }
    Serial.printf("CLOCK timezone migration legacy=%u stable_id=%s fallback=%d\n",(unsigned)oldTimezone,TpTimezone::id(timezonePreset),timezoneFallback?1:0);
  }
  // Keep the previous tp-clock acceptance range for backward compatibility.
  if (wallOffset<-2*86400L||wallOffset>2*86400L) mapped=false;
  Preferences w; bool wifiNvs=w.begin("tp-wifi",true); bool migrateLegacy=false;
  if (wifiNvs) {
    wifiOn=w.getBool("on",false); knownCount=min((int)w.getUChar("count",0),TpWifi::MAX_SAVED_NETWORKS);
    for (int i=0;i<knownCount;++i) { char sk[8],pk[8]; snprintf(sk,sizeof(sk),"ssid%d",i); snprintf(pk,sizeof(pk),"pass%d",i); w.getString(sk,knownSsid[i],sizeof(knownSsid[i])); w.getString(pk,knownPass[i],sizeof(knownPass[i])); }
    if (!knownCount&&w.isKey("ssid")) { w.getString("ssid",knownSsid[0],sizeof(knownSsid[0])); w.getString("pass",knownPass[0],sizeof(knownPass[0])); if (knownSsid[0][0]) { knownCount=1; migrateLegacy=true; } }
    w.end();
  }
  wifiConfigured=knownCount>0; if (migrateLegacy) persistKnownNetworks(); uint32_t restore=validEpoch(savedWall)?savedWall:0;
  if (petSavedWall>restore&&validEpoch(petSavedWall)) restore=petSavedWall;
  if (mapped) setSystem(savedUtc); else if (restore) setSystem(restore);
  bootAtMs=millis();
  if (mapped) { sourceUtc=savedUtc; sourceAtMs=bootAtMs; }
  syncState=RTC_SYNC_GPS_SEARCHING; startGps(GPS_PRIMARY_BAUD,"tdeck-plus-default");
  WiFi.onEvent(onWifiEvent);
  Serial.printf("CLOCK boot saved_wall=%lu saved_utc=%lu offset=%ld mapped=%d wifi=%s\n",
                (unsigned long)savedWall,(unsigned long)savedUtc,(long)wallOffset,mapped?1:0,!wifiOn?"off":wifiConfigured?"configured":"not-configured");
  esp_reset_reason_t resetReason=esp_reset_reason();
  Serial.printf("RESET reason=%s code=%d previous_wifi_action=%s nvs=%s\n",resetReasonName(resetReason),(int)resetReason,
                wifiBreadcrumbMagic==WIFI_BREADCRUMB_MAGIC?wifiCommandName((WifiCommand)wifiBreadcrumbAction):"unknown",
                wifiNvs?"ok":"open-failed");
  wifiBreadcrumbMagic=WIFI_BREADCRUMB_MAGIC; wifiBreadcrumbAction=WIFI_CMD_NONE;
  return true;
}
uint32_t rtcEpoch() {
  if (!clockValid) return 0; uint32_t systemEpoch=(uint32_t)time(nullptr);
  return mapped?wallFromUtc(systemEpoch):systemEpoch;
}
void rtcSetEpoch(uint32_t e) { setSystem(e); }
bool rtcPollAuthoritative(uint32_t &wallEpoch) {
  uint32_t now=millis(),utc=0;
  if (pollGps(utc)&&!policy.delivered&&acceptSource(RTC_TIME_GPS,utc,now,wallEpoch)) return true;
  startAutomaticFallback(now); pollWifi(now);
  if (gpsManualUntil&&(int32_t)(now-gpsManualUntil)>=0) { gpsManualUntil=0; if (!policy.delivered) { syncState=RTC_SYNC_FAILED; setFeedback("NO GPS TIME"); } }
  if (ntpSynced&&wifiAction!=WIFI_NONE) { ntpSynced=false; utc=(uint32_t)time(nullptr); if (acceptSource(RTC_TIME_WIFI,utc,now,wallEpoch)) return true; }
  return false;
}
bool rtcAuthoritative() { return authoritative; }
void rtcPersistReference(uint32_t wallEpoch) {
  if (!validEpoch(wallEpoch)||millis()-lastPersistMs<PERSIST_MS) return;
  persistNow(wallEpoch,mapped?currentUtc():0);
}
void rtcManualClockSet(uint32_t wallEpoch) {
  manualCatchup.cancel();
  uint32_t utc=currentUtc(); policy.setManual(); trusted=true; timeSource=RTC_TIME_MANUAL; syncState=RTC_SYNC_IDLE; setFeedback("MANUAL TIME SET");
  if (authoritative&&validEpoch(utc)) {
    uint32_t enteredUtc=utcFromWall(wallEpoch);
    if (!validEpoch(enteredUtc)) { setFeedback("INVALID LOCAL TIME"); return; }
    sourceUtc=enteredUtc; sourceAtMs=millis(); savedWall=wallEpoch; setSystem(enteredUtc); persistNow(wallEpoch,enteredUtc);
  } else {
    mapped=false; savedUtc=0; savedWall=wallEpoch; setSystem(wallEpoch); persistNow(wallEpoch,0);
  }
}
bool rtcHasTrustedReference() { return trusted&&validEpoch(savedWall); }
bool rtcPrepareManualCatchup(uint32_t referenceWall,uint32_t newWall,uint32_t &elapsedSeconds) {
  elapsedSeconds=0;
  if (!rtcHasTrustedReference()||!manualCatchup.prepare(referenceWall,newWall)) {
    setFeedback(!rtcHasTrustedReference()?"NO TRUSTED TIME":newWall<=referenceWall?"TIME MUST ADVANCE":"INVALID TIME JUMP");
    return false;
  }
  elapsedSeconds=manualCatchup.elapsed; return true;
}
bool rtcApplyManualCatchup(uint32_t referenceWall,uint32_t newWall) {
  if (!manualCatchup.consume(referenceWall,newWall)) { setFeedback("CATCH-UP EXPIRED"); return false; }
  policy.setManual(); trusted=true; timeSource=RTC_TIME_MANUAL; syncState=RTC_SYNC_IDLE;
  if (mapped) {
    uint32_t utc=utcFromWall(newWall);
    if (!validEpoch(utc)) { setFeedback("INVALID TIME JUMP"); return false; }
    sourceUtc=utc; sourceAtMs=millis(); setSystem(utc); persistNow(newWall,utc);
  } else {
    mapped=false; savedUtc=0; setSystem(newWall); persistNow(newWall,0);
  }
  setFeedback("CATCH-UP APPLIED"); return true;
}
void rtcCancelManualCatchup() { manualCatchup.cancel(); }
RtcTimeSource rtcTimeSource() { return timeSource; }
RtcSyncState rtcSyncState() { return syncState; }
const char *rtcTimeStatusLabel() {
  return TpTime::sourceLabel((TpTime::Source)timeSource);
}
const char *rtcTimezoneLabel() { return TpTimezone::label(timezonePreset); }
uint8_t rtcTimezoneIndex() { return (uint8_t)timezonePreset; }
uint32_t rtcSetTimezoneIndex(uint8_t index) {
  timezonePreset=TpTimezone::valid(index); applyTimezone();
  Preferences p; if (p.begin("tp-clock",false)) { p.putUChar("tz",(uint8_t)timezonePreset); p.putString("tzid",TpTimezone::id(timezonePreset)); p.end(); }
  uint32_t utc=mapped?(uint32_t)time(nullptr):0,wall=validEpoch(utc)?wallFromUtc(utc):rtcEpoch();
  if (mapped&&validEpoch(wall)) persistNow(wall,utc);
  setFeedback("TIMEZONE CHANGED");
  Serial.printf("CLOCK timezone changed authoritative_utc=%lu local_wall=%lu catchup=0\n",(unsigned long)utc,(unsigned long)wall);
  return wall;
}
const char *rtcSyncFeedback() { return feedback[0]&&(int32_t)(feedbackUntil-millis())>0?feedback:nullptr; }
bool rtcAutomaticSyncFailed() { return policy.consumeFailurePrompt(autoFailed); }
bool rtcLastSyncAllowsCatchup() { return lastSyncCatchupAllowed; }
void rtcRequestGpsSync() {
  if (policy.delivered) { setFeedback(timeSource==RTC_TIME_GPS?"GPS TIME OK":"TIME ALREADY SYNCED"); return; }
  gpsManualUntil=millis()+MANUAL_GPS_MS; syncState=RTC_SYNC_GPS_SEARCHING; setFeedback("GPS SEARCHING"); Serial.println("CLOCK manual GPS request");
}
void rtcRequestWifiSync() {
  if (policy.delivered) { setFeedback(timeSource==RTC_TIME_WIFI?"WIFI TIME OK":"TIME ALREADY SYNCED"); return; }
  if (!wifiConfigured) { setFeedback("WIFI NOT CONFIGURED"); Serial.println("WIFI action=ntp-sync phase=rejected reason=not-configured"); return; }
  queueWifi(WIFI_CMD_SYNC);
}
bool rtcWifiEnabled() { return wifiOn; }
bool rtcWifiConfigured() { return wifiConfigured; }
const char *rtcWifiSsid() { return knownCount?knownSsid[currentKnown]:""; }
int rtcWifiKnownCount() { return knownCount; }
void rtcWifiSetEnabled(bool enabled) {
  wifiOn=enabled; queueWifi(enabled?WIFI_CMD_ENABLE:WIFI_CMD_DISABLE); setFeedback(enabled?"WIFI ENABLING":"WIFI DISABLING");
}
void rtcWifiForget() {
  queueWifi(WIFI_CMD_FORGET); setFeedback("FORGETTING WIFI");
}
bool rtcWifiStartScan() {
  if (!wifiOn) { setFeedback("WIFI OFF"); return false; }
  queueWifi(WIFI_CMD_SCAN); setFeedback("WIFI SCAN QUEUED"); return true;
}
bool rtcWifiScanning() { return scanning; }
int rtcWifiScanCount() { return scanCount; }
const char *rtcWifiScanSsid(int index) { return index>=0&&index<scanCount?scanSsid[index]:""; }
int rtcWifiScanRssi(int index) { return index>=0&&index<scanCount?scanRssi[index]:0; }
bool rtcWifiScanKnown(int index) { return index>=0&&index<scanCount&&knownIndexFor(scanSsid[index])>=0; }
bool rtcWifiConnectScanResult(int index) {
  if (!rtcWifiScanKnown(index)) return false; currentKnown=(uint8_t)knownIndexFor(scanSsid[index]);
  queueWifi(WIFI_CMD_CONNECT_SELECTED); setFeedback("KNOWN NETWORK"); return true;
}
bool rtcWifiConnectSaved() {
  if (!wifiConfigured) { setFeedback("WIFI NOT CONFIGURED"); Serial.println("WIFI action=connect-saved phase=rejected reason=not-configured"); return false; }
  queueWifi(WIFI_CMD_CONNECT_SAVED); setFeedback("WIFI CONNECT QUEUED"); return true;
}
bool rtcWifiConnectCandidate(const char *ssid,const char *password) {
  if (!ssid||!ssid[0]) return false;
  snprintf(candidateSsid,sizeof(candidateSsid),"%s",ssid); snprintf(candidatePass,sizeof(candidatePass),"%s",password?password:"");
  Serial.printf("WIFI candidate ssid=\"%s\" password_length=%u terminated=1\n",candidateSsid,(unsigned)strlen(candidatePass));
  queueWifi(WIFI_CMD_CONNECT_CANDIDATE); setFeedback("WIFI CONNECT QUEUED"); return true;
}

static int batteryMv=0,batteryPct=-1,batteryAdcMv=0,batteryRaw=0;
static uint32_t batteryReadAt=0,batteryLogAt=0;
static void refreshBattery(bool force=false) {
  uint32_t now=millis();
  if (!force&&batteryReadAt&&now-batteryReadAt<10000UL) return;
  batteryReadAt=now?now:1;
  uint32_t rawSum=0,mvSum=0;
  for (int i=0;i<8;++i) { rawSum+=analogRead(TDeckPins::batteryAdc); mvSum+=analogReadMilliVolts(TDeckPins::batteryAdc); }
  batteryRaw=(int)(rawSum/8); batteryAdcMv=(int)(mvSum/8);
  int sample=TpBattery::batteryMillivoltsFromAdc(batteryAdcMv);
  if (!TpBattery::validMillivolts(sample)) { batteryMv=0; batteryPct=-1; }
  else {
    batteryMv=TpBattery::smoothMillivolts(batteryMv,sample);
    batteryPct=TpBattery::stabilizePercent(batteryPct,TpBattery::estimatePercent(batteryMv));
  }
  if (force||!batteryLogAt||now-batteryLogAt>=60000UL) {
    batteryLogAt=now?now:1;
    Serial.printf("BATTERY raw_adc=%d adc_mv=%d cell_mv=%d formula=adc_mv*2 smooth_mv=%d estimated_pct=%d approximate=1\n",
                  batteryRaw,batteryAdcMv,sample,batteryMv,batteryPct);
  }
}
bool batBegin() {
  pinMode(TDeckPins::batteryAdc,INPUT);
  analogReadResolution(12); analogSetPinAttenuation(TDeckPins::batteryAdc,ADC_11db);
  refreshBattery(true); return batteryPct>=0;
}
void pmuEnablePanel() {}
int batPercent() { refreshBattery(); return batteryPct; }
bool batCharging() { return false; }
int batMillivolts() { refreshBattery(); return batteryMv; }
bool usbPresent() { return false; }
void pwrSetup() {}
bool pwrShortPressed() { return false; }
bool powerStatusKnown() { return false; }
#endif
