# ClickerBot — device firmware

Firmware for an anti-stress clicker built on **ESP32-C3**: a tiny 72×40 OLED, two
buttons, seven animated "skins", per-skin click statistics, a battery gauge with
automatic deep sleep — plus two local multiplayer games over **ESP-NOW** (no
router, no internet).

Русская версия: **[README.ru.md](README.ru.md)**

This repository holds the **device firmware only**, and it is fully **offline**.
There is no account, no cloud sync and no device provisioning: except for the
Wi-Fi setup screen, the firmware talks to nothing but other clickers nearby over
ESP-NOW.

---

## Features

- **7 skins**, each with its own mechanic and its own persistent click counter
  (stored in NVS, survives deep sleep, flashing and a battery change):

  | key | Skin | Mechanic | Counter keys |
  |---|---|---|---|
  | `eye` | Eye | blinks, squints when clicked fast, cracks, explodes, respawns | `cntEye` |
  | `bubble` | Bubbles | bubble wrap: press in and pop the next bubble, fresh sheet when done | `cntBubbles` |
  | `cardio` | Cardio | ECG trace and a heart whose rate follows your clicking | `cntCardio` |
  | `odometer` | Odometer | mechanical 3-drum counter, rolls on every click, resets at 999 | `cntOdo` |
  | `bandit` | Bandit | slot machine: reels spin while you click, then stop one by one | `cntBandit` / `banditMatches` |
  | `duel` | Duel | 1v1 tug of war over ESP-NOW, 15 s rounds | `cntDuel` / `duelWins` |
  | `koth` | KOTH | King of the Hill over ESP-NOW, up to 12 players, 60 s rounds | `cntKoth` / `kothWins` |

  The menu also has a **Random** entry that picks a random solo skin on every
  wake-up (multiplayer skins are excluded).

- **Statistics screen**: a marquee with every skin's counters, the grand total
  and the battery level.
- **Local Wi-Fi setup page** served by the device itself (no app): choose a skin,
  set a nickname, scan/join networks, watch the counters.
- **Battery monitoring** on a 47k/47k divider: percentage on the splash screen,
  crossed-out icon below 5 %, immediate deep sleep to avoid brownout reset loops.
- **Power saving**: 20 s without activity → minimal picture, 20 s more → deep
  sleep; any click on the main button wakes the device up with a splash screen.
- **"Ready to flash" screen**: hold the board button for 5 s before pressing
  RESET, so you can see when the chip is safe to flash.

## Hardware

| Part | Notes |
|---|---|
| MCU | ESP32-C3 (4 MB flash), e.g. a DevKitM-1 style board |
| Display | SSD1306 OLED **72×40**, I2C: SDA `GPIO5`, SCL `GPIO6`, 400 kHz |
| Click button | `GPIO3` to GND, internal pull-up (also the deep-sleep wake source) |
| Menu button | `GPIO9` (the `BOOT` button on most boards) to GND |
| Battery sense | 47k/47k divider: mid-point → `GPIO0` (ADC), low side → `GPIO20` |

Wiring table, divider diagram and electrical notes: **[docs/HARDWARE.md](docs/HARDWARE.md)**.

## Quick start

1. Install [PlatformIO](https://platformio.org/) (VS Code extension, or
   `pip install platformio`).
2. Build and flash from the repository root:

   ```bash
   pio run -t upload
   ```

3. Optional: `pio device monitor -b 115200` to see the boot banner.

Prebuilt binaries, `esptool` instructions, partition layout and troubleshooting:
**[docs/BUILD.md](docs/BUILD.md)**.

## Controls

| Button | Action | Result |
|---|---|---|
| Click (`GPIO3`) | click | skin interaction (pol, burst, roll…) |
| Click | hold 3 s | statistics screen |
| Click | hold, in the menu | Wi-Fi screen |
| Click | hold 1 s, in Duel / KOTH | challenge an opponent / start a round |
| Click | short release, in Duel / KOTH | page through opponents / accept |
| Menu (`GPIO9`) | hold 1 s | open the menu, or confirm the selection and exit |
| Menu | hold 5 s | "ready to flash" screen (then BOOT + RESET) |
| Menu | click | exit the Wi-Fi screen |
| — | 20 s idle | minimal picture, then deep sleep |

In the Wi-Fi screen a click switches between access point and client mode.

## Wi-Fi setup screen

- **Access point**: SSID `ClickerBot_XXXX` (last two bytes of the MAC), WPA2
  password `12345678`, page at `http://192.168.4.1`.
- **Client**: joins a saved network and serves the same page on its DHCP address.
- The screen leaves by itself after 5 minutes without activity to save power.

## Project layout

```
src/
  main.cpp        app modes (skin / menu / stats / Wi-Fi), setup and loop
  Config.h        pins and timings
  hal/            display, buttons, battery, NVS settings, sleep manager
  skins/          one class per skin + SkinRegistry
  fx/             crack / shatter / sparkle effects, pop counter
  menu/           skin menu, statistics screen, Wi-Fi screen, flash screen
  duel/           ESP-NOW transport and packet protocol
  web/            minimal HTTP server for the setup page
  util/           small helpers
```

## Adding a skin

Implement the `Skin` interface (`src/skins/Skin.h`) and add one line to
`SkinRegistry.cpp`. The menu, the statistics screen and the web page pick the new
skin up automatically through `counterKey()` / `statsSubCounter()`.

## Gotchas worth knowing

- **Flash must run at 40 MHz in DIO mode.** At 80 MHz some ESP32-C3 modules with
  a noname flash chip (JEDEC-ID `0x464016`) boot and work, but silently refuse to
  erase or write from the application: nothing is persisted (NVS cannot even
  create a namespace). This is already forced in `platformio.ini`.
- The partition table is custom — one 2.5 MB application partition, **no OTA** —
  so keep the bundled `partitions.csv`.
- Flashing does not wipe NVS: counters, nickname and Wi-Fi settings survive a
  firmware update.
- The firmware prints one banner line to UART0 at boot; on boards whose USB port
  is the native USB-Serial-JTAG you need `-D ARDUINO_USB_CDC_ON_BOOT=1` to see it
  in the USB serial monitor.

## Scope

The online leaderboard, device provisioning and the production flasher are **not**
part of this repository. This firmware is intentionally self-contained, so that a
self-built device works without any server.

## License

MIT — see [LICENSE](LICENSE). Bundles no third-party source, but links against
[U8g2](https://github.com/olikraus/u8g2) (BSD-2-Clause) and the Arduino ESP32 core.

The device is a DIY project: build it and use it at your own risk.
