# TamaPoke — T-Deck Plus Edition

An unofficial port of [TamaPoke](https://github.com/socquique/TamaPoke) to the **LilyGO T-Deck Plus**. TamaPoke, by
socquique, is a virtual pet inspired by first-generation Pokémon: hatch an egg, feed, clean and train your Pokémon,
evolve it and fill a Pokédex of all 151 (shinies included). Upstream runs on a round 466×466 AMOLED board; this edition
redraws the game for the T-Deck Plus's 320×240 landscape screen and adds the hardware support that board needs: its
display and touch panel, SPI microSD, speaker, battery meter, and GPS or Wi-Fi time.

This is a fan project and not an official TamaPoke release. It is not affiliated with socquique, LilyGO, Nintendo,
Game Freak or The Pokémon Company.

## Download

**[TamaPoke T-Deck Plus Edition 1.16-tdeck.1 (preview)](https://github.com/thekojac304/TamaPoke-TDeck-Plus/releases/tag/v1.16-tdeck.1)**:
`TamaPoke-TDeckPlus-1.16-tdeck.1-firmware.zip` and `SHA256SUMS.txt`.

The zip contains the four flash pieces (bootloader, partition table, `boot_app0`, application), a single
`merged` image for a first installation, flashing instructions, and the license and credits.

Upstream's web installer and releases are for the Waveshare board. Don't flash them onto a T-Deck.

## Status

**Preview, partly validated on hardware.** The firmware runs on a T-Deck Plus, and the GPS time path has been
confirmed over serial on a real device. The latest fixes in this build (Wi-Fi connection lifecycle, timezone presets)
compile and pass the host tests but have not been re-checked on the device. The battery percentage and the settings
pages are provisional. See [TDECK_PLUS_BRINGUP.md](TDECK_PLUS_BRINGUP.md) for the full test checklist.

## What works on the T-Deck Plus

| Hardware | Support in this edition |
|---|---|
| ST7789 320×240 display | Full game, landscape layout, pulse-controlled backlight with dimming and a screen timeout |
| GT911 touch panel | All game input: care buttons, menus, Pokédex, minigames, swipes |
| Trackball | Center press wakes the screen; holding it 1.75 s turns the screen off. Movement keeps the screen awake. It does not navigate menus |
| Keyboard | Only for typing a Wi-Fi password |
| microSD (SPI) | Animated sprites for all 151 + shinies, read from `/mons/`. Without a card the game uses its built-in flash sprites |
| Speaker (I2S) | The game's sound effects. No microphone |
| Battery | Voltage and an approximate percentage (no charging indicator) |
| GPS (Plus only) | Sets the clock from satellite UTC (no position fix needed) |
| Wi-Fi | Optional NTP time sync with saved networks; the radio is off otherwise |
| LoRa, Bluetooth, deep sleep | Not supported. Deep sleep is designed but not built ([design](TDECK_PLUS_DEEP_SLEEP_DESIGN.md)) |

Only the **T-Deck Plus** is targeted. The original T-Deck (without GPS) uses a different display start-up path and
hasn't been tried.

## Differences from upstream

- **New board target.** A board layer (`src/board/`) separates the hardware. `TAMAPOKE_TDECK_PLUS=1` selects the
  T-Deck Plus; the Waveshare build is unchanged and still the default.
- **Landscape interface** for 320×240: compact home screen with side needs bars and four care buttons, plus landscape
  versions of the profile card, Pokédex grid and detail, medals, settings, nickname keyboard, strength training and
  the ball minigame. Gestures are the same as upstream (swipe up for the card, down for settings, sideways for pages).
- **Time without an RTC chip.** GPS or Wi-Fi/NTP sets the clock. A manual clock with an optional, confirmed catch-up
  is also available. A worldwide list of timezone presets with daylight-saving rules.
- **Settings pages** for Time, Wi-Fi and System (screen timeout, sound, language, battery).
- **Unchanged:** species, evolution, care, training, Pokédex, the save format and the sprite packs. Saves stay in the
  device's flash and survive firmware updates done as described below.

## Installation

Upstream's one-click web installer doesn't support this board. Flash the firmware from a computer:

1. Download `TamaPoke-TDeckPlus-1.16-tdeck.1-firmware.zip` and unzip it.
2. Connect the T-Deck Plus by USB-C. If it doesn't show up as a serial port, put it into download mode: hold the
   trackball down, then turn the device on (or press reset), then release.
3. Flash it with one of these:
   - **esptool** (Python), keeps your saved pet:
     ```text
     esptool --chip esp32s3 --port COM5 write-flash 0x0 TamaPoke.ino.bootloader.bin 0x8000 TamaPoke.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 TamaPoke.ino.bin
     ```
   - **Espressif's browser flasher** ([esptool-js](https://espressif.github.io/esptool-js/), Chrome or Edge): add the
     same four files at the same addresses. Not yet tried with this release.
   - **First installation only:** `TamaPoke-TDeckPlus-1.16-tdeck.1-merged.bin` at address `0x0`. It overwrites the
     save area, so never use it to update a device with a pet you want to keep.
4. Press reset. To fill the microSD card, copy this repository's `tools/sdcard/mons/` folder to the root of a
   FAT32 card as `/mons/`, then insert it.

`FLASHING.txt` in the zip repeats these steps. Don't erase the flash when updating: the pet is stored at `0x9000`.

### If something goes wrong

- **No serial port:** use a data USB-C cable and enter download mode (step 2).
- **Black screen after flashing:** open a serial monitor at 115200 baud and press reset. `DISPLAY ... FAIL` or
  `TOUCH ... FAIL` points at the failing part. The [bring-up notes](TDECK_PLUS_BRINGUP.md#expected-serial-output) list
  the expected start-up log.
- **Sprites missing or plain:** the card isn't mounted (`SD ... FAIL` in the log) or `/mons/` is missing. The game
  still runs with its flash sprites.
- **Clock wrong:** pick your timezone under Settings → Time. GPS needs a clear view of the sky; Wi-Fi time needs a
  saved network.

## Using it

- **Care:** the four buttons under the scene are Feed (berries or candy), Play (the ball minigame), Lights (sleep)
  and Bath. Tap the Pokémon to pet it; tap an egg to help it hatch.
- **Header icons:** Profile, Pokédex and Settings. Swipe up for the profile card, down for settings.
- **Screen:** it dims, then turns off after the timeout (2 minutes by default). Press the trackball to wake it.
  The game and the clock keep running while the screen is off.
- How the game itself works (needs, evolution, training, medals) is described in upstream's manual, kept in
  [docs/UPSTREAM_README.md](docs/UPSTREAM_README.md#game-manual-the-actual-numbers).

## Building from source

Arduino CLI with ESP32 core 3.3.10, GFX Library for Arduino 1.6.7, SensorLib 0.4.1, XPowersLib 0.3.3 and U8g2 2.36.19:

```text
python tools/build_firmware.py tdeck-plus
```

The output lands in `build/tdeck-plus/`. The branch for this edition is `codex/tdeck-plus-port`; `main` tracks
upstream. The release `v1.16-tdeck.1` was built from commit `ebd66ff`.

## Credits and license

- **TamaPoke** by Quique Tortosa ([socquique/TamaPoke](https://github.com/socquique/TamaPoke)): the game, its
  engine, tools and the Waveshare firmware. Upstream's README is in [docs/UPSTREAM_README.md](docs/UPSTREAM_README.md).
- **Sprites:** [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab), CC BY-NC 4.0, non-commercial use only.
  Pokémon and all related names are © Nintendo / Game Freak / The Pokémon Company. See [CREDITS.md](CREDITS.md).
- **T-Deck Plus hardware reference:** [LilyGO T-Deck](https://github.com/Xinyuan-LilyGO/T-Deck) examples.

The code is MIT ([LICENSE](LICENSE)), as upstream's. The sprite packs keep their own non-commercial license.
