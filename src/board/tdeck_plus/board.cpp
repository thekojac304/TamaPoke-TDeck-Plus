#include "../board.h"
#if TAMAPOKE_TDECK_PLUS
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TouchDrvGT911.hpp>
#include <Preferences.h>
#include "st7789_init.h"
#include "display_power_policy.h"
namespace Board {
using namespace TDeckPins;
static TouchDrvGT911 touch;
static bool touchOk = false, storageOk = false;
static uint8_t timeoutIndex=TpDisplay::DEFAULT_TIMEOUT;
static constexpr uint8_t KEYBOARD_ADDRESS=0x55;
// Match LILYGO's SPI mode 0. Arduino_ST7789 otherwise overrides it to mode 3.
class PlusPanel : public Arduino_ST7789 {
public:
 PlusPanel(Arduino_DataBus *bus) : Arduino_ST7789(bus, -1, 1, true, 240, 320,
   0,0,0,0, plusInit, sizeof(plusInit)) {}
 bool begin(int32_t speed) override {
  _override_datamode = SPI_MODE0;
  return Arduino_TFT::begin(speed);
 }
};
Arduino_Canvas *canvas() {
 // Function locals avoid cross-translation-unit initialization order dependencies.
 static Arduino_HWSPI bus(lcdDc, lcdCs, sck, mosi, miso, &SPI, true);
 static PlusPanel panel(&bus);
 return new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, &panel);
}
void brightness(uint8_t value) {
 static uint8_t level = 0;
 uint8_t target = value ? 1 + (uint16_t(value) * 15 / 255) : 0;
 Serial.printf("DISPLAY brightness value=%u pulse_step=%u\n",value,target);
 if (!target) {
  digitalWrite(backlight, LOW); delay(3); level = 0; return;
 }
 if (!level) { digitalWrite(backlight, HIGH); delayMicroseconds(30); level = 16; }
 int pulses = (16 + level - target) % 16;
 for (int i=0; i<pulses; ++i) { digitalWrite(backlight, LOW); digitalWrite(backlight, HIGH); }
 level = target;
}
bool begin(Arduino_Canvas *gfx) {
 pinMode(power, OUTPUT); digitalWrite(power, HIGH);
 for (int cs : {lcdCs, sdCs, radioCs}) { pinMode(cs, OUTPUT); digitalWrite(cs, HIGH); }
 pinMode(backlight, OUTPUT); brightness(0);
 for (int pin : {trackCenter,trackUp,trackRight,trackDown,trackLeft}) pinMode(pin,INPUT_PULLUP);
 Preferences prefs; prefs.begin("tp-display",true);
 timeoutIndex=TpDisplay::validIndex(prefs.getUChar("timeout",TpDisplay::DEFAULT_TIMEOUT)); prefs.end();
 delay(500);
 Serial.printf("BOARD T-Deck Plus flash=%u PSRAM=%u power=ON\n", ESP.getFlashChipSize(), ESP.getPsramSize());
 Wire.begin(sda, scl); Wire.setTimeOut(50);
 // Arduino_HWSPI::begin initializes SPI with the mapped pins exactly once.
 // SD.begin calls SPI.begin again, which is an idempotent no-op in core 3.3.10.
 bool ok = gfx->begin(40000000);
 Serial.printf("DISPLAY ST7789 320x240 RGB565 SPI=40000000 %s\n", ok ? "OK" : "FAIL");
 if (!ok) return false;
 gfx->fillScreen(RGB565_BLACK); gfx->flush(); brightness(180);
 touch.setPins(-1, touchInt);
 for (int i=0; i<3 && !touchOk; ++i) {
  touchOk = touch.begin(Wire, GT911_SLAVE_ADDRESS_L);
  if (!touchOk) delay(150);
 }
 touch.setMaxCoordinates(320,240);
 touch.setSwapXY(true); touch.setMirrorXY(false,true);
 Serial.printf("TOUCH GT911 0x%02X swapXY=1 mirrorY=1 %s\n", touch.getAddress(), touchOk ? "OK" : "FAIL");
 return true;
}
bool sampleAuxiliaryInput(AuxiliaryInput &s) {
 static bool initialized=false;
 static uint8_t prior=0x1F;
 const int pins[5]={trackUp,trackRight,trackDown,trackLeft,trackCenter};
 uint8_t state=0;
 for (int i=0;i<5;++i) if (digitalRead(pins[i])!=LOW) state|=1u<<i;
 s.centerPressed=(state&(1u<<4))==0; s.key=0;
 s.activity=initialized&&((state^prior)&0x0F)!=0;
 if (Wire.requestFrom(KEYBOARD_ADDRESS,(uint8_t)1)==1) {
  s.key=(uint8_t)Wire.read(); if (s.key) s.activity=true;
 }
 prior=state; initialized=true; return true;
}
uint8_t displayTimeoutIndex() { return timeoutIndex; }
uint32_t displayTimeoutMs() { return TpDisplay::timeoutMs(timeoutIndex); }
const char *displayTimeoutLabel() { return TpDisplay::timeoutLabel(timeoutIndex); }
void setDisplayTimeoutIndex(uint8_t index) {
 timeoutIndex=TpDisplay::validIndex(index);
 Preferences prefs; prefs.begin("tp-display",false); prefs.putUChar("timeout",timeoutIndex); prefs.end();
}
bool sampleTouch(TouchSample &s, bool) {
 if (!touchOk) return false;
 // SensorLib 0.4.1 reports zero points even when no new report is ready.
 // Preserve the last state in that case; a ready zero-point report is release.
 static TouchSample last{};
 Wire.beginTransmission(touch.getAddress());
 Wire.write(0x81); Wire.write(0x4E);
 if (Wire.endTransmission(false) != 0) return false;
 if (Wire.requestFrom(touch.getAddress(), (uint8_t)1) != 1) return false;
 uint8_t status = Wire.read();
 if (!(status & 0x80)) { s = last; return true; }
 if ((status & 0x0F) > 5) return false;
 s.pressed = touch.getPoint(&s.x, &s.y, 1) > 0;
 if ((status & 0x0F) && !s.pressed) return false; // read failed, not a release
 if (s.pressed) {
  s.x = constrain(s.x,0,LCD_WIDTH-1); s.y = constrain(s.y,0,LCD_HEIGHT-1);
#ifdef TAMAPOKE_TOUCH_DEBUG
  Serial.printf("TOUCH %d %d\n", s.x, s.y);
#endif
 }
 last = s;
 return true;
}
bool mountStorage() {
 digitalWrite(lcdCs,HIGH); digitalWrite(radioCs,HIGH); digitalWrite(sdCs,HIGH);
 storageOk = SD.begin(sdCs, SPI, 800000, "/sdcard", 5, false);
 Serial.printf("SD SPI CS=39 Hz=800000 format=NO %s\n", storageOk ? "OK" : "FAIL");
 return storageOk;
}
fs::FS &storage() { return SD; }
uint64_t cardSize() { return storageOk ? SD.cardSize() : 0; }
uint64_t totalBytes() { return storageOk ? SD.totalBytes() : 0; }
uint64_t usedBytes() { return storageOk ? SD.usedBytes() : 0; }
}
#endif
