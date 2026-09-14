#pragma once
#include <stdint.h>

namespace TpDisplay {
constexpr uint8_t TIMEOUT_COUNT=6,DEFAULT_TIMEOUT=2;
constexpr uint32_t TIMEOUT_MS[TIMEOUT_COUNT]={30000UL,60000UL,120000UL,300000UL,600000UL,0};
inline uint8_t validIndex(uint8_t index) { return index<TIMEOUT_COUNT?index:DEFAULT_TIMEOUT; }
inline uint32_t timeoutMs(uint8_t index) { return TIMEOUT_MS[validIndex(index)]; }
inline const char *timeoutLabel(uint8_t index) {
  static const char *const labels[TIMEOUT_COUNT]={"30 SEC","1 MIN","2 MIN","5 MIN","10 MIN","NEVER"};
  return labels[validIndex(index)];
}
inline bool shouldTurnOff(uint32_t idleMs,uint8_t index) {
  uint32_t limit=timeoutMs(index); return limit&&idleMs>=limit;
}
inline bool shouldDim(uint32_t idleMs,uint8_t index) {
  uint32_t limit=timeoutMs(index); return limit&&idleMs<limit&&limit-idleMs<=15000UL;
}
}
