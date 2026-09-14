#include "../board_select.h"
#if TAMAPOKE_TDECK_PLUS
#include "../../../rtcbat.h"
#include "pins.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <sys/time.h>

// The game historically stores a local-wall-clock-shaped epoch. Keep that
// user-facing convention. Once UTC is learned, persist UTC and wall snapshots
// together so later boots can measure real elapsed time without inventing a
// timezone or reinterpreting old saves.
constexpr uint32_t MIN_VALID_EPOCH=1704067200UL; // 2024-01-01 UTC
constexpr uint32_t MAX_VALID_EPOCH=4102444799UL; // end of 2099
constexpr uint32_t MAX_SOURCE_JUMP=10UL*366*86400;
constexpr uint32_t GPS_GRACE_MS=45000, WIFI_WAIT_MS=15000, NTP_WAIT_MS=20000;
constexpr uint32_t PERSIST_MS=5UL*60*1000;
static bool clockValid=false, mapped=false, authoritative=false;
static bool sourceDelivered=false, gpsAccepted=false, ntpAccepted=false;
static bool gpsSeen=false, wifiTried=false, ntpStarted=false, hadPriorWall=false;
static uint32_t savedWall=0, savedUtc=0, sourceUtc=0, sourceAtMs=0;
static int32_t wallOffset=0;
static uint32_t wifiStartedAt=0, ntpStartedAt=0, wallBeforeNtp=0, lastPersistMs=0;
static volatile bool ntpSynced=false;
static HardwareSerial gps(1);
static char nmea[112]; static uint8_t nmeaLen=0;

static bool validEpoch(uint32_t e) { return e>=MIN_VALID_EPOCH && e<=MAX_VALID_EPOCH; }
static void setSystem(uint32_t e) {
  timeval tv{(time_t)e,0};
  clockValid=settimeofday(&tv,nullptr)==0;
}
static uint32_t currentUtc() {
  return sourceUtc ? sourceUtc+(millis()-sourceAtMs)/1000 : 0;
}
static uint32_t wallFromUtc(uint32_t utc) {
  int64_t wall=(int64_t)utc+wallOffset;
  return wall>0 && wall<=UINT32_MAX ? (uint32_t)wall : 0;
}
static void persistNow(uint32_t wall,uint32_t utc) {
  if (!validEpoch(wall)) return;
  Preferences p; p.begin("tp-clock",false);
  p.putUInt("wall",wall);
  if (mapped && validEpoch(utc)) {
    p.putUInt("utc",utc); p.putInt("offset",wallOffset); p.putBool("mapped",true);
  }
  p.end(); lastPersistMs=millis();
}

// Civil date -> Unix day count, independent of libc timezone configuration.
static int64_t daysFromCivil(int y,unsigned m,unsigned d) {
  y-=m<=2; const int era=(y>=0?y:y-399)/400;
  const unsigned yoe=(unsigned)(y-era*400);
  const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
  const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
  return era*146097+(int)doe-719468;
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
  return hh>=0&&hh<24&&mm>=0&&mm<60&&ss>=0&&ss<=60;
}
static int hexNibble(char c) {
  if (c>='0'&&c<='9') return c-'0'; if (c>='A'&&c<='F') return c-'A'+10;
  if (c>='a'&&c<='f') return c-'a'+10; return -1;
}
static bool parseNmea(char *line,uint32_t &utc) {
  char *star=strchr(line,'*'); if (line[0]!='$'||!star||strlen(star)<3) return false;
  uint8_t sum=0; for (char *p=line+1;p<star;++p) sum^=(uint8_t)*p;
  int hi=hexNibble(star[1]),lo=hexNibble(star[2]); if (hi<0||lo<0||sum!=(uint8_t)((hi<<4)|lo)) return false;
  *star=0; char *fields[16]; int count=0;
  fields[count++]=line;
  for (char *p=line;*p&&count<16;++p) if (*p==',') { *p=0; fields[count++]=p+1; }
  int hh,mm,ss;
  if (count>=10 && (!strcmp(fields[0],"$GPRMC")||!strcmp(fields[0],"$GNRMC"))) {
    if (!parseTime(fields[1],hh,mm,ss)||strlen(fields[9])<6) return false;
    for (int i=0;i<6;++i) if (fields[9][i]<'0'||fields[9][i]>'9') return false;
    int day=(fields[9][0]-'0')*10+fields[9][1]-'0';
    int mon=(fields[9][2]-'0')*10+fields[9][3]-'0';
    int year=2000+(fields[9][4]-'0')*10+fields[9][5]-'0';
    return makeUtc(year,mon,day,hh,mm,ss,utc); // UTC is valid before a position fix.
  }
  if (count>=5 && (!strcmp(fields[0],"$GPZDA")||!strcmp(fields[0],"$GNZDA"))) {
    if (!parseTime(fields[1],hh,mm,ss)) return false;
    return makeUtc(atoi(fields[4]),atoi(fields[3]),atoi(fields[2]),hh,mm,ss,utc);
  }
  return false;
}
static bool pollGps(uint32_t &utc) {
  while (gps.available()) {
    char c=(char)gps.read(); gpsSeen=true;
    if (c=='\n') {
      nmea[nmeaLen]=0; bool ok=parseNmea(nmea,utc); nmeaLen=0; if (ok) return true;
    } else if (c!='\r') {
      if (nmeaLen<sizeof(nmea)-1) nmea[nmeaLen++]=c; else nmeaLen=0;
    }
  }
  return false;
}
static void onNtpSync(timeval *) { ntpSynced=true; }
static bool storedWifiCredentials() {
  WiFi.mode(WIFI_STA); wifi_config_t config{};
  bool present=esp_wifi_get_config(WIFI_IF_STA,&config)==ESP_OK && config.sta.ssid[0];
  if (!present) WiFi.mode(WIFI_OFF);
  return present;
}
static void pollNtp(uint32_t now) {
  if (!wifiTried && now>=GPS_GRACE_MS) {
    wifiTried=true;
    if (storedWifiCredentials()) {
      WiFi.begin(); wifiStartedAt=now;
      Serial.println("CLOCK GPS pending; trying stored Wi-Fi credentials for NTP");
    } else Serial.println("CLOCK NTP skipped: no stored Wi-Fi credentials");
  }
  if (!wifiStartedAt) return;
  if (!ntpStarted && WiFi.status()==WL_CONNECTED) {
    ntpStarted=true; ntpStartedAt=now; ntpSynced=false;
    wallBeforeNtp=rtcEpoch();
    sntp_set_time_sync_notification_cb(onNtpSync);
    configTime(0,0,"pool.ntp.org","time.cloudflare.com");
  }
  if (!ntpStarted && now-wifiStartedAt>WIFI_WAIT_MS) {
    WiFi.disconnect(false,false); WiFi.mode(WIFI_OFF); wifiStartedAt=0;
    Serial.println("CLOCK NTP skipped: stored network unavailable");
  } else if (ntpStarted && !ntpSynced && now-ntpStartedAt>NTP_WAIT_MS) {
    WiFi.disconnect(false,false); WiFi.mode(WIFI_OFF); wifiStartedAt=0;
    Serial.println("CLOCK NTP timed out; manual clock remains available");
  }
}

bool rtcBegin(uint32_t petSavedWall) {
  Preferences p; p.begin("tp-clock",true);
  savedWall=p.getUInt("wall",0); savedUtc=p.getUInt("utc",0);
  mapped=p.getBool("mapped",false) && validEpoch(savedWall) && validEpoch(savedUtc);
  wallOffset=p.getInt("offset",0); p.end();
  if (wallOffset < -2*86400L || wallOffset > 2*86400L) mapped=false;
  hadPriorWall=validEpoch(savedWall)||validEpoch(petSavedWall);
  uint32_t restore=validEpoch(savedWall)?savedWall:0;
  if (petSavedWall>restore && validEpoch(petSavedWall)) restore=petSavedWall;
  if (restore) setSystem(restore);
  gps.begin(9600,SERIAL_8N1,TDeckPins::gpsRx,TDeckPins::gpsTx);
  Serial.printf("CLOCK T-Deck restored=%lu mapping=%s; GPS UART RX=%d TX=%d\n",
                (unsigned long)restore,mapped?"yes":"no",TDeckPins::gpsRx,TDeckPins::gpsTx);
  return true;
}
uint32_t rtcEpoch() { return clockValid ? (uint32_t)time(nullptr) : 0; }
void rtcSetEpoch(uint32_t e) { setSystem(e); }

bool rtcPollAuthoritative(uint32_t &wallEpoch) {
  uint32_t utc=0,now=millis(); const char *source=nullptr;
  if (pollGps(utc)) source="GPS";
  else if (!gpsAccepted) {
    pollNtp(now);
    if (ntpSynced) { utc=(uint32_t)time(nullptr); ntpSynced=false; source="NTP"; }
  }
  if (!source || !validEpoch(utc)) return false;
  if ((!strcmp(source,"GPS")&&gpsAccepted)||(!strcmp(source,"NTP")&&ntpAccepted)) return false;
  if (!strcmp(source,"NTP")) { WiFi.disconnect(false,false); WiFi.mode(WIFI_OFF); wifiStartedAt=0; }
  else if (wifiStartedAt) { WiFi.disconnect(false,false); WiFi.mode(WIFI_OFF); wifiStartedAt=0; }
  if (sourceDelivered && !strcmp(source,"NTP")) return false; // GPS may supersede NTP, not vice versa.

  uint32_t currentWall=ntpStarted && validEpoch(wallBeforeNtp)
                         ? wallBeforeNtp+(now-ntpStartedAt)/1000 : rtcEpoch();
  if (mapped) {
    uint32_t delta=utc>=savedUtc?utc-savedUtc:0;
    if (utc<savedUtc || delta>MAX_SOURCE_JUMP) {
      Serial.printf("CLOCK %s rejected saved delta=%lu; no offline catch-up\n",source,(unsigned long)delta);
      wallEpoch=currentWall;
      wallOffset=(int32_t)((int64_t)currentWall-utc);
    } else wallEpoch=wallFromUtc(utc);
  } else if (validEpoch(currentWall) && hadPriorWall) {
    // One-time legacy migration: preserve the displayed local wall clock and
    // establish its UTC offset. There is no sound basis for guessing how much
    // of the difference was timezone versus powered-off time.
    wallEpoch=currentWall;
    wallOffset=(int32_t)((int64_t)wallEpoch-utc);
    Serial.printf("CLOCK %s established wall/UTC mapping; legacy offline interval not guessed\n",source);
  } else {
    wallOffset=0; wallEpoch=utc;
  }
  mapped=true; authoritative=true; sourceDelivered=true;
  if (!strcmp(source,"GPS")) gpsAccepted=true; else ntpAccepted=true;
  sourceUtc=utc; sourceAtMs=now; savedUtc=utc; savedWall=wallEpoch;
  setSystem(wallEpoch); persistNow(wallEpoch,utc);
  Serial.printf("CLOCK authoritative=%s utc=%lu wall=%lu offset=%ld gps-data=%d\n",source,
                (unsigned long)utc,(unsigned long)wallEpoch,(long)wallOffset,gpsSeen?1:0);
  return true;
}
bool rtcAuthoritative() { return authoritative; }
void rtcPersistReference(uint32_t wallEpoch) {
  if (!validEpoch(wallEpoch) || millis()-lastPersistMs<PERSIST_MS) return;
  persistNow(wallEpoch,authoritative?currentUtc():0);
}
void rtcManualClockSet(uint32_t wallEpoch) {
  uint32_t utc=currentUtc();
  if (authoritative && validEpoch(utc)) {
    wallOffset=(int32_t)((int64_t)wallEpoch-utc);
    savedWall=wallEpoch; persistNow(wallEpoch,utc);
  } else {
    // A manual correction made before a source arrives breaks the pairing with
    // the old UTC snapshot. Invalidate only the separate clock mapping; the pet
    // save and its schema remain untouched.
    mapped=false; savedUtc=0; savedWall=wallEpoch; hadPriorWall=true;
    Preferences p; p.begin("tp-clock",false);
    p.putUInt("wall",wallEpoch); p.putBool("mapped",false); p.remove("utc"); p.remove("offset");
    p.end(); lastPersistMs=millis();
  }
}
bool batBegin() { return false; }
void pmuEnablePanel() {} // Board::begin owns peripheral power.
int batPercent() { return -1; }
bool batCharging() { return false; } // unavailable; consult powerStatusKnown()
int batMillivolts() { return 0; }
bool usbPresent() { return false; } // conservative brightness policy, not a measurement
void pwrSetup() {}
bool pwrShortPressed() { return false; }
bool powerStatusKnown() { return false; }
#endif
