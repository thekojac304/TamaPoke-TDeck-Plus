#pragma once
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
namespace TDeckPins {
constexpr int power = 10, backlight = 42;
constexpr int sck = 40, mosi = 41, miso = 38;
constexpr int lcdCs = 12, lcdDc = 11, sdCs = 39, radioCs = 9;
constexpr int sda = 18, scl = 8, touchInt = 16;
constexpr int speakerBck = 7, speakerWs = 5, speakerDout = 6;
}
