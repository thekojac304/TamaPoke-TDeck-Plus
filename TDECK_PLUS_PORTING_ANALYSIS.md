# TamaPoke → LILYGO T-Deck Plus porting analysis

Repository baseline: `93ca723`.

This document records a read-only review of the firmware, asset formats and packs, build/CI configuration, tests, provisioning tools, and web installer. No firmware source was modified, no branch was created, and no firmware was built or flashed. This Markdown document was subsequently added at the user's request.

## Recommendation

**The port is feasible from one codebase, with the existing pet logic and sprite formats preserved.** The substantial work is replacing the hardware interfaces and adapting the UI layout. Changing pin definitions and resolution alone will not produce a usable port.

The recommended first milestone is a **touch-operated T-Deck Plus build using Arduino_GFX, the existing game engine, SPI SD storage, and unchanged NVS saves**, with sound added after those work.

The milestone is deliberately minimal:

1. Firmware boots on T-Deck Plus.
2. Display initializes correctly.
3. TamaPoke home screen renders at 320×240.
4. Touch works.
5. Existing game state and pet logic run.
6. SD sprite loading works.
7. Saves persist.
8. Audio works if practical.

GPS, LoRa, keyboard shortcuts, trackball controls, Wi-Fi features, and advanced T-Deck-specific features remain deferred until the base port is stable.

The hardware baseline comes from LILYGO's [official pin definitions](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/utilities.h), [factory example](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/UnitTest.ino), and dedicated peripheral examples. There are inconsistencies between some documentation snippets and example code; those are called out below. These upstream links refer to moving branches; implementation should record the exact revisions used.

## Repository structure and reusable systems

**The existing separation is good enough to preserve upstream compatibility.** `pet.cpp` contains the needs, aging, evolution, breeding, streaks, bond, medals, training rewards, and save/load behavior. It depends on Arduino timing, Preferences, species data, and the abstract sound-effect API—not display or board pins.

| Repository area | Assessment |
|---|---|
| `TamaPoke.ino` | Hardware startup, touch acquisition, gestures, rendering, screen navigation, minigame presentation/physics, and some action completion are combined. This is the largest porting surface. |
| `pin_config.h` | Entirely Waveshare-specific. |
| `rtcbat.cpp` | Entirely tied to PCF85063 RTC and AXP2101 power management. |
| `audio.cpp` | Mixes reusable synthesis/queue logic with Waveshare ES8311 initialization and amplifier control. |
| `sdmon.cpp` | Reusable asset parsing mixed with direct `SD_MMC` access. |
| `pet.*`, `dex.h`, `i18n.*` | Can remain functionally unchanged for the first port. |
| `species.h` | Art can remain unchanged; its preferred display scales need overriding in the renderer. |
| Tools and asset packs | Mostly reusable. Build output selection and serial-port discovery need attention. |

A useful boundary is to keep **hardware behavior in board implementations, geometry in layout profiles, and game rules in their current files**.

## Target hardware mapping

| Function | T-Deck Plus configuration |
|---|---|
| Peripheral power | GPIO10, enabled HIGH before peripheral initialization |
| Shared SPI | SCK 40, MOSI 41, MISO 38 |
| LCD | ST7789; CS 12, DC 11, no dedicated reset pin in the official setup |
| Backlight | GPIO42 |
| SD | SPI, CS 39 |
| LoRa | CS 9; leave deselected even when radio features are disabled |
| I²C | SDA 18, SCL 8 |
| Touch | GT911, interrupt GPIO16 |
| Speaker output | I²S BCK 7, WS 5, DOUT 6 |
| Battery measurement | ADC GPIO4 |
| Keyboard | Interrupt 46; I²C protocol described below |
| Trackball | G01=3, G02=2, G03=15, G04=1; center/BOOT=0 |
| GPS | TX definition 43, RX definition 44 |

These assignments come from [LILYGO's `utilities.h`](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/utilities.h). The microphone has a separate ES7210 interface; it is not needed for game sound.

## Display driver and framebuffer transport

**Keep the graphics API and replace the transport and panel implementation.** The existing display construction at `TamaPoke.ino:30` uses `Arduino_ESP32QSPI`, `Arduino_CO5300`, and a full-screen `Arduino_Canvas`.

For T-Deck Plus:

- Retain `Arduino_Canvas`, RGB565 drawing, existing primitives, and font support.
- Replace CO5300/QSPI with ST7789 over shared SPI.
- Use a 320×240 landscape canvas.
- Remove the Waveshare panel offsets and reset pin.
- Route brightness through the board layer rather than `panel->setBrightness()`.
- Start with synchronous framebuffer transfers and the current roughly 10 FPS schedule.

LILYGO's display setup specifies native 240×320 dimensions, RGB order, `INIT_SEQUENCE_2`, reset `-1`, and 40 MHz SPI; the factory sketch rotates it to landscape. Its modified initialization includes panel-specific gamma and power settings. Those settings matter: LILYGO explicitly warns about differences between T-Deck and Plus panels. [Display setup](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/lib/TFT_eSPI/User_Setups/Setup210_LilyGo_T_Deck.h), [initialization sequence](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/lib/TFT_eSPI/TFT_Drivers/ST7789_Init.h).

There is **no need to migrate the game to TFT_eSPI or LVGL** just because the factory example uses them. Arduino_GFX already has a T-Deck Plus declaration. Its shared `Arduino_HWSPI` backend accepts a `SPIClass` instance, making it a suitable starting point for sharing the same bus with Arduino SD. The exact version and panel initialization still need pinning and validation against LILYGO's sequence. [Arduino_GFX board declarations](https://github.com/moononournation/Arduino_GFX/blob/master/examples/PDQgraphicstest/Arduino_GFX_dev_device.h), [shared SPI interface](https://github.com/moononournation/Arduino_GFX/blob/master/src/databus/Arduino_HWSPI.h).

Do not carry over the assumption that an 80 MHz QSPI framebuffer transfer behaves like an 80 MHz single-lane SPI transfer.

## Touch driver

**Replace acquisition while retaining gestures and actions.** The current touch handler at `TamaPoke.ino:473` contains CST9217-specific workarounds: address `0x5A`, a post-initialization reset, interrupt gating, and an address-only I²C transaction to wake the controller.

For GT911, follow LILYGO's dedicated example:

- `TouchDrvGT911`
- `setPins(-1, BOARD_TOUCH_INT)`
- `GT911_SLAVE_ADDRESS_L`
- Maximum coordinates 320×240
- XY swap enabled
- Mirror X disabled, mirror Y enabled

That is the official starting transform, to be verified at the screen corners. [LILYGO touch example](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/Touchpad/Touchpad.ino).

Expose normalized `pressed/x/y` samples to the existing gesture handler. Preserve taps on release, swipes, long presses, and wake-touch suppression. Do not transfer the CST-specific reset/wake sequence into the GT911 implementation.

Both hitboxes and gesture thresholds need reviewing: an 80-pixel swipe consumes a much larger fraction of a 240-pixel-tall screen. Drawing and hit testing should use the same layout rectangles.

## SD-card changes

**This is a transport change, not an asset-format change.** `sdmon.cpp:143` currently mounts one-bit SDMMC and accesses `SD_MMC` directly throughout loading, listing, diagnostics, and USB uploads.

The T-Deck implementation should:

1. Enable peripheral power.
2. Initialize LCD, SD, and radio chip selects HIGH.
3. Initialize the shared SPI bus once.
4. Mount `SD` using CS39.
5. Keep display transfers and SD operations serialized.

LILYGO's examples start SD at **800 kHz**. Use that for initial validation, then measure and increase cautiously: loading a roughly 0.5 MB sprite pack can be noticeably slow at that rate. The example's printed `SD_MMC` messages are misleading—the actual API is `SD.begin(...)`. [Official SD initialization](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/I2SPlay/I2SPlay.ino).

Have the board layer provide `fs::FS&` for file access, plus separate capacity diagnostics because those are not all generic filesystem operations. Preserve `/mons/`, TPK1/TPK2/TPTH parsing, shiny fallback, `sdDirty`, and the `PUT`/`LS`/`SDINFO` protocol.

The current mount requests formatting on failure. For the T-Deck bring-up, use a prepared FAT32 card and avoid interpreting a shared-bus failure as permission to format it.

## Audio changes

**Audio looks practical, but should follow display, touch, and saves.** The existing `audio.cpp` synthesizes short stereo 16-bit effects at 16 kHz in a FreeRTOS task. The note tables, queue, sound IDs, enable preference, and gameplay calls are reusable.

Replace only the output-specific portion:

- Remove ES8311 register writes for T-Deck.
- Send PCM to BCK7/WS5/DOUT6.
- Disable unused I²S input/MCLK assignments.
- Replace the Waveshare PA-pin behavior with the target's supported mute/output behavior.
- Keep a harmless silent backend available if sound initialization fails.

LILYGO configures speaker output with those three I²S signals. Its ES7210 setup is explicitly for **audio capture**, so it should not be mistaken for the replacement speaker codec. [Official audio example](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/I2SPlay/I2SPlay.ino).

Raw synthesized PCM does not require adding an MP3 decoder or the factory example's entire audio dependency stack. Verify channel selection, volume, clicks, idle hiss, and sleep muting physically.

## Power and RTC assumptions

`rtcbat.cpp` assumes:

- AXP2101 BLDO1 powers the panel.
- AXP2101 supplies battery percentage, charging and USB status.
- AXP2101 reports short power-button presses.
- A four-second power-button hold switches the board off.
- PCF85063 provides time independently of the running game.

None of these interfaces should remain active on the T-Deck backend.

Use GPIO10 for peripheral power. Implement LCD brightness using LILYGO's **16-step pulse-controlled backlight behavior**, translating the game's 0–255 brightness request into supported levels. The factory example drives this explicitly; generic PWM should not be assumed equivalent. [Factory backlight and power behavior](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/UnitTest.ino).

For the first milestone:

- Keep the CPU running when the screen dims.
- Preserve dimming and touch-to-wake.
- Do not introduce deep sleep yet.
- Treat battery percentage as unavailable until ADC conversion is verified.
- Represent unknown USB/charging state explicitly instead of inventing AXP-like readings.
- Do not claim the original PWR-button semantics on the target.

This also matters to saves: the sketch flushes deferred saves when dimmed, screen-off, or sleeping. Removing that policy without replacing its save opportunity could delay persistence.

### Recommended time source

| Time source | First-port decision |
|---|---|
| ESP32 system time | **Use while running.** Implement the existing clock interface with system-time functions. |
| Last timestamp in NVS | **Use as a recovery seed**, not as evidence of elapsed time while off. |
| ESP32 RTC timer during deep sleep | Consider later, with reset/wake validity handling. |
| GPS time | Defer. It must not block boot or become mandatory for gameplay. |
| Wi-Fi/NTP time | Defer with the other network features. |
| Added battery-backed RTC | Optional later if accurate offline aging without GPS/network is essential. |

ESP32-S3's RTC timer can retain timing through sleep and some resets, but power-on reset clears it. System time can be read/set through standard APIs. [Espressif system-time documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/system_time.html).

The official material reviewed does not establish a PCF85063-equivalent, battery-backed calendar RTC available to TamaPoke on this target. Do not design the base port around one.

At cold boot, restore the saved reference—or the existing initial seed for a new game—and advance from there. Feed that into the existing `Pet::syncClock()` interface. This preserves live aging, day/night backgrounds, and day-based logic as the restored clock advances, but **cannot account for unknown time spent completely powered off**.

Simply returning zero forever is insufficient: `Pet::tick()` advances age, but does not advance `lastSeenEpoch`; scene time and daily care depend on the latter.

Also retain the current manual clock convention initially: the UI stores local wall-clock time directly in an epoch-like value. GPS/NTP later introduces a UTC/timezone decision. Preserve the distinction between `setClock()`—a correction without progression—and `syncClock()`—offline catch-up.

## UI adaptation analysis

**The UI needs selective adaptation, not a game redesign.** Fixed geometry is spread across nearly every renderer and corresponding touch handler.

Examples include `CX/CY=233`, ground Y304, lower panel Y312, buttons near Y400, circle radius231, 408-pixel-wide prompts, and settings down to Y436.

Uniform scaling is not a good general solution: fitting the square into 240 pixels makes small text and touch targets cramped; stretching it to 320×240 distorts sprites.

| Screen/system | Existing assumption | Recommended treatment |
|---|---|---|
| Home screen | Tall scene, two rows of needs, bottom button arc | **Layout reflow.** Compact header, pet scene, needs area, and straight four-button toolbar. Keep all existing actions and style. |
| Starter selection | Three 326×70 rows starting Y110 | **Simple reflow/resizing. Required immediately:** fresh NVS opens this before home. |
| Egg/hatching screen | 160-pixel egg and text below Y300 | Reposition and reduce integer zoom; preserve animation and hatch rules. |
| Food selector | Four icons in a 264×64 popup at Y288 | Move into the target viewport; preserve four choices. |
| Release confirmation | Fixed popup and buttons below Y250 | Reposition/rescale controls; retain confirmation behavior. |
| Evolution/farewell/runaway prompts | Large central buttons; long single-line text | Resize, wrap text, and update matching hitboxes. |
| Bath, hearts, poops, evolution/ending effects | Fixed ground, offsets and particle ranges | Re-anchor to the scene rectangle; preserve timing and effects. |
| Profile card | Portrait stacked over several information rows | **Layout redesign:** portrait beside information is a better fit. |
| Battle-stat card | Four widely spaced bars plus training button | Compact/reposition; stat calculations unchanged. |
| Medals card | Two wide columns of descriptive badges | **Layout redesign:** fit/wrap labels or paginate; retain all medals. |
| Progress card | Large level heading and vertically spaced text | Compact/reposition, allowing wrapping. |
| Clock/language/sound settings | Large clock, controls through Y436 | **Layout redesign** within one compact screen or small settings pages. |
| Touch nickname keyboard | Six columns × five rows, 64×52 spacing | **Layout redesign.** Retain touch entry; physical keyboard is optional later. |
| Pokédex grid | Four-by-four 80-pixel cells | Can retain **16 entries/page** with roughly 40-pixel thumbnails at 1× and smaller cells. Update pagination geometry. |
| Pokédex detail | Large sprite with ground Y300 | Reposition and fit sprite to available bounds. |
| Training bag/results | Tall bag and counters below Y268 | Resize/reposition; duration and rewards unchanged. |
| Ball minigame/results | Circular radius205 boundary, floor Y384 | **Viewport/physics adaptation required**, beyond moving labels. |

The minigame is the exception to “just reposition the graphics.” `stepGame()` at `TamaPoke.ino:1149` reflects the ball against a circular boundary. To preserve gameplay most closely, keep its logical circular playfield and transform rendering/input into a smaller square area, using landscape side space for HUD information. Replacing the circle with rectangular walls would require gameplay retuning.

Some presentation functions also complete actions: `drawBath()` calls `pet.clean()`, and minigame render paths advance/finish sessions. Temporarily skipping those renderers must not leave an action started but unable to complete.

For the deliberately minimal milestone, adapt **starter selection, home/egg, touch care controls, and required overlays** first. Unadapted secondary screens should be temporarily unavailable in the T-Deck build rather than opening unusable off-screen controls. Full secondary-screen support follows the base milestone.

## Sprite reuse and PSRAM implications

**Existing art can be reused byte-for-byte.** The review inspected the 302 normal/shiny TPK2 packs:

| Measurement | Repository result |
|---|---:|
| Combined TPK2 data | 41,571,630 bytes |
| Largest pack | 495,503 bytes, Gyarados |
| Thumbnail pack | 169,311 bytes |
| Largest frame width observed | 128 pixels |
| Largest frame height observed | 168 pixels |
| Gyarados idle canvas | 72×128 pixels |

These files store indexed pixels, RGB565 palettes, action frames, and durations—not 466×466 screenshots.

The renderer needs to:

- Allow **1× scale**; current PMD scaling generally has a minimum of 2×.
- Fit both width and height, including action-frame differences.
- Preserve feet anchoring.
- Bound wandering by sprite extent and the scene viewport.
- Override flash-sprite preferred zoom without editing generated artwork.
- Keep integer scaling where it fits; reserve enough scene space for large sprites.

The current 2× Gyarados idle frame is 256 pixels tall. That is a concrete reason to change rendering policy even though the files remain reusable.

Framebuffer memory improves substantially:

| Buffer | Bytes | Approximate size |
|---|---:|---:|
| Current 466×466 RGB565 | 434,312 | 424 KiB |
| Target 320×240 RGB565 | 153,600 | 150 KiB |
| Two target framebuffers | 307,200 | 300 KiB |

Even conservatively budgeting three largest-size PMD blobs, thumbnails, and one target framebuffer gives approximately **1.73 MiB**, before other allocations. The current asset set comfortably fits the target PSRAM model.

Keep allocation failure handling and monitor largest free blocks: the loader accepts larger files than the repository currently contains. Internal RAM for tasks, queues, and transfer buffers still matters.

At 40 MHz single-lane SPI, the theoretical pixel-data transfer minimum is about **30.7 ms per full target frame**, excluding overhead. Start with one framebuffer and measured transfers; additional buffering is not initially necessary.

## Deferred T-Deck features

**Keyboard, trackball, GPS, and radio can remain deferred.** Future input should call the same game actions as touch:

- Keyboard: nickname entry, menu navigation, optional care shortcuts.
- Trackball: selection/pointer movement and confirm.
- Keep those mappings out of `Pet`.

The keyboard is a separate ESP32-C3 device exposing an I²C interface at `0x55`; LILYGO's example reads key bytes and allows startup time after power-on. There is no reason to reflash its firmware for the base port. [Keyboard example](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/Keyboard_T_Deck_Master/Keyboard_T_Deck_Master.ino).

Reserve GPS pins43/44 and avoid using the Plus's Grove interface for another device. GPS documentation snippets differ in UART argument ordering; the factory code passes RX44 then TX43 and contains support for different GNSS initialization paths. Verify the installed module before enabling it. [Factory GPS implementation](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/UnitTest.ino), [Plus documentation](https://wiki.lilygo.cc/products/t-deck-series/t-deck-plus/).

## Blockers and risky dependencies

| Risk | Consequence and mitigation |
|---|---|
| Plus-specific panel initialization | Wrong colors, offsets, inversion, or blank display. Validate against the official sequence before integrating game rendering. |
| Shared SPI ownership | SD failures or display corruption. Use one shared bus instance, transactions, and completed transfers before changing devices. |
| Mixed software generations | TamaPoke CI pins Arduino-ESP32 **3.3.10**; LILYGO's PlatformIO example uses `espressif32@6.3.0` and legacy I²S code. Preserve TamaPoke's baseline and adapt peripheral behavior rather than copying the factory project wholesale. |
| Unpinned third-party libraries | CI currently installs latest library releases. Record tested versions for reproducible dual-board builds. |
| Clock validity | Incorrect offline aging or day/streak changes. Track whether time is restored, retained, or externally synchronized. |
| NVS save scheduling and power loss | Preserve existing save opportunities. Current multi-key saves are not a single atomic game snapshot. |
| Small-screen text | Long translations and Japanese glyphs need layout checks; byte-count centering remains in some screens. |
| Installer identifies only ESP32-S3 | It cannot distinguish the boards automatically. Provide explicit board selection and separate binaries. |

The software-generation comparison is based on the repository's `.github/workflows/ci.yml` and [LILYGO's build configuration](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/platformio.ini).

There is no identified CPU, flash, PSRAM, asset-format, or game-engine blocker. Accurate aging through complete power removal is the one clear capability gap in the proposed minimal configuration.

## Branch and file architecture

**Use a short-lived port branch and two build targets.** Recommended branch: `codex/tdeck-plus-port`, developed as reviewable commits: board extraction, target drivers, layout adaptation, then packaging. Merge into one maintained branch after validation; avoid permanent hardware forks.

Keep current game files in place. Proposed additions:

```text
C:/Dev/TamaPoke/src/
  board/
    board.h
    board_select.h
    waveshare_amoled/
      board.cpp
      pins.h
    tdeck_plus/
      board.cpp
      pins.h
      st7789_init.h
  ui/
    layout.h
    layout_round466.h
    layout_landscape320.h
```

The board interface should provide display/canvas creation and presentation, normalized touch samples, storage access, brightness, power status, clock operations, and the audio output hooks.

Keep `audio.h` and `rtcbat.h` as compatibility facades initially. Compile only the chosen board implementation, and conditionally include its dependencies. Preserve the original board as the default when no target is specified.

Geometry belongs in the UI profiles, not the board driver. Use shared rectangles for drawing and touch testing. Extract only the screens that truly need different arrangements; avoid duplicating the entire sketch.

## File-by-file change map

Paths below are relative to the repository root, `C:/Dev/TamaPoke`.

| Existing file | Proposed change |
|---|---|
| `TamaPoke.ino` | Replace hardware construction/startup and touch acquisition with board calls; add layout selection; adapt rendering, hitboxes, sprite scale, and minigame viewport. Preserve action routing. |
| `pin_config.h` | Become a board-selection compatibility header; move Waveshare pins intact and add separate T-Deck definitions. |
| `rtcbat.cpp` | Delegate to board implementations; retain PCF85063/AXP2101 for Waveshare and add system-time/power behavior for T-Deck. |
| `rtcbat.h` | Preserve callable facade; clarify time validity and unsupported power capabilities. |
| `audio.cpp` | Keep notes, synthesis, queue, and preferences; separate codec/I²S/amplifier backend. |
| `audio.h` | Keep sound IDs and public API; remove board-specific documentation assumptions. |
| `sdmon.cpp` | Replace direct `SD_MMC` access with board storage access; preserve parsing and provisioning protocol. Move legacy preferred scale into layout/render policy. |
| `sdmon.h` | Keep data structures and formats; update storage comments and any required interface declarations. |
| `pet.cpp`, `pet.h` | No initial rule or save-schema changes. |
| `dex.h` | Unchanged generated species/evolution/stat data. |
| `species.h` | Unchanged generated art; renderer chooses target scale. |
| `i18n.cpp`, `i18n.h` | Preserve translations and preference storage. Add wording only if the new clock/status UI requires it. |
| `.github/workflows/ci.yml` | Add both hardware builds, pin dependencies, retain existing tests and generated-header checks. |
| `tools/build_web.sh` | Accept board selection and produce distinct firmware outputs. |
| `tools/send_sd.py` | Protocol unchanged; improve macOS-only autodetection. Existing `--port COMx` can serve Windows initially. |
| `tools/touch_log.py` | Add explicit/cross-platform port selection; optionally include raw and transformed coordinates. |
| `tools/pack_pmd.py`, `tools/make_thumbs.py`, `tools/pack_bundle.py` | No format or regeneration changes required. |
| `tools/sprites.py`, `tools/import_gif.py` | No required changes. |
| `tools/gen_dex.py`, `tools/gen_names.py`, `tools/dex_data.py`, `tools/dex_names.py`, `tools/dex_stats.py`, `tools/test_i18n_formats.py` | Unchanged data/generation/validation pipeline. |
| `tools/sdcard/mons/*`, `tools/pmd_test/*`, `web/sprites.pak` | Reuse unchanged. |
| `web/index.html` | Later: explicit hardware selection, target instructions, and board-aware upload-resume handling. |
| `web/manifest.json` | Separate board manifests/firmware paths; preserve saves during update flows. |
| `web/firmware/tamapoke.bin` | Keep original-board artifact; generate a distinct T-Deck binary later. |
| `test/test_pet.cpp`, `test/test_dex.cpp`, `test/test_i18n.cpp`, `test/test_tools.py` | Preserve existing coverage. Add targeted clock/layout/storage checks separately. |
| `test/main.cpp`, `test/framework.h`, `test/shim/*`, `test/Makefile`, `test/run_tests.sh` | Retain test infrastructure; extend only for new portable components. |
| `README.md`, `web/README.md`, `test/README.md`, `CHANGELOG.md` | Document board targets, first-milestone limits, clock behavior, and verification. |
| `LICENSE`, `CREDITS.md`, `tools/assets/README.md`, `ruff.toml`, `.gitignore` | No functional port changes; add attribution/output exclusions only where needed. |

## Dependency and library changes

| Dependency/build setting | Recommendation |
|---|---|
| Arduino-ESP32 | Start with existing **3.3.10** pin. |
| Arduino_GFX | Keep; pin a tested version supporting shared SPI/ST7789 and the required initialization customization. |
| SensorLib | Keep; use CST92xx on Waveshare and GT911 on T-Deck. |
| XPowersLib | Waveshare-only dependency. |
| U8g2 | Keep existing font data and text behavior. |
| SPI, SD, FS | Use ESP32 core libraries for T-Deck storage. |
| SD_MMC | Retain for Waveshare only. |
| ESP_I2S, Preferences, FreeRTOS | Retain existing core facilities. |
| TFT_eSPI, LVGL, ESP32-audioI2S | Not required by the proposed port; factory references only unless bring-up proves a specific need. |
| GPS/LoRa libraries | Defer. |
| Flash/PSRAM configuration | 16 MB flash, OPI PSRAM, QIO flash, USB CDC. Verify reported sizes at boot. |
| Partition scheme | Keep the existing compatible 16 MB arrangement initially; sprites live on SD and saves in NVS. |
| PlatformIO | Optional later. Arduino CLI already supports the repository's workflow. |

LILYGO's [board definition](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/boards/T-Deck.json) confirms 16 MB flash, 8 MB OPI PSRAM, and QIO/OPI configuration. Preserve NVS location/schema when updating firmware. Moving the SD card alone will **not** transfer an existing pet between physical devices; saves live in the ESP32's flash.

## Recommended implementation order and difficulty

| Step | Scope and acceptance criterion | Difficulty |
|---|---|---|
| 1 | Extract the board boundary; original Waveshare build still compiles with equivalent behavior. | Medium |
| 2 | Add T-Deck build configuration; boot reliably on USB and battery; verify flash/PSRAM and nonblocking serial. | Low–medium |
| 3 | Initialize power and display; render colors, text, edges, and a 320×240 test pattern correctly. | Medium |
| 4 | Add GT911 sampling; verify corners, drag, release, and wake-touch behavior. | Medium |
| 5 | Mount shared SPI SD; read thumbnails and large normal/shiny packs while display remains stable. | Medium–high |
| 6 | Add system-time fallback; confirm saved-time recovery and absence of fabricated offline progression. | Medium |
| 7 | Adapt starter selection, home/egg, care controls, and necessary overlays. | Medium–high |
| 8 | Verify game ticks and persistent state across reset/power cycle, including deferred saves and missing SD. | Medium |
| 9 | Enable synthesized speaker effects and confirm mute/sleep behavior. | Medium; optional milestone item |
| 10 | Adapt remaining screens and minigame viewport without changing rewards/rules. | Medium–high; after base port |
| 11 | Publish separate board artifacts and installer selection; validate both builds in CI. | Low–medium |

Existing host tests cover substantial pet, persistence, and offline-time behavior, but do not cover hardware drivers or the sketch's UI. That coverage was inspected; build/test workflows that generate files were not executed during the read-only analysis.

## Physical-hardware questions and unknowns

1. **Panel revision:** Which initialization/inversion settings produce correct colors and all 320×240 pixels on this particular Plus?
2. **Touch:** Does the official swap/mirror transform align everywhere, and are press/release events reliable after idle?
3. **Shared SPI:** What SD speed is stable alongside display transfers, including large sprite loads and USB writes?
4. **Power:** Does GPIO10 sequencing work on cold battery boot, and does the 16-step backlight behave as expected?
5. **Audio:** Does 16 kHz stereo PCM play cleanly through the speaker, including short effects and silence?
6. **Persistence/time:** Which resets preserve system time, what happens at the physical power switch, and do completed saves survive interruption?
7. **Later GPS:** Which GNSS module/revision is fitted, and which UART ordering/baud matches it? This should not gate the base port.
