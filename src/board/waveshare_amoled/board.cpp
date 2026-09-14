#include "../board.h"
#if !TAMAPOKE_TDECK_PLUS
#include <Wire.h>
#include <SD_MMC.h>
#include <TouchDrvCSTXXX.hpp>
#include "../../../rtcbat.h"
namespace Board {
static Arduino_CO5300 *panel = nullptr;
static TouchDrvCST92xx touch;
static volatile bool gTouchIrq = false;
static void IRAM_ATTR touchIsr() { gTouchIrq = true; }
Arduino_Canvas *canvas() {
static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
 LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
panel = new Arduino_CO5300(
 bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
 return new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
}
bool begin(Arduino_Canvas *gfx) {
 Wire.begin(IIC_SDA, IIC_SCL);
 Wire.setTimeOut(50);
 pmuEnablePanel();
 bool ok = gfx->begin(80000000);
 if (!ok) Serial.println("gfx->begin() fallo");
 panel->setBrightness(180);
  touch.setPins(TP_RESET, TP_INT);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {  // a veces falla al primer intento
    touchOk = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);
    if (!touchOk) delay(150);
  }
  if (!touchOk) Serial.println("CST9217 no detectado");
  // begin() deja el chip en modo comando (lee la identidad y no sale);
  // hace falta un reset por hardware para que vuelva a reportar toques
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);  // el panel esta montado girado 180 grados
  // INT activo-bajo: salta cuando hay datos. Gatea las lecturas I2C (ver loop)
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);

 return ok;
}
bool sampleTouch(TouchSample &s, bool wasPressed) {
 if (!gTouchIrq && !wasPressed) return false;
 gTouchIrq = false;
 Wire.beginTransmission(0x5A);
 if (Wire.endTransmission() != 0) return false;
 s.pressed = touch.getPoint(&s.x, &s.y, 1) > 0;
 return true;
}
bool sampleAuxiliaryInput(AuxiliaryInput &) { return false; }
uint8_t displayTimeoutIndex() { return 5; }
uint32_t displayTimeoutMs() { return 0; }
const char *displayTimeoutLabel() { return "NEVER"; }
void setDisplayTimeoutIndex(uint8_t) {}
void brightness(uint8_t value) { panel->setBrightness(value); }
bool mountStorage() {
 SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
 return SD_MMC.begin("/sdcard", true, true);
}
fs::FS &storage() { return SD_MMC; }
uint64_t cardSize() { return SD_MMC.cardSize(); }
uint64_t totalBytes() { return SD_MMC.totalBytes(); }
uint64_t usedBytes() { return SD_MMC.usedBytes(); }
}
#endif
