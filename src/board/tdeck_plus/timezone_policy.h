#pragma once
#include <stdint.h>
#include <string.h>

namespace TpTimezone {
enum Region:uint8_t { REGION_UTC=0,NORTH_AMERICA,SOUTH_AMERICA,EUROPE,AFRICA,ASIA,OCEANIA,REGION_COUNT };

// Values 0..4 are unchanged so firmware 1.16 NVS values migrate in place.
enum Preset:uint8_t {
  UTC=0,US_EASTERN,US_CENTRAL,US_MOUNTAIN,US_PACIFIC,
  NEWFOUNDLAND,ATLANTIC,ARIZONA,ALASKA,HAWAII,
  BRASILIA,BUENOS_AIRES,SANTIAGO,BOGOTA,LIMA,
  LONDON,DUBLIN,LISBON,CENTRAL_EUROPE,EASTERN_EUROPE,HELSINKI,ATHENS,MOSCOW,
  CASABLANCA,WEST_AFRICA,CENTRAL_AFRICA,SOUTH_AFRICA,EAST_AFRICA,
  ISRAEL,DUBAI,INDIA,PAKISTAN,BANGLADESH,THAILAND_VIETNAM,CHINA,JAPAN,KOREA,SINGAPORE,
  JAKARTA,MAKASSAR,JAYAPURA,
  PERTH,ADELAIDE,DARWIN,BRISBANE,SYDNEY,MELBOURNE,HOBART,AUCKLAND,CHATHAM,COUNT
};
enum DstRule:uint8_t { NO_DST=0,NORTH_AMERICAN_DST,EUROPEAN_DST,AUSTRALIAN_DST,NEW_ZEALAND_DST,CHATHAM_DST,CHILE_DST,ISRAEL_DST };
struct Definition {
  Preset preset; const char *id,*name,*shortName; Region region; const char *posix;
  int16_t standardMinutes,daylightMinutes; DstRule dst;
};
#define Z(p,id,n,s,r,t,o,d,rule) {p,id,n,s,r,t,o,d,rule}
static constexpr Definition PRESETS[COUNT]={
  Z(UTC,"utc","UTC","UTC",REGION_UTC,"UTC0",0,0,NO_DST),
  Z(US_EASTERN,"us-eastern","Eastern","EASTERN",NORTH_AMERICA,"EST5EDT,M3.2.0/2,M11.1.0/2",-300,-240,NORTH_AMERICAN_DST),
  Z(US_CENTRAL,"us-central","Central","CENTRAL",NORTH_AMERICA,"CST6CDT,M3.2.0/2,M11.1.0/2",-360,-300,NORTH_AMERICAN_DST),
  Z(US_MOUNTAIN,"us-mountain","Mountain","MOUNTAIN",NORTH_AMERICA,"MST7MDT,M3.2.0/2,M11.1.0/2",-420,-360,NORTH_AMERICAN_DST),
  Z(US_PACIFIC,"us-pacific","Pacific","PACIFIC",NORTH_AMERICA,"PST8PDT,M3.2.0/2,M11.1.0/2",-480,-420,NORTH_AMERICAN_DST),
  Z(NEWFOUNDLAND,"ca-newfoundland","Newfoundland","NEWFOUNDLAND",NORTH_AMERICA,"NST3:30NDT,M3.2.0/2,M11.1.0/2",-210,-150,NORTH_AMERICAN_DST),
  Z(ATLANTIC,"ca-atlantic","Atlantic","ATLANTIC",NORTH_AMERICA,"AST4ADT,M3.2.0/2,M11.1.0/2",-240,-180,NORTH_AMERICAN_DST),
  Z(ARIZONA,"us-arizona","Arizona","ARIZONA",NORTH_AMERICA,"MST7",-420,-420,NO_DST),
  Z(ALASKA,"us-alaska","Alaska","ALASKA",NORTH_AMERICA,"AKST9AKDT,M3.2.0/2,M11.1.0/2",-540,-480,NORTH_AMERICAN_DST),
  Z(HAWAII,"us-hawaii","Hawaii","HAWAII",NORTH_AMERICA,"HST10",-600,-600,NO_DST),
  Z(BRASILIA,"br-brasilia","Brasilia","BRASILIA",SOUTH_AMERICA,"<-03>3",-180,-180,NO_DST),
  Z(BUENOS_AIRES,"ar-buenos-aires","Buenos Aires","BUENOS AIRES",SOUTH_AMERICA,"<-03>3",-180,-180,NO_DST),
  Z(SANTIAGO,"cl-santiago","Santiago","SANTIAGO",SOUTH_AMERICA,"<-04>4<-03>,M9.1.6/24,M4.1.6/24",-240,-180,CHILE_DST),
  Z(BOGOTA,"co-bogota","Bogota","BOGOTA",SOUTH_AMERICA,"<-05>5",-300,-300,NO_DST),
  Z(LIMA,"pe-lima","Lima","LIMA",SOUTH_AMERICA,"<-05>5",-300,-300,NO_DST),
  Z(LONDON,"gb-london","London","LONDON",EUROPE,"GMT0BST,M3.5.0/1,M10.5.0/2",0,60,EUROPEAN_DST),
  Z(DUBLIN,"ie-dublin","Dublin","DUBLIN",EUROPE,"GMT0IST,M3.5.0/1,M10.5.0/2",0,60,EUROPEAN_DST),
  Z(LISBON,"pt-lisbon","Lisbon","LISBON",EUROPE,"WET0WEST,M3.5.0/1,M10.5.0/2",0,60,EUROPEAN_DST),
  Z(CENTRAL_EUROPE,"eu-central","Central Europe","CENTRAL EUROPE",EUROPE,"CET-1CEST,M3.5.0/2,M10.5.0/3",60,120,EUROPEAN_DST),
  Z(EASTERN_EUROPE,"eu-eastern","Eastern Europe","EASTERN EUROPE",EUROPE,"EET-2EEST,M3.5.0/3,M10.5.0/4",120,180,EUROPEAN_DST),
  Z(HELSINKI,"fi-helsinki","Helsinki","HELSINKI",EUROPE,"EET-2EEST,M3.5.0/3,M10.5.0/4",120,180,EUROPEAN_DST),
  Z(ATHENS,"gr-athens","Athens","ATHENS",EUROPE,"EET-2EEST,M3.5.0/3,M10.5.0/4",120,180,EUROPEAN_DST),
  Z(MOSCOW,"ru-moscow","Moscow","MOSCOW",EUROPE,"MSK-3",180,180,NO_DST),
  Z(CASABLANCA,"ma-casablanca","Casablanca","CASABLANCA",AFRICA,"<+01>-1",60,60,NO_DST),
  Z(WEST_AFRICA,"af-west","West Africa","WEST AFRICA",AFRICA,"WAT-1",60,60,NO_DST),
  Z(CENTRAL_AFRICA,"af-central","Central Africa","CENTRAL AFRICA",AFRICA,"CAT-2",120,120,NO_DST),
  Z(SOUTH_AFRICA,"za-johannesburg","South Africa","SOUTH AFRICA",AFRICA,"SAST-2",120,120,NO_DST),
  Z(EAST_AFRICA,"af-east","East Africa","EAST AFRICA",AFRICA,"EAT-3",180,180,NO_DST),
  Z(ISRAEL,"il-jerusalem","Israel","ISRAEL",ASIA,"IST-2IDT,M3.4.4/26,M10.5.0/2",120,180,ISRAEL_DST),
  Z(DUBAI,"ae-dubai","Gulf / Dubai","DUBAI",ASIA,"<+04>-4",240,240,NO_DST),
  Z(INDIA,"in-india","India","INDIA",ASIA,"IST-5:30",330,330,NO_DST),
  Z(PAKISTAN,"pk-pakistan","Pakistan","PAKISTAN",ASIA,"PKT-5",300,300,NO_DST),
  Z(BANGLADESH,"bd-bangladesh","Bangladesh","BANGLADESH",ASIA,"<+06>-6",360,360,NO_DST),
  Z(THAILAND_VIETNAM,"sea-mainland","Thailand / Vietnam","THAILAND / VIETNAM",ASIA,"<+07>-7",420,420,NO_DST),
  Z(CHINA,"cn-china","China","CHINA",ASIA,"CST-8",480,480,NO_DST),
  Z(JAPAN,"jp-japan","Japan","JAPAN",ASIA,"JST-9",540,540,NO_DST),
  Z(KOREA,"kr-korea","Korea","KOREA",ASIA,"KST-9",540,540,NO_DST),
  Z(SINGAPORE,"sg-singapore","Singapore","SINGAPORE",ASIA,"SGT-8",480,480,NO_DST),
  Z(JAKARTA,"id-jakarta","Indonesia / Jakarta","JAKARTA",ASIA,"WIB-7",420,420,NO_DST),
  Z(MAKASSAR,"id-makassar","Indonesia / Makassar","MAKASSAR",ASIA,"WITA-8",480,480,NO_DST),
  Z(JAYAPURA,"id-jayapura","Indonesia / Jayapura","JAYAPURA",ASIA,"WIT-9",540,540,NO_DST),
  Z(PERTH,"au-perth","Perth","PERTH",OCEANIA,"AWST-8",480,480,NO_DST),
  Z(ADELAIDE,"au-adelaide","Adelaide","ADELAIDE",OCEANIA,"ACST-9:30ACDT,M10.1.0/2,M4.1.0/3",570,630,AUSTRALIAN_DST),
  Z(DARWIN,"au-darwin","Darwin","DARWIN",OCEANIA,"ACST-9:30",570,570,NO_DST),
  Z(BRISBANE,"au-brisbane","Brisbane","BRISBANE",OCEANIA,"AEST-10",600,600,NO_DST),
  Z(SYDNEY,"au-sydney","Sydney","SYDNEY",OCEANIA,"AEST-10AEDT,M10.1.0/2,M4.1.0/3",600,660,AUSTRALIAN_DST),
  Z(MELBOURNE,"au-melbourne","Melbourne","MELBOURNE",OCEANIA,"AEST-10AEDT,M10.1.0/2,M4.1.0/3",600,660,AUSTRALIAN_DST),
  Z(HOBART,"au-hobart","Tasmania / Hobart","HOBART",OCEANIA,"AEST-10AEDT,M10.1.0/2,M4.1.0/3",600,660,AUSTRALIAN_DST),
  Z(AUCKLAND,"nz-auckland","Auckland","AUCKLAND",OCEANIA,"NZST-12NZDT,M9.5.0/2,M4.1.0/3",720,780,NEW_ZEALAND_DST),
  Z(CHATHAM,"nz-chatham","Chatham Islands","CHATHAM",OCEANIA,"<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45",765,825,CHATHAM_DST)
};
#undef Z

static constexpr const char *REGION_LABELS[REGION_COUNT]={"UTC","NORTH AMERICA","SOUTH AMERICA","EUROPE","AFRICA","ASIA","OCEANIA"};
inline Preset valid(uint8_t value){ return value<COUNT?(Preset)value:UTC; }
inline const Definition &definition(Preset z){ return PRESETS[(uint8_t)valid((uint8_t)z)]; }
inline const char *id(Preset z){ return definition(z).id; }
inline const char *label(Preset z){ return definition(z).name; }
inline const char *shortLabel(Preset z){ return definition(z).shortName; }
inline const char *posix(Preset z){ return definition(z).posix; }
inline Region region(Preset z){ return definition(z).region; }
inline const char *regionLabel(Region r){ return REGION_LABELS[r<REGION_COUNT?r:REGION_UTC]; }
inline bool findId(const char *value,Preset &result){ if(value&&value[0]) for(uint8_t i=0;i<COUNT;++i) if(!strcmp(PRESETS[i].id,value)){ result=(Preset)i; return true; } result=UTC; return false; }
inline Preset fromId(const char *value){ Preset result=UTC; findId(value,result); return result; }
inline uint8_t countInRegion(Region r){ uint8_t n=0; for(uint8_t i=0;i<COUNT;++i) if(PRESETS[i].region==r) ++n; return n; }
inline Preset presetInRegion(Region r,uint8_t offset){ for(uint8_t i=0;i<COUNT;++i) if(PRESETS[i].region==r&&offset--==0) return (Preset)i; return UTC; }
inline uint8_t offsetInRegion(Preset z){ Region r=region(z); uint8_t offset=0; for(uint8_t i=0;i<(uint8_t)valid((uint8_t)z);++i) if(PRESETS[i].region==r) ++offset; return offset; }

inline int64_t daysFromCivil(int y,unsigned m,unsigned d){
  y-=m<=2; const int era=(y>=0?y:y-399)/400; const unsigned yoe=(unsigned)(y-era*400);
  const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
  return era*146097+(int)(yoe*365+yoe/4-yoe/100+doy)-719468;
}
inline unsigned weekday(int y,unsigned m,unsigned d){ int v=(int)((daysFromCivil(y,m,d)+4)%7); return (unsigned)(v<0?v+7:v); }
inline unsigned nthWeekday(int y,unsigned m,unsigned dow,unsigned nth){ return 1+((7+dow-weekday(y,m,1))%7)+(nth-1)*7; }
inline unsigned daysInMonth(int y,unsigned m){ static const uint8_t ds[12]={31,28,31,30,31,30,31,31,30,31,30,31}; return ds[m-1]+(m==2&&((y%4==0&&y%100!=0)||y%400==0)); }
inline unsigned lastWeekday(int y,unsigned m,unsigned dow){ unsigned last=daysInMonth(y,m); return last-(7+weekday(y,m,last)-dow)%7; }
inline uint32_t epoch(int y,unsigned m,unsigned d,unsigned hh=0,unsigned mm=0,unsigned ss=0){ return (uint32_t)(daysFromCivil(y,m,d)*86400+hh*3600+mm*60+ss); }
inline int yearAtUtc(uint32_t utc){ int y=1970; while(y<2105&&utc>=epoch(y+1,1,1)) ++y; return y; }
inline uint32_t localTransitionUtc(int y,unsigned m,unsigned d,unsigned hh,int offsetMinutes){ return (uint32_t)((int64_t)epoch(y,m,d,hh)-(int64_t)offsetMinutes*60); }
inline bool dstAtUtc(Preset z,uint32_t utc){
  const Definition &d=definition(z); if(d.dst==NO_DST) return false; int y=yearAtUtc(utc); uint32_t start,end;
  if(d.dst==NORTH_AMERICAN_DST){ start=localTransitionUtc(y,3,nthWeekday(y,3,0,2),2,d.standardMinutes); end=localTransitionUtc(y,11,nthWeekday(y,11,0,1),2,d.daylightMinutes); return utc>=start&&utc<end; }
  if(d.dst==EUROPEAN_DST){ start=epoch(y,3,lastWeekday(y,3,0),1); end=epoch(y,10,lastWeekday(y,10,0),1); return utc>=start&&utc<end; }
  if(d.dst==AUSTRALIAN_DST||d.dst==NEW_ZEALAND_DST||d.dst==CHATHAM_DST||d.dst==CHILE_DST){
    unsigned sm=d.dst==CHILE_DST?9:d.dst==AUSTRALIAN_DST?10:9;
    unsigned sd=d.dst==CHILE_DST?nthWeekday(y,9,0,1):d.dst==AUSTRALIAN_DST?nthWeekday(y,10,0,1):lastWeekday(y,9,0);
    start=localTransitionUtc(y,sm,sd,d.dst==CHILE_DST?0:2,d.standardMinutes);
    end=localTransitionUtc(y,4,nthWeekday(y,4,0,1),3,d.daylightMinutes); return utc>=start||utc<end;
  }
  start=localTransitionUtc(y,3,lastWeekday(y,3,5),2,d.standardMinutes);
  end=localTransitionUtc(y,10,lastWeekday(y,10,0),2,d.daylightMinutes); return utc>=start&&utc<end;
}
inline int32_t offsetSeconds(Preset z,int,uint32_t utc){ const Definition &d=definition(z); return (dstAtUtc(z,utc)?d.daylightMinutes:d.standardMinutes)*60; }
inline uint32_t localEpoch(Preset z,uint32_t utc){ return (uint32_t)((int64_t)utc+offsetSeconds(z,yearAtUtc(utc),utc)); }
}
