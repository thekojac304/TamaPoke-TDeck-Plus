# T-Deck Plus playable port and secondary UI

Branch: `codex/tdeck-plus-port`. This is a compile-tested bring-up candidate;
boot, panel colors, touch alignment, shared-bus reliability, and physical save
persistence still require a T-Deck Plus. No device was flashed during development.

## 2026-09-14 physical-test follow-up (provisional)

The fixes in this section build and pass host tests but are **not yet confirmed
on hardware**. GPS, Wi-Fi, battery percentage, the settings pages, and the new
dim step must remain provisional until the checklist below is completed.

- Wi-Fi actions are now requests serviced by `rtcPollAuthoritative()` rather
  than driver/NVS operations executed inside the touch handler. The old
  `radioOff()` called `WiFi.disconnect(true, false)` (which deinitializes the
  driver) and then `WiFi.mode(WIFI_OFF)` a second time; scan immediately changed
  back to station mode in that same callback. That is the source-level reboot
  path: a double teardown followed by immediate reinitialization can race the
  Arduino Wi-Fi event task. The replacement performs one ordered shutdown and
  has no delay/wait loops. A retained breadcrumb plus `esp_reset_reason()` now
  identifies whether any remaining hardware reset is panic/watchdog/brownout
  and the action in progress. No Wi-Fi path intentionally restarts the ESP.
- Every Wi-Fi action logs begin/completion, free/minimum heap, largest internal
  block, driver mode, and link status. Passwords are never logged. Missing
  credentials return `Wi-Fi not configured`; disabled Wi-Fi does not start the
  radio.
- GNSS is UART1, ESP RX GPIO44, ESP TX GPIO43, 8N1, **38400 baud first**. This
  matches WadaMesh's working T-Deck Plus target and LILYGO's u-blox M10 path.
  LILYGO's 9600 path is for the alternate L76K. TamaPoke emits an `0xFF` wake
  edge, waits 250 ms, and only if no checksum-valid NMEA appears probes 9600 for
  2.5 seconds before returning to 38400. No other TamaPoke subsystem uses UART1.
- Settings is now three landscape pages: Time (live clock/source and GPS,
  Wi-Fi, manual actions), Wi-Fi (enable/status/scan/connect/forget/sync), and
  System (display timeout/sound/language/battery). Manual edit and confirmed
  catch-up remain separate pages. Rendering and hit testing use the same
  rectangles.
- Battery voltage follows LILYGO's hardware description: the eight-sample
  eFuse-calibrated ADC millivolt average on GPIO4 multiplied by the exact 2:1
  100k/100k divider. No unmeasured correction factor was added. Percentage uses
  Meshtastic's 11-point single-cell Li-ion OCV curve, remains approximate, and
  is shown together with voltage. EMA and two-point hysteresis remain after the
  unsmoothed converted voltage is logged.

Available firmware uses materially different conversion and percentage policy:

| Firmware | ADC-to-cell conversion | Percentage policy |
|---|---|---|
| LILYGO official T-Deck test | Arduino calibrated `analogReadMilliVolts(GPIO4) * 2` | Displays voltage; no authoritative percentage curve |
| WadaMesh | Eight-sample calibrated millivolts times `2.037` (the underlying meshcomod target uses `2.0`) | Linear from 3.3 V to configurable full voltage, normally 4.2 V |
| Launcher | Calibrated millivolts times `2.0` | Linear estimate over an approximately 3.3--4.1 V range |
| Meshtastic | ESP-IDF calibrated ADC conversion times `2.11` | 11-point 3.10--4.19 V Li-ion open-circuit curve |
| TamaPoke (this pass) | Eight-sample calibrated millivolts times exact `2.0` | Meshtastic's 11-point curve; shown as `~%` beside voltage |

TamaPoke follows LILYGO for the board voltage conversion because its exact 2:1
divider is the hardware-authoritative baseline. It borrows only Meshtastic's
more realistic percentage shape. Voltage-conversion confidence is medium-high
from the design/source comparison, but it is not physically calibrated on this
unit. Percentage confidence is low-to-medium because load, cell chemistry, and
the lack of a fuel gauge can shift an instantaneous voltage estimate.
- T-Deck awake brightness remains 145 (battery) or 180 (reported external
  power). The former dim value 60 mapped to pulse step 4. The new value is
  derived as awake/6: 24 or 30, both pulse step 2. Wake recomputes and restores
  the exact awake value. Manual off still goes directly to step 0. Waveshare's
  brightness code and former dim values are unchanged.

## Physical-validation audio and time fixes

The compact touch router had omitted the original sound calls for starter/egg
taps, food selection, care-button taps and petting. The ball game remained
audible because its sound call lives in shared game logic. Those compact paths
now emit the same existing IDs as the round UI. The T-Deck backend logs queued,
started, completed and cancelled/discarded IDs, stops the current tone on mute or
sleep, clears queued effects, and still releases I2S after each burst.

GPS and NTP were parsed as UTC but the first authoritative mapping used a zero
wall offset, so UTC was displayed directly. The ESP32 system clock now stays on
the authoritative UTC timeline. User-facing and local-gameplay values are
projected through a persisted POSIX timezone preset. The preset list is UTC, US
Eastern, Central, Mountain, and Pacific; US zones use the current second-Sunday
in March/first-Sunday in November DST rules. A missing setting defaults to UTC
and must be selected explicitly. Changing it calls `setClock()`, never
`syncClock()`, so it cannot replay offline progression. Existing `tp-clock`
wall/UTC/mapping keys remain readable.

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

Settings now has non-overlapping Time, Wi-Fi and System pages. Time owns the live
local time, authoritative source, timezone preset, `SYNC GPS`, `SYNC WI-FI`, and
`SET MANUALLY`. Wi-Fi provides on/off, known SSID, low-power disconnected status,
scan, connect, forget and sync controls. Scan selection opens a masked password
field driven by the physical keyboard at I2C address `0x55`; printable ASCII,
backspace/delete, Enter and Escape are supported, while Connect and Cancel remain
touch controls. Credentials are saved only after successful authentication and
DHCP, and are retained while Wi-Fi is off.

The connection failure path was a scan-radio teardown/association race: the code
could call `WiFi.begin()` immediately after asynchronous scan cleanup and station
disconnect. Association is now staged for 200 ms on the main loop, uses owned,
NUL-terminated buffers, and logs station states/events without logging secrets.

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
| `tp-clock` | `tz` | uint8 | POSIX timezone preset index; absent/invalid means UTC |
| `tp-display` | `timeout` | uint8 | 0..5 timeout choice; default 2 (two minutes) |

The `trusted` key is backward-compatible: a valid legacy mapped pair is treated
as trusted when the key is absent. No existing pet NVS key changed.

Successful GPS sync emits (numbers vary):

```text
GPS UART initialized uart=1 rx=44 tx=43 baud=38400 wake=FF reason=tdeck-plus-default startup_ms=250
GPS diag baud=38400 bytes_s=<n> bytes_total=<n> types=RMC/GGA/... checksum_ok=<n> rmc_ok=<n> zda_ok=<n> parsed_utc=<epoch> ubx=<0|1>
CLOCK source=GPS acquired_utc=<epoch>
CLOCK references saved_utc=<epoch-or-0> saved_wall=<epoch-or-0> offset=<seconds>
CLOCK mapping current_wall=<epoch> synced_wall=<epoch> offset=<seconds>
GPS authoritative accepted=1 utc=<epoch> catchup=<0|1>
CLOCK game pre_sync=<epoch> post_sync=<epoch> offline_applied=<0|1> catchup_minutes=<minutes>
```

Boot and Wi-Fi action diagnostics include the readable reset class and retained
in-progress action. A reset during a scan, for example, should begin with:

```text
RESET reason=<panic|task-watchdog|interrupt-watchdog|watchdog|brownout|software|...> code=<n> previous_wifi_action=scan nvs=<ok|open-failed>
WIFI action=scan phase=begin heap=<bytes> min=<bytes> largest_internal=<bytes> state=<mode> status=<status>
WIFI action=scan phase=complete heap=<bytes> min=<bytes> largest_internal=<bytes> state=<mode> status=<status>
```

Successful Wi-Fi setup or NTP sync includes:

```text
WIFI candidate ssid="<ssid>" password_length=<n> terminated=1
WIFI connect staged ssid="<ssid>" password_length=<n> purpose=<setup|auto-ntp|manual-ntp> credentials=tp-wifi
WIFI begin ssid="<ssid>" password_length=<n> return_status=<status>
WIFI state previous=<status> current=<status>
WIFI auth=success dhcp=waiting
WIFI dhcp=acquired ip=<address>
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
WIFI disconnected reason=<code> auth=<failed|not-established>
WIFI connect timeout status=<status> disconnect_reason=<code>
CLOCK Wi-Fi/NTP failed reason=<WIFI AUTH FAILED|WIFI CONNECT FAILED|NTP FAILED>
CLOCK automatic sync failed gps=<no-time|no-data> wifi=<reason>
```

Physical validation for this pass:

1. Verify startup, hatch, feed, pet/heart, light sleep/wake, bath, evolution,
   ceremony, strength training and ball-game sounds; mute during a long effect
   and confirm playback/queue stop; confirm idle has no hiss.
2. Select US Eastern, boot outdoors, and confirm `TIME: GPS` plus UTC-5 in winter
   or UTC-4 in summer; switch UTC/Eastern/Pacific and confirm only wall display
   and local day/night semantics move, with `catchup=0` for each timezone change.
3. Leave Settings and then the manual editor open while GPS sync arrives; both
   must refresh without closing, overlap or stale hour/minute values.
4. Enable Wi-Fi, scan, select a network, enter letters/numbers/symbols with the
   physical keyboard, test backspace, Escape, Enter, and touch Connect/Cancel.
   Confirm masked display, `TIME: WIFI`, no password in serial output, saved
   `KNOWN` status, automatic reuse after reboot/manual sync, and radio shutdown.
5. Enter a deliberately wrong password and confirm an authentication reason is
   logged, no secret appears, the device stays running, and retry succeeds.
6. Reboot after a measured powered-off interval and confirm exactly one nonzero
   `catchup_minutes`; repeat with a backward/bad timestamp and confirm zero.
7. Disable Wi-Fi and test GPS failure: the Set Time/Later prompt appears once,
   Later keeps gameplay usable, and manual setting shows `TIME: MANUAL`.
8. Forget the known network, verify automatic reuse stops, then toggle Wi-Fi off
   with credentials saved and verify the radio stays off until re-enabled.
9. Smoke-test the Waveshare build for unchanged clock, layout and audio behavior.

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
BATTERY raw_adc=<code> adc_mv=<pin mV> cell_mv=<adc_mv*2> formula=adc_mv*2 smooth_mv=<EMA> estimated_pct=<0..100|-1> approximate=1
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
- Battery GPIO4 is sampled at 12-bit resolution with 11 dB attenuation and the
  calibrated Arduino millivolt API, then multiplied by the board's 100k/100k
  divider (2:1). Eight-sample averaging, a 7/8 EMA, two-percentage-point
  hysteresis, and Meshtastic's 11-point Li-ion OCV estimate reduce flicker. Implausible
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
11. For this pass, compare `raw_adc`, `adc_mv`, `cell_mv` and `estimated_pct`
    against the other firmware at the same resting state; a multimeter is not
    required. Confirm Settings shows voltage plus `~%`, invalid/no-battery samples
    fabricate neither percentage nor charging/USB state. A later meter check at
    charged, mid-level and low states is still needed for physical calibration.

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
  reports 1,344,931 bytes of sketch flash use and 57,248 bytes static RAM.
- Host suite: **134 tests, 39,634 assertions, all passed**. Targeted coverage now
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

This physical-test pass modified `TDECK_PLUS_BRINGUP.md`, `TamaPoke.ino`,
`src/board/tdeck_plus/{audio.cpp,battery_policy.h,board.cpp,display_power_policy.h,rtcbat.cpp}`,
`src/ui/{layout_landscape320.h,secondary_landscape.h}`, and
`test/{test_battery.cpp,test_display_power.cpp,test_layout.cpp}`. Gameplay
data/rules, sprite assets, Waveshare behavior, and the existing pet NVS schema
were preserved.

Final firmware results:

| Target | Compile | Application `.bin` bytes | Static RAM reported |
|---|---|---:|---:|
| Waveshare | PASS | 812,528 | 33,548 |
| T-Deck Plus | PASS | 1,345,072 | 57,248 |

T-Deck application SHA-256:
`121b3cbe658e05a21496e0bc03409372f76b1c8eeaa37c35294c72c9b4f284af`.
Waveshare application SHA-256:
`b74a4118b5589545e26710938ea058d84273f4781dc2c6191239700bf82b060a`.
Esptool validated the T-Deck ESP32-S3 image checksum/hash and 16 MB flash header.
The bootstrap image header says DIO, as configured by Arduino's QIO boot profile;
that is separate from LCD SPI mode 0 and its 40 MHz speed.
The split pieces were checked against the corresponding offsets in the generated
merged image, and staged firmware sources matched the final working files.
Build warnings include existing core/library initializer warnings and SensorLib's
deprecated compatibility touch API; neither build reports errors.
