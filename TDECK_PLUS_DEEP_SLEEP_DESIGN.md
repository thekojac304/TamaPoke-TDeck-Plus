# T-Deck Plus future deep-sleep design (analysis only)

Deep sleep is deliberately not implemented in this pass. The current display
timeout is the safe first stage: game/time tasks keep running while only the LCD
backlight and rendering are stopped.

## Wake inputs

ESP32-S3 GPIO0 through GPIO21 are RTC-capable, so every current trackball input
is technically usable for deep-sleep wake: center GPIO0 and directions GPIO1,
GPIO2, GPIO3, and GPIO15. Prefer center-only wake with EXT1 active-low to avoid
pocket wakeups. LILYGO's own T-Deck deep-sleep example already uses GPIO0 this
way. GPIO0 is also the BOOT strapping pin: deep-sleep wake is feasible, but the
implementation must wait for release/debounce on boot and hardware testing must
cover a held button during reset or cold power-up, where bootloader entry is a
risk. Touch IRQ GPIO16 is RTC-capable too, but should not be enabled because the
product requirement intentionally avoids incidental touch wake.

References: [LILYGO T-Deck UnitTest deep-sleep code](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/UnitTest.ino),
[ESP-IDF ESP32-S3 sleep modes](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/api-reference/system/sleep_modes.html).

## Retained state and one-shot catch-up

Before sleeping, flush `Pet`, persist the current `tp-clock` wall/UTC pair, then
store a small versioned `RTC_DATA_ATTR` record containing a magic value, CRC,
trusted wall epoch at sleep entry, expected source/mapping metadata, and a unique
sleep/catch-up sequence token. On wake, require a deep-sleep wake cause and a
valid record. Use the ESP32-S3 RTC-maintained system time to calculate a sane,
forward delta, then feed the resulting wall epoch to the existing `syncClock()`
path. Mark/clear the sequence token before normal operation so a reset cannot
apply it twice. The existing `syncClock()` rules and two-week cap remain the sole
gameplay progression implementation.

RTC RAM retention distinguishes deep sleep while battery remains connected from
true power removal: loss or corruption of the marker must fall back to GPS,
NTP, or confirmed manual catch-up. A timer wake may be enabled independently,
but the configured timer duration alone should not be treated as elapsed time
after an unrelated reset.

## Launcher interaction and principal risk

This repository contains no Launcher handoff contract. Its build uses OTA slot
`app0` at 0x10000 with `otadata`; deep-sleep wake follows the normal ROM/bootloader
reset path and therefore starts whichever partition the bootloader considers
active. If an external Launcher starts TamaPoke transiently without making its
OTA slot the active boot partition, wake can return to Launcher instead of the
game.

Before implementing deep sleep, inspect the exact Launcher build and verify the
running partition, configured boot partition, and `otadata` behavior on real
hardware. The robust choices are a Launcher-aware RTC resume marker that relaunches
TamaPoke, or a documented Launcher API that makes the selected app persistent.
Do not make TamaPoke rewrite the boot partition without that contract. Test center
wake, timer wake, full power removal, reset while GPIO0 is held, and both direct
flash and Launcher-installed boot flows.
