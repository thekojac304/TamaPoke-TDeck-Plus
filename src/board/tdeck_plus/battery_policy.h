#pragma once
#include <stdint.h>

namespace TpBattery {
// T-Deck routes VBAT through a 100k/100k divider, so calibrated ADC millivolts
// are doubled. Values outside a plausible single-cell Li-ion range are absent
// or electrically invalid, not zero-percent batteries.
inline bool validMillivolts(int mv) { return mv>=3000&&mv<=4400; }
inline int estimatePercent(int mv) {
  struct Point { int mv,pct; };
  static constexpr Point curve[]={
    {3300,0},{3500,5},{3600,10},{3700,20},{3750,30},{3800,45},
    {3850,60},{3900,70},{4000,82},{4100,92},{4200,100}
  };
  if (mv<=curve[0].mv) return 0;
  for (unsigned i=1;i<sizeof(curve)/sizeof(curve[0]);++i) if (mv<=curve[i].mv) {
    const Point &a=curve[i-1],&b=curve[i];
    return a.pct+(mv-a.mv)*(b.pct-a.pct)/(b.mv-a.mv);
  }
  return 100;
}
inline int smoothMillivolts(int previous,int sample) {
  return previous>0?(previous*7+sample+4)/8:sample;
}
inline int stabilizePercent(int previous,int estimate) {
  if (previous<0) return estimate;
  int delta=estimate-previous;
  return delta>-2&&delta<2?previous:estimate;
}
}
