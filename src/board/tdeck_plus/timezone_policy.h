#pragma once
#include <stdint.h>

namespace TpTimezone {
enum Preset : uint8_t { UTC=0,US_EASTERN,US_CENTRAL,US_MOUNTAIN,US_PACIFIC,COUNT };

inline const char *label(Preset zone) {
  static const char *const labels[COUNT]={"UTC","US EASTERN","US CENTRAL","US MOUNTAIN","US PACIFIC"};
  return labels[zone<COUNT?zone:UTC];
}
inline const char *posix(Preset zone) {
  static const char *const values[COUNT]={
    "UTC0","EST5EDT,M3.2.0/2,M11.1.0/2","CST6CDT,M3.2.0/2,M11.1.0/2",
    "MST7MDT,M3.2.0/2,M11.1.0/2","PST8PDT,M3.2.0/2,M11.1.0/2"
  };
  return values[zone<COUNT?zone:UTC];
}
inline Preset valid(uint8_t value) { return value<COUNT?(Preset)value:UTC; }

inline int64_t daysFromCivil(int y,unsigned m,unsigned d) {
  y-=m<=2; const int era=(y>=0?y:y-399)/400; const unsigned yoe=(unsigned)(y-era*400);
  const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
  return era*146097+(int)(yoe*365+yoe/4-yoe/100+doy)-719468;
}
inline unsigned weekday(int y,unsigned m,unsigned d) {
  int64_t days=daysFromCivil(y,m,d); int value=(int)((days+4)%7); return (unsigned)(value<0?value+7:value);
}
inline unsigned nthSunday(int y,unsigned month,unsigned nth) {
  unsigned first=weekday(y,month,1); return 1+((7-first)%7)+(nth-1)*7;
}
inline uint32_t epoch(int y,unsigned m,unsigned d,unsigned hh=0,unsigned mm=0,unsigned ss=0) {
  return (uint32_t)(daysFromCivil(y,m,d)*86400+hh*3600+mm*60+ss);
}
inline int standardOffsetHours(Preset zone) {
  return zone==US_EASTERN?-5:zone==US_CENTRAL?-6:zone==US_MOUNTAIN?-7:zone==US_PACIFIC?-8:0;
}
inline bool usDstAtUtc(Preset zone,int year,uint32_t utc) {
  if (zone==UTC) return false;
  int stdHours=standardOffsetHours(zone);
  uint32_t start=epoch(year,3,nthSunday(year,3,2),(unsigned)(2-stdHours));
  uint32_t end=epoch(year,11,nthSunday(year,11,1),(unsigned)(2-(stdHours+1)));
  return utc>=start&&utc<end;
}
inline int offsetSeconds(Preset zone,int year,uint32_t utc) {
  return (standardOffsetHours(zone)+(usDstAtUtc(zone,year,utc)?1:0))*3600;
}
}
