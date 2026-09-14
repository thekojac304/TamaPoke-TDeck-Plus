#pragma once
#include <stdint.h>

namespace TpTime {
constexpr uint32_t MIN_EPOCH=1704067200UL,MAX_EPOCH=4102444799UL;
constexpr uint32_t MAX_DELTA=10UL*366*86400;
enum Source:uint8_t { LOCAL=0,GPS,WIFI,MANUAL };

inline bool validEpoch(uint32_t epoch) { return epoch>=MIN_EPOCH&&epoch<=MAX_EPOCH; }
inline bool validDelta(uint32_t acquired,uint32_t saved) {
  return validEpoch(acquired)&&validEpoch(saved)&&acquired>=saved&&acquired-saved<=MAX_DELTA;
}

struct SyncPolicy {
  Source source=LOCAL;
  bool delivered=false,catchupConsumed=false,promptConsumed=false;
  bool accept(Source next,uint32_t epoch) {
    if (delivered||next==LOCAL||next==MANUAL||!validEpoch(epoch)) return false;
    delivered=true; source=next; return true;
  }
  void setManual() { source=MANUAL; }
  bool consumeCatchup() {
    if (!delivered||catchupConsumed) return false;
    catchupConsumed=true; return true;
  }
  bool consumeFailurePrompt(bool failed) {
    if (!failed||delivered||promptConsumed) return false;
    promptConsumed=true; return true;
  }
  static bool wifiFallbackAllowed(bool enabled,bool configured) { return enabled&&configured; }
};

// A prepared manual catch-up is deliberately a two-step, one-shot operation.
// The UI must show the exact reference/new pair before consume() can succeed.
struct ManualCatchupPolicy {
  uint32_t reference=0,next=0,elapsed=0;
  bool pending=false,consumed=false;
  bool prepare(uint32_t saved,uint32_t entered) {
    cancel();
    if (!validDelta(entered,saved)||entered==saved) return false;
    reference=saved; next=entered; elapsed=entered-saved; pending=true;
    return true;
  }
  bool consume(uint32_t saved,uint32_t entered) {
    if (!pending||consumed||saved!=reference||entered!=next) return false;
    pending=false; consumed=true; return true;
  }
  void cancel() { reference=next=elapsed=0; pending=false; consumed=false; }
};
}
