# Third-party notices

This repository and its firmware builds combine TamaPoke's own code with third-party components. Each keeps its own
license; this page lists them so the firmware download can be redistributed correctly. It is not legal advice.

## This project

| Part | License | Notes |
|---|---|---|
| TamaPoke game code and tools | MIT, Copyright (c) 2026 Quique Tortosa ([LICENSE](LICENSE)) | Upstream [socquique/TamaPoke](https://github.com/socquique/TamaPoke) |
| T-Deck Plus port changes in this repository | MIT, as upstream (see README) | Unofficial port, not affiliated with upstream |

## Content

| Content | License | Where |
|---|---|---|
| Pokémon sprites (PMD SpriteCollab) | CC BY-NC 4.0, non-commercial; credit in [CREDITS.md](CREDITS.md) | `tools/sdcard/mons/`, `web/sprites.pak`, and the small built-in sprite set compiled into the firmware |
| Gen 1 base stats | from [PokéAPI](https://pokeapi.co) | `dex.h` and generators |
| 3D-printed case (upstream) | CC BY-NC-SA | linked from upstream's README |

Pokémon and all related names are trademarks of Nintendo / Game Freak / The Pokémon Company. Neither this
repository nor its firmware may be used commercially, because of the sprite license.

## Libraries compiled into the firmware

Versions are the ones pinned for the `v1.16-tdeck.1` build (see `TDECK_PLUS_BRINGUP.md`).

| Component | Version | License |
|---|---|---|
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) | 3.3.10 | LGPL-2.1-or-later (core sources); precompiled ESP-IDF libraries under Apache-2.0 and the licenses listed in that package |
| [ESP-IDF](https://github.com/espressif/esp-idf) (via the core) | bundled with 3.3.10 | Apache-2.0, with third-party components listed in ESP-IDF's own notices |
| [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) | 1.6.7 | BSD license (Adafruit-derived, see its `license.txt`) |
| [SensorLib](https://github.com/lewisxhe/SensorLib) | 0.4.1 | MIT, Copyright (c) 2022 lewis he |
| [XPowersLib](https://github.com/lewisxhe/XPowersLib) (Waveshare build only) | 0.3.3 | MIT, Copyright (c) 2022 lewis he |
| [U8g2](https://github.com/olikraus/u8g2) (CJK font data) | 2.36.19 | 2-clause BSD for the code; fonts carry their own licenses as listed in U8g2's `LICENSE` |

The Arduino core is LGPL-licensed and statically linked into the firmware. To exercise your LGPL rights, rebuild the
firmware from this repository's source with `tools/build_firmware.py` and the same or a modified core.
