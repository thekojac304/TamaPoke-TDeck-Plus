#pragma once
#include "board_select.h"
#include <Arduino_GFX_Library.h>
#include <FS.h>
namespace Board {
struct TouchSample { bool pressed; int16_t x, y; };
Arduino_Canvas *canvas();
bool begin(Arduino_Canvas *canvas);
bool sampleTouch(TouchSample &sample, bool wasPressed);
void brightness(uint8_t value);
bool mountStorage();
fs::FS &storage();
uint64_t cardSize();
uint64_t totalBytes();
uint64_t usedBytes();
// Display and every SD operation run synchronously on the Arduino loop task.
// No display DMA, worker task, or ISR may access this bus.
}
