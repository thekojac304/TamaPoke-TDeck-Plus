# T-Deck Plus playable port and secondary UI

Branch: `codex/tdeck-plus-port`. This is a compile-tested bring-up candidate;
boot, panel colors, touch alignment, shared-bus reliability, and physical save
persistence still require a T-Deck Plus. No device was flashed during development.

## Scope and architecture

- `src/board/board.h` exposes canvas creation/startup, normalized touch,
  brightness, and generic filesystem access with capacity diagnostics.
- `src/board/board_select.h` selects T-Deck only with
  `TAMAPOKE_TDECK_PLUS=1`; Waveshare remains the default.
- `src/board/waveshare_amoled/` retains the original CO5300/QSPI, CST9217,
  SDMMC, ES8311 audio, PCF85063 and AXP2101 implementations. Root `audio.cpp`
  and `rtcbat.cpp` are compatibility placeholders; their public headers remain.
- `src/board/tdeck_plus/` owns all target pins, ST7789 initialization, GT911,
  pulse backlight, SPI SD, silent audio and system-clock fallback.
- `src/ui/layout*.h` defines geometry. `playable_landscape.h` renders the small
  screen; `actions_landscape.h` routes its rectangles into existing Pet methods.
  The same rectangles drive drawing and touch hit tests.
- `sdmon.cpp` uses `Board::storage()` without changing formats, paths, shiny
  fallback, thumbnail loading, or the PUT/LS/SDINFO protocol.
- `pet.cpp`, `pet.h`, `species.h`, `dex.h`, `i18n.cpp`, `i18n.h` and sprite assets
  remain unchanged. NVS namespaces, keys, schema, partition addresses, and
  deferred-save opportunities remain unchanged.

The compact home has a 208x174 scene, side needs bars, and four 80x40 care
buttons. Sprites use integer scaling, including 1x; PMD action bounds are fitted
individually and feet anchored within the scene. All 302 existing PMD packs fit.
The starter rows, egg taps/cracks, four food options, care actions, evolution,
farewell/runaway and release confirmations are available. Bath rendering still
completes the original delayed cleaning action. Long press is still three
seconds, and waking consumes the whole first gesture.

The header exposes Profile, Pokédex, and Settings touch targets. The original
gesture model is also restored: swipe up for the four-page card, down for
Settings, and horizontally for the Pokédex/page navigation. Landscape renderers
cover Profile, battle stats, all eight medals, progress, clock, language, sound,
nickname entry, Pokédex grid/detail, strength training/results, and the ball
minigame/results. The grid retains 16 entries per page. The touch keyboard keeps
all 30 original keys in a 6x5 layout. Strength training retains its 10-second
timer and original reward call. The ball game retains the 466x466 logical circle,
gravity, bounce, misses, scoring, timing, and reward call; drawing and touch are
transformed into a 240x240 square beside a landscape HUD.

## Reproducible builds

Tested dependencies:

| Dependency | Version |
|---|---|
| Arduino ESP32 core | 3.3.10 |
| GFX Library for Arduino | 1.6.7 |
| SensorLib | 0.4.1 |
| XPowersLib (Waveshare) | 0.3.3 |
| U8g2 | 2.36.19 |

Install the ESP32 board index and these versions with Arduino CLI, then run:

```text
python tools/build_firmware.py waveshare
python tools/build_firmware.py tdeck-plus
```

The script selects ESP32-S3, USB CDC, 16 MB flash, OPI PSRAM, and the existing
`app3M_fat9M_16MB` partition layout. The core's default flash setting is QIO 80 MHz;
LCD SPI is separately set to 40 MHz. The script preserves core USB build flags.
It stages only the sketch sources and `src/`, uses separate cache/output
directories, and rejects missing or empty binaries.
CI builds both targets and uploads separately named artifacts; the web installer
and its existing firmware were not modified.

The local tooling used for this development is ignored under `.build-tools/`:

```text
python tools/build_firmware.py tdeck-plus --cli .build-tools/cli/arduino-cli.exe --config-file .build-tools/arduino-cli.yaml
```

## Firmware to flash

Application: `C:/Dev/TamaPoke/build/tdeck-plus/TamaPoke.ino.bin`, address **0x10000**.
For first installation or a complete firmware update, flash these separate
pieces with esptool (substitute the actual serial port):

```text
esptool --chip esp32s3 --port COMx write-flash 0x0 build/tdeck-plus/TamaPoke.ino.bootloader.bin 0x8000 build/tdeck-plus/TamaPoke.ino.partitions.bin 0xe000 build/tdeck-plus/boot_app0.bin 0x10000 build/tdeck-plus/TamaPoke.ino.bin
```

These writes leave NVS at **0x9000–0xdfff** untouched. Do not erase flash or flash
`TamaPoke.ino.merged.bin` when preserving saves: the merged file spans the NVS
region. A card transfers sprites, not a pet; saves live on each device's flash.
No Launcher package or web-installer support is included.

## Hardware reference and bus ownership

Official LILYGO reference revision:
[`d0d202b456ded47dc4d4d63bb686ab4c5e120ab7`](https://github.com/Xinyuan-LilyGO/T-Deck/tree/d0d202b456ded47dc4d4d63bb686ab4c5e120ab7).
The Plus sequence comes from `lib/TFT_eSPI/TFT_Drivers/ST7789_Init.h`,
`INIT_SEQUENCE_2`; backlight/rotation from `examples/UnitTest/UnitTest.ino`;
touch mapping from `examples/Touchpad/Touchpad.ino`.

Power GPIO10 is enabled, and LCD/SD/radio CS lines are deselected before startup.
Arduino_HWSPI initializes SPI40/41/38. SD reuses that instance; its core
`SPI.begin()` is an idempotent no-op once initialized. Transactions set display
mode 0 at 40 MHz and SD at 800 kHz. All display, loader, thumbnail and serial
upload operations execute synchronously on the Arduino loop task. There is no
concurrent bus worker, ISR access, or asynchronous framebuffer transfer.
The radio CS9 remains high. Card mounting explicitly disables auto-format.

GT911 starts at 0x5D (SensorLib can discover 0x14), with swap XY and mirror Y,
then clamps into 320x240 coordinates. A report-ready check preserves held-finger
state between reports; a ready zero-point report produces release. No CST wake
transaction or hardware reset is copied into the GT911 acquisition path.

## Expected serial output

Open the USB serial monitor at 115200 and reset. USB serial never blocks waiting
for a host; messages emitted before connection may be missed.

```text
TamaPoke fw v1.16
BOARD T-Deck Plus flash=16777216 PSRAM=8388608 power=ON
DISPLAY ST7789 320x240 RGB565 SPI=40000000 OK
TOUCH GT911 0x5D swapXY=1 mirrorY=1 OK
SD SPI CS=39 Hz=800000 format=NO OK
SD montada: <card size> MB
miniaturas cargadas: <count> (<size> KB)
CLOCK saved-time seed; powered-off elapsed time unavailable
RTC sin hora: sembrado en <saved epoch or 1767225600> ...
cargado /mons/p001.bin (<size> KB)
```

The species load appears after hatching/loading a pet, not necessarily at fresh
boot. Missing cards report `FAIL` followed by the existing flash-sprite fallback
message. Missing thumbnails or packs retain their existing diagnostics.
`DISPLAY ... FAIL` stops at a serial fatal message, avoiding a null framebuffer.
`TOUCH ... FAIL` leaves the game rendering for diagnosis but touch is unavailable.
`HEALTH`, `STATS`, `SDINFO`, and `LS` remain available. Power readings are unknown:
battery is -1; extended health charging/USB fields are -1. For coordinate logs,
rebuild with `--touch-debug` and look for `TOUCH x y` on new pressed reports.

## Temporary limitations

- The ESP32 system clock is the active game clock. A non-blocking GPS parser first
  accepts checksum-valid RMC or ZDA UTC (date/time does not require a positional
  fix). After 45 seconds, NTP is tried only when the ESP32 already has stored
  station credentials; connection and synchronization have bounded timeouts.
  Manual Set Time remains available and the settings screen shows `TIME: LOCAL`
  until an authoritative source has been accepted.
- The game still displays its historical local-wall-clock-shaped epoch. The
  separate `tp-clock` NVS namespace pairs that wall value with authoritative UTC
  and an offset. The first sync of a legacy save preserves its displayed clock
  and deliberately does not guess whether the old difference was timezone or
  powered-off time. Later boots use the paired UTC references for one absolute
  `syncClock()` call; negative and over-ten-year source jumps are not applied,
  while the existing game logic retains its two-week progression cap.
- The CPU keeps running while dimmed. No deep sleep or physical power-key
  semantics are emulated. Battery percentage, voltage, charging and USB detection
  are unavailable. Brightness uses the official 16-level pulse scheme.
- Speaker output uses the existing synthesized 16 kHz PCM effects on I2S BCK 7,
  WS 5 and DOUT 6. Effect bursts end with zero samples and release I2S for idle
  silence; initialization failure degrades to silent operation. There is no
  microphone support. Physical keyboard, trackball, LoRa, BLE, general networking
  and launcher work remain out of scope.
- Long labels are clipped to their reserved rectangles when a translation cannot
  fit at the smallest supported font. Ceremony/evolution effects remain simplified
  on the compact scene; the engine's timing and outcomes are retained.
- SD is deliberately conservative at 800 kHz, so large sprite loads and USB
  uploads may pause animation. Malformed oversized external sprites are not
  resampled; use the existing supplied packs for bring-up.

## Physical test sequence

1. Prepare FAT32 SD with unchanged `tools/sdcard/mons/` contents at `/mons/`.
   Flash the split T-Deck files above. Capture a USB boot log and verify flash,
   PSRAM, display, touch and SD diagnostics.
2. Verify screen orientation, RGB colors, readable text, and all screen edges.
   With a touch-debug build, check corners, center, drags and release. Verify a
   held touch produces one action on release; long press opens release confirmation.
3. On fresh NVS, try each starter on separate test runs; hatch with three taps.
   Check normal sprite animation, food options, Play, Light, Bath and petting.
   Bath must complete and clear poops; sleeping must gate other care actions.
4. Leave idle for dimming (90 seconds and five minutes). First touch must only
   wake; the next touch must act. Hold/drag through wake and confirm no care action
   leaks through. Keep checking health logs for stalled loops.
5. Confirm/decline evolution and farewell; cancel/confirm release on a disposable
   test pet. Check the ceremony completes and returns to an egg. Existing debug
   commands can accelerate testing but deliberately change the test pet.
6. Exercise large packs (especially Gyarados), shiny fallback, `LS`, `SDINFO`, and
   a known asset PUT while rendering. Check for corruption, failed reads and
   clipping. Repeat without SD to verify flash sprites and clear missing-art text.
7. Perform a care action that saves, reset, and verify state. Let deferred saves
   flush while dimmed, then use the physical switch and verify again.
8. Repeat cold boot on battery, with/without USB monitor and with/without SD.
   Finally smoke-test the original Waveshare binary on its own board.
9. With Sound on, verify the startup jingle, food/care sounds, both minigames,
   rapid repeated effects and sleep gating. Toggle Sound off during an effect and
   confirm playback stops; leave the device idle and listen for clicks or hiss.
10. Boot outdoors and confirm a `CLOCK authoritative=GPS` log can appear before a
   position fix. Power off for a measured interval, reboot, and verify exactly one
   catch-up. Repeat indoors with pre-existing Wi-Fi credentials for NTP, then with
   no credentials and confirm `TIME: LOCAL` plus manual Set Time remain usable.

### Secondary UI physical checklist

1. Home: tap `i`, `#`, and `...`; also verify up/down/horizontal swipes reach the
   same Profile, Settings, and Pokédex destinations and every path can return.
2. Profile: verify portrait and identity/info columns do not overlap; tap the name.
3. Nickname: enter letters, dot and hyphen, delete, save, cancel, and confirm the
   saved name survives reset.
4. Battle: verify all four compact bars/values and open strength training.
5. Medals: inspect all eight earned/unearned badges in every language.
6. Progress: inspect level bar, next-level text, evolution state, and mistakes.
7. Settings: adjust hour/minute across 00/23 and 00/59, toggle sound, cycle all
   seven languages, cancel once, apply once, and verify persistence after reset.
8. Pokédex grid: inspect all ten pages, including the final partial page; select
   registered, unknown, and shiny entries and return from each detail screen.
9. Pokédex detail: check small and largest PMD sprites, silhouette behavior, header,
   text column, animation, and Back target for clipping or stale pixels.
10. Strength training: hit inside/outside the outlined target, exit early, finish a
    full ten-second run, inspect gain/new-record result, and verify the exact reward.
11. Ball game: tap at the four edges and center, verify circular rebounds, three
    misses, score/record/lives HUD, early exit, result screen, and speed reward.
12. Confirmations: accept/cancel Release, Evolution, and Farewell and verify taps
    outside the shared button rectangles do not trigger an action.

## Validation and changed files

- Original Waveshare default target built successfully immediately after board
  extraction. Both final target builds use the pinned dependencies above.
- Host suite: **110 tests, 39,363 assertions, all passed**. Coverage includes every
  current PMD pack; generated secondary layout cells and controls; visible home
  navigation; starter/egg taps; food choices; training/light/bath/affection
  routing; and confirmation expiry/cancellation. Hardware acquisition and
  framebuffer output are not simulated by these host tests.
- Translation format check passed. Python tools suite: **16 passed, one failed**
  on Windows: `test_los_scripts_ejecutables_declaran_shebang` assumes POSIX
  executable permissions; Windows reports `dex_data.py` as executable. The
  existing test was left unchanged. Run with `PYTHONUTF8=1` on Windows so its
  generator subprocesses use UTF-8 as on Linux.
- ASan/UBSan was attempted but could not link: this Windows compiler has no
  `libasan`/`libubsan`. Linux CI retains the sanitizer run; it was not executed
  remotely during this work.
- The actual partition binaries retain NVS at 0x9000, size 0x5000. Protected
  game/data/asset files have no diff. No physical device tests or flashing were
  performed.

Modified existing files: `TamaPoke.ino`, `pin_config.h`, `audio.cpp`,
`rtcbat.cpp`, `rtcbat.h`, `sdmon.cpp`, `.github/workflows/ci.yml`, `.gitignore`,
`test/Makefile`, `README.md`.

Added files: `src/board/board.h`, `src/board/board_select.h`;
`src/board/waveshare_amoled/{pins.h,board.cpp,audio.cpp,rtcbat.cpp}`;
`src/board/tdeck_plus/{pins.h,board.cpp,st7789_init.h,audio.cpp,rtcbat.cpp}`;
`src/ui/{layout.h,layout_round466.h,layout_landscape320.h,playable_landscape.h,actions_landscape.h}`;
`test/test_layout.cpp`, `test/test_compact.cpp`, `tools/build_firmware.py`, and
this guide. The pre-existing `TDECK_PLUS_PORTING_ANALYSIS.md` was preserved.

Final firmware results:

| Target | Compile | Application binary bytes | Static RAM reported |
|---|---|---:|---:|
| Waveshare | PASS | 812,098 | 33,548 |
| T-Deck Plus | PASS | 1,319,031 | 56,296 |

T-Deck application SHA-256:
`5b0cfdbf12b06cf7ca521d6b01e71e0041df75ee19ba37a46d77470a9e7b83ff`.
Waveshare application SHA-256:
`80f2b2e600811569f98bc6ea09c2ebca8f960d644762a4aae4203d4160c0f58f`.
Esptool validated the T-Deck ESP32-S3 image checksum/hash and 16 MB flash header.
The bootstrap image header says DIO, as configured by Arduino's QIO boot profile;
that is separate from LCD SPI mode 0 and its 40 MHz speed.
The split pieces were checked against the corresponding offsets in the generated
merged image, and staged firmware sources matched the final working files.
Build warnings include existing core/library initializer warnings and SensorLib's
deprecated compatibility touch API; neither build reports errors.
