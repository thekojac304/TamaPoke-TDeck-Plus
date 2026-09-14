#pragma once
#include <Arduino.h>

// Board clock: PCF85063 on Waveshare; saved-time-seeded system clock on T-Deck.
bool rtcBegin(uint32_t savedWallEpoch = 0);
uint32_t rtcEpoch();             // segundos unix; 0 si el RTC no es valido
void rtcSetEpoch(uint32_t e);
// Non-blocking external-time support. Waveshare's battery RTC needs no polling.
// On T-Deck, a returned wall epoch is an absolute value suitable for syncClock().
bool rtcPollAuthoritative(uint32_t &wallEpoch);
bool rtcAuthoritative();
void rtcPersistReference(uint32_t wallEpoch);
void rtcManualClockSet(uint32_t wallEpoch);

// PMU AXP2101: estado de la bateria
bool batBegin();
void pmuEnablePanel();           // enciende BLDO1 (OLED VDD 3.3V); llamar antes de gfx->begin()
int batPercent();                // 0-100, -1 si no hay bateria conectada
bool batCharging();
int batMillivolts();               // tension de la bateria en mV, 0 si no hay lectura
bool usbPresent();

// boton PWR del AXP2101: pulsacion larga 4s = apagado fisico (RTC sigue vivo);
// la pulsacion corta la captura el firmware (pantalla on/off)
void pwrSetup();
bool pwrShortPressed();  // sondear en el loop

// False means USB/charging/battery readings are unavailable, not measured false.
bool powerStatusKnown();
