#pragma once
#include "board_select.h"
#if TAMAPOKE_TDECK_PLUS
#include "tdeck_plus/display_power_policy.h"
#endif
#include <Arduino_GFX_Library.h>
#include <FS.h>
namespace Board {
struct TouchSample { bool pressed; int16_t x, y; };
struct AuxiliaryInput { bool centerPressed; bool activity; };
Arduino_Canvas *canvas();
bool begin(Arduino_Canvas *canvas);
bool sampleTouch(TouchSample &sample, bool wasPressed);
bool sampleAuxiliaryInput(AuxiliaryInput &sample);
void brightness(uint8_t value);
uint8_t displayTimeoutIndex();
uint32_t displayTimeoutMs();
const char *displayTimeoutLabel();
void setDisplayTimeoutIndex(uint8_t index);
bool mountStorage();
fs::FS &storage();
uint64_t cardSize();
uint64_t totalBytes();
uint64_t usedBytes();
// Display and every SD operation run synchronously on the Arduino loop task.
// No display DMA, worker task, or ISR may access this bus.
}
