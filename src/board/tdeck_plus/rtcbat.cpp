#include "../board_select.h"
#if TAMAPOKE_TDECK_PLUS
#include "../../../rtcbat.h"
#include <sys/time.h>
// Every boot seeds from existing savedEpoch in setup. Never infer powered-off time.
static bool clockValid = false;
bool rtcBegin() { clockValid = false; Serial.println("CLOCK saved-time seed; powered-off elapsed time unavailable"); return true; }
uint32_t rtcEpoch() { return clockValid ? (uint32_t)time(nullptr) : 0; }
void rtcSetEpoch(uint32_t e) { timeval tv{(time_t)e,0}; clockValid = settimeofday(&tv,nullptr) == 0; }
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
