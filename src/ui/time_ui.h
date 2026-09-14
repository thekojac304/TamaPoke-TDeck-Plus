#pragma once
#include <stdint.h>
inline void timeFieldsFromEpoch(uint32_t epoch,int &hour,int &minute) {
  hour=(int)((epoch/3600)%24); minute=(int)((epoch/60)%60);
}
