#include "../board_select.h"
#if TAMAPOKE_TDECK_PLUS
#include "../../../audio.h"
// Deliberately silent: no peripheral setup, task, or NVS writes.
void audioBegin() {}
void sfxPlay(uint8_t) {}
void audioSetEnabled(bool) {}
bool audioEnabled() { return false; }
void audioSetSleeping(bool) {}
#endif
