# T-Deck Plus playable port and secondary UI

Branch: `codex/tdeck-plus-port`. This is a compile-tested bring-up candidate;
boot, panel colors, touch alignment, shared-bus reliability, and physical save
persistence still require a T-Deck Plus. No device was flashed during development.

## Physical-validation audio and time fixes

The compact touch router had omitted the original sound calls for starter/egg
taps, food selection, care-button taps and petting. The ball game remained
audible because its sound call lives in shared game logic. Those compact paths
now emit the same existing IDs as the round UI. The T-Deck backend logs enqueue,
start, completion and cancellation/discard, stops the current tone on mute or
sleep, clears queued effects, and still releases I2S after each burst.

The previous first authoritative sync deliberately preserved a stale legacy
wall value, while the already-open Settings screen retained its old edit buffer.
That produced `TIME: SYNC` without a corrected displayed clock. An unmapped
legacy clock now adopts trustworthy UTC without inventing offline elapsed time;
after the user sets local time, the normal `offset` mapping is persisted. A
mapped clock applies safe powered-off catch-up once. Settings renders the live
wall clock and refreshes the manual editor immediately when sync arrives, which
also updates day/night rendering through `pet.lastSeenEpoch`.

The T-Deck state flow is:

```text
GPS: SEARCHING (12 s preference)
  -> TIME: GPS (first valid source wins)
  -> WIFI: CONNECTING -> WIFI: SYNCING -> TIME: WIFI
  -> TIME: LOCAL -> one Set Time / Later prompt
Manual Set Clock Only / confirmed Set & Catch Up -> TIME: MANUAL
```

GPS parsing continues during Wi-Fi fallback, but a second source cannot deliver
another `syncClock()` or another powered-off catch-up. The default manual action,
`Set Clock Only`, uses `setClock()` and never performs catch-up. `Set & Catch Up`
requires a saved/trusted reference, a sane forward time, and an Apply confirmation;
it then calls the same `syncClock()` path exactly once. Invalid, backward or
over-ten-year deltas are rejected, and `syncClock()` retains its two-week gameplay
cap. Wi-Fi/NTP work is asynchronous and the radio is disabled again after scan,
setup, connect, success or failure.

Settings now has non-overlapping `SYNC GPS`, `SYNC WIFI`, and `MANUAL` controls,
plus a dedicated live time/source row. `WIFI TIME` provides Wi-Fi on/off, saved
SSID, scan, connect, forget and sync controls. Scan shows the strongest nearby
networks; selection opens a touch password keyboard with lower/upper/symbol
pages. Credentials are saved only after a successful connection.

TamaPoke never reads Launcher, Meshtastic, WadaMesh or generic ESP32 station
credentials. Its new NVS namespace is `tp-wifi`:

| Key | Type | Meaning |
|---|---|---|
| `on` | bool | User permits Wi-Fi fallback; defaults false |
| `ssid` | string | TamaPoke-selected station SSID |
| `pass` | string | Station password; never logged |

T-Deck-specific state is isolated from the existing pet-save schema:

| Namespace | Key | Type | Meaning |
|---|---|---|---|
| `tp-clock` | `wall` | uint32 | Last wall-clock reference |
| `tp-clock` | `utc` | uint32 | Paired authoritative UTC reference when mapped |
| `tp-clock` | `offset` | int32 | Wall-minus-UTC offset |
| `tp-clock` | `mapped` | bool | UTC/wall pair is valid |
| `tp-clock` | `trusted` | bool | Manual catch-up has a valid reference |
| `tp-display` | `timeout` | uint8 | 0..5 timeout choice; default 2 (two minutes) |

The `trusted` key is backward-compatible: a valid legacy mapped pair is treated
as trusted when the key is absent. No existing pet NVS key changed.

Successful GPS sync emits (numbers vary):

```text
CLOCK source=GPS acquired_utc=<epoch>
CLOCK references saved_utc=<epoch-or-0> saved_wall=<epoch-or-0> offset=<seconds>
CLOCK mapping current_wall=<epoch> synced_wall=<epoch> offset=<seconds>
CLOCK game pre_sync=<epoch> post_sync=<epoch> offline_applied=<0|1> catchup_minutes=<minutes>
```

Successful NTP sync adds the connection/request lines:

```text
CLOCK Wi-Fi connect purpose=<auto-ntp|manual-ntp> credentials=tp-wifi
CLOCK source=WIFI NTP requested
CLOCK source=WIFI acquired_utc=<epoch>
CLOCK references saved_utc=<epoch-or-0> saved_wall=<epoch-or-0> offset=<seconds>
CLOCK mapping current_wall=<epoch> synced_wall=<epoch> offset=<seconds>
CLOCK game pre_sync=<epoch> post_sync=<epoch> offline_applied=<0|1> catchup_minutes=<minutes>
```

Bounded automatic failure emits one of:

```text
CLOCK automatic sync failed gps=<no-time|no-data> wifi=<disabled|not-configured>
```

or, after a configured network attempt:

```text
CLOCK Wi-Fi/NTP failed reason=<WIFI CONNECT FAILED|NTP FAILED>
CLOCK automatic sync failed gps=<no-time|no-data> wifi=<reason>
```

Physical validation for this pass:

1. Verify startup, hatch, feed, pet/heart, light sleep/wake, bath, evolution,
   ceremony, strength training and ball-game sounds; mute during a long effect
   and confirm playback/queue stop; confirm idle has no hiss.
2. Boot outdoors and confirm the status changes from `GPS: SEARCHING` to
   `TIME: GPS`, the displayed time changes immediately, and day/night follows.
3. Leave Settings and then the manual editor open while GPS sync arrives; both
   must refresh without closing, overlap or stale hour/minute values.
4. Enable Wi-Fi, scan, select a network, enter its password, connect, then sync.
   Confirm `TIME: WIFI`, no password in serial output, and radio shutdown.
5. Reboot after a measured powered-off interval and confirm exactly one nonzero
   `catchup_minutes`; repeat with a backward/bad timestamp and confirm zero.
6. Disable Wi-Fi and test GPS failure: the Set Time/Later prompt appears once,
   Later keeps gameplay usable, and manual setting shows `TIME: MANUAL`.
7. Smoke-test the Waveshare build for unchanged clock, layout and audio behavior.

## Scope and architecture

- `src/board/board.h` exposes canvas creation/startup, normalized touch,
  brightness, and generic filesystem access with capacity diagnostics.
- `src/board/board_select.h` selects T-Deck only with
  `TAMAPOKE_TDECK_PLUS=1`; Waveshare remains the default.
- `src/board/waveshare_amoled/` retains the original CO5300/QSPI, CST9217,
  SDMMC, ES8311 audio, PCF85063 and AXP2101 implementations. Root `audio.cpp`
  and `rtcbat.cpp` are compatibility placeholders; their public headers remain.
- `src/board/tdeck_plus/` owns all target pins, ST7789 initialization, GT911,
  trackball-center power behavior, pulse backlight, battery ADC, SPI SD, I2S
  audio, and the GPS/NTP/manual system-clock implementation.
- `src/ui/layout*.h` defines geometry. `playable_landscape.h` renders the small
  screen; `actions_landscape.h` routes its rectangles into existing Pet methods.
  The same rectangles drive drawing and touch hit tests.
- `sdmon.cpp` uses `Board::storage()` without changing formats, paths, shiny
  fallback, thumbnail loading, or the PUT/LS/SDINFO protocol.
- Species, evolution, care, progression, training, Pokédex, pet-save schema,
  partition addresses, and sprite assets remain unchanged. Pet clock telemetry
  exposes the last applied catch-up without persisting a new gameplay field.

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
`HEALTH`, `STATS`, `SDINFO`, and `LS` remain available. Battery startup and
once-per-minute diagnostics show calibrated ADC, doubled cell voltage, smoothed
voltage, and estimated percentage:

```text
BATTERY adc_mv=<divider millivolts> cell_mv=<doubled millivolts> smooth_mv=<EMA> estimated_pct=<0..100|-1>
```

Charging and USB state remain deliberately unknown (`-1`) because the T-Deck
does not expose a reliable signal for them here. For coordinate logs, rebuild
with `--touch-debug` and look for `TOUCH x y` on new pressed reports.

## Temporary limitations

- The ESP32 system clock is the active game clock. A non-blocking GPS parser first
  accepts checksum-valid RMC or ZDA UTC (date/time does not require a positional
  fix). After a 12-second preference window, NTP is tried only when the user has
  enabled Wi-Fi and configured TamaPoke credentials. Connection and synchronization
  have bounded timeouts; manual time remains available throughout.
- The game still displays its historical local-wall-clock-shaped epoch. The
  separate `tp-clock` NVS namespace pairs that wall value with authoritative UTC
  and an offset. The first sync of an unmapped legacy save adopts UTC and
  deliberately does not guess powered-off time. Later manual time establishes
  the local offset; mapped boots use the paired UTC references for one absolute
  `syncClock()` call; negative and over-ten-year source jumps are not applied,
  while the existing game logic retains its two-week progression cap.
- The T-Deck screen timeout choices are 30 seconds, 1, 2, 5, and 10 minutes, or
  Never; two minutes is the default. The last 15 seconds are dimmed. Timeout or
  a 1750 ms trackball-center hold flushes a pending save, stops rendering, and
  turns off only the backlight. Game logic and time sync keep running. A normal
  center press wakes at normal brightness; touch cannot wake the display.
  Trackball movement and normal touch activity reset the idle timer. No deep
  sleep is implemented; see `TDECK_PLUS_DEEP_SLEEP_DESIGN.md`.
- Battery GPIO4 is sampled with the calibrated Arduino ADC API and the board's
  100k/100k divider (2:1). Eight-sample averaging, a 7/8 EMA, two-percentage-point
  hysteresis, and a piecewise Li-ion discharge estimate reduce flicker. Implausible
  readings outside 3.0–4.4 V are omitted. Charging/USB state is not inferred.
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
4. Cycle every screen timeout and verify persistence after reboot. At 30 seconds,
   verify dimming during the last 15 seconds and full backlight-off at the limit.
   Confirm the game clock/progression and background time sync continue. Touch
   must not wake; a normal trackball-center press must wake immediately without
   leaking an action. Hold center for at least 1750 ms while on and verify immediate
   screen-off plus a save flush. Confirm direction movement resets the idle timer.
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
11. Compare GPIO4 battery diagnostics with a multimeter across charged, mid-level,
    and low battery conditions. Confirm Home percentage is stable, Settings voltage
    is plausible, invalid/no-battery samples show no fabricated percentage, and no
    charging icon/state is invented.

### Secondary UI physical checklist

1. Home: tap `i`, `#`, and `...`; also verify up/down/horizontal swipes reach the
   same Profile, Settings, and Pokédex destinations and every path can return.
2. Profile: verify portrait and identity/info columns do not overlap; tap the name.
3. Nickname: enter letters, dot and hyphen, delete, save, cancel, and confirm the
   saved name survives reset.
4. Battle: verify all four compact bars/values and open strength training.
5. Medals: inspect all eight earned/unearned badges in every language.
6. Progress: inspect level bar, next-level text, evolution state, and mistakes.
7. Settings: adjust hour/minute across 00/23 and 00/59. Verify Set Clock Only
   changes the display and `TIME: MANUAL` but leaves age, level/evolution timing,
   needs, and daily state untouched. For Set & Catch Up, verify the confirmation
   shows the saved reference, new time and interval; Cancel changes nothing, Apply
   advances those systems once, and a second tap/re-entry cannot duplicate it.
   Verify backward/absurd input and missing trusted reference are rejected. Also
   toggle sound, cycle all seven languages, and verify settings persistence.
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

- Both final targets build with the pinned dependencies above. Waveshare reports
  812,378 bytes of sketch flash use and 33,548 bytes static RAM. T-Deck Plus
  reports 1,341,371 bytes of sketch flash use and 57,192 bytes static RAM.
- Host suite: **133 tests, 39,610 assertions, all passed**. Targeted coverage now
  proves Set Clock Only does not progress the pet; manual confirmed catch-up and
  GPS/Wi-Fi authoritative delivery advance age, level/evolution readiness, needs,
  and existing daily state through `syncClock()`; and each absolute catch-up can
  be consumed only once. Timeout and battery estimation policies are also tested.
  Hardware ADC, GPIO, GPS, Wi-Fi, audio, and framebuffer output are not simulated.
- Translation format check passed. Python tools suite: **16 passed, one failed**
  on Windows: `test_los_scripts_ejecutables_declaran_shebang` assumes POSIX
  executable permissions; Windows reports `dex_data.py` as executable. The
  existing test was left unchanged. Run with `PYTHONUTF8=1` on Windows so its
  generator subprocesses use UTF-8 as on Linux.
- ASan/UBSan was attempted but could not link: this Windows compiler has no
  `libasan`/`libubsan`. Linux CI retains the sanitizer run; it was not executed
  remotely during this work.
- The actual partition binary retains NVS at 0x9000, size 0x5000. Esptool 5.3.0
  recognizes the final application as ESP32-S3/16 MB and validates its image
  checksum and hash. Staged T-Deck firmware sources match the working tree.
  No physical device tests or flashing were performed.

This pass modified `TamaPoke.ino`, `rtcbat.h`, `src/board/board.h`, both board
implementations' compatibility surfaces, the T-Deck pins/clock/board policies,
the compact layout/renderers, `test/Makefile`, and focused time/layout/Pet tests.
It added `src/board/tdeck_plus/{display_power_policy.h,battery_policy.h}`,
`test/{test_display_power.cpp,test_battery.cpp}`, and
`TDECK_PLUS_DEEP_SLEEP_DESIGN.md`. `TDECK_PLUS_PORTING_ANALYSIS.md`, gameplay
data/rules, sprite assets, and the existing pet NVS schema were preserved.

Final firmware results:

| Target | Compile | Application `.bin` bytes | Static RAM reported |
|---|---|---:|---:|
| Waveshare | PASS | 812,528 | 33,548 |
| T-Deck Plus | PASS | 1,341,520 | 57,192 |

T-Deck application SHA-256:
`0446683e14ac9358984b4eb85562554f73a2522b9c36bd9527d776917fb4aecb`.
Waveshare application SHA-256:
`f0a50c8579aa472db7a78f72b8f201be3f4769968eae3c31b78c36f28c20de03`.
Esptool validated the T-Deck ESP32-S3 image checksum/hash and 16 MB flash header.
The bootstrap image header says DIO, as configured by Arduino's QIO boot profile;
that is separate from LCD SPI mode 0 and its 40 MHz speed.
The split pieces were checked against the corresponding offsets in the generated
merged image, and staged firmware sources matched the final working files.
Build warnings include existing core/library initializer warnings and SensorLib's
deprecated compatibility touch API; neither build reports errors.
