# ClickerBot — Anti-stress clicker

Русская версия: **[README.ru.md](README.ru.md)**

<img src="img/ClickerBot.png" alt="ClickerBot" width="360"><img src="img/hand.png" alt="ClickerBot in hand" width="158">

An anti-stress clicker built on **ESP32-C3**: a tiny 72×40 OLED, a tactile button, the buttons on the board (`BOOT` — the menu and `RESET` — for flashing), seven animated "skins", per-skin click statistics, a battery gauge with automatic deep sleep — plus two local multiplayer games over **ESP-NOW** (no router, no internet).


> **A ready-made device** can be bought on the project website — **[clickerbot.net](https://clickerbot.net/)**. The firmware here is the same device code without the online leaderboard, so a self-built clicker works completely offline; an assembled one additionally syncs clicks there.

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

- **Statistics screen**: a marquee with every skin's counters, the grand total and
  the battery level.
- **Local setup page** served by the device itself (no app needed): choose a skin,
  set a nickname, scan and join networks, watch the counters.
- **Battery monitoring** on a 47k/47k divider: percentage on the splash screen,
  crossed-out battery below 5 % and immediate deep sleep — otherwise the ESP32
  gets stuck in brownout reset loops.
- **Power saving**: 20 s without activity → minimal picture, 20 s more → deep
  sleep; waking up is a click on the main button (with a splash screen).
- **"Ready to flash" screen**: hold the menu (`BOOT`) button for 5 s before
  pressing `RESET`, so you can see when the chip is safe to flash.

## Hardware

| Part | Item |
|---|---|
| Microcontroller | ESP32-C3 0.42-Inch OLED White Light Display Development Board |
| Click button | Cherry MX Gateron Mechanical Keyboard |
| Charge controller | TP4056 Lithium Battery Charger Module |
| Battery | Li-Pol 402030 200 mAh 3.7 V |
| Battery sense | 47k/47k resistor divider: mid-point → `GPIO0` (ADC), low side → `GPIO20` |

Assembly diagram, mounting drawing, wiring table, divider diagram and electrical
notes: **[docs/HARDWARE.md](docs/HARDWARE.md)**.

## Quick start

**Flash the ready-made image** (nothing to install but `esptool`) — grab
[`firmware/clickerbot-2.1-diy-merged.bin`](firmware/clickerbot-2.1-diy-merged.bin)
and write it at offset `0x0`:

```bash
esptool.py --chip esp32c3 --baud 460800 write_flash 0x0 clickerbot-2.1-diy-merged.bin
```

**Or build from source:**

1. Install [PlatformIO](https://platformio.org/) (VS Code extension, or
   `pip install platformio`).
2. Build and flash from the repository root:

   ```bash
   pio run -t upload
   ```

3. Optional: `pio device monitor -b 115200` to see the version line.

The ready-made image, `esptool` instructions, partition layout and troubleshooting:
**[firmware/README.md](firmware/README.md)** and **[docs/BUILD.md](docs/BUILD.md)**.

## Game
| Button | Action | Result |
|---|---|---|
| Click (`GPIO3`) | click | skin interaction (pop, burst, roll…) |
| Click | hold 3 s | statistics screen |
| Click | hold 1 s, in Duel / KOTH | challenge an opponent / start a round |
| Click | short release, in Duel / KOTH | page through opponents / accept |
| — | 20 s idle | minimal picture, then deep sleep |

## Controls
| Button | Action | Result |
|---|---|---|
| Menu / `BOOT` (`GPIO9`) | hold 1 s | open the menu, or confirm the selection and exit |
| Click | hold, in the menu | Wi-Fi screen |
| Menu / `BOOT` | hold 5 s | "ready to flash" screen (then `BOOT` + `RESET`) |
| `RESET` | press | reboot; hold `BOOT`, press and release `RESET`, then release `BOOT` to enter the flashing mode |

In the Wi-Fi screen a click switches between access point and client mode.

## Wi-Fi setup screen
- **Access point**: SSID `ClickerBot_XXXX` (last two bytes of the MAC), WPA2
  password `12345678`, page at `http://192.168.4.1`.
- **Client**: joins a saved network and serves the same page on its DHCP address.
- The screen closes by itself after 5 minutes without activity, to save power.

## Project layout
```
src/
  main.cpp        modes (skin / menu / stats / Wi-Fi), setup and loop
  Config.h        pins and timings
  hal/            display, buttons, battery, NVS settings, sleep
  skins/          one class per skin + SkinRegistry
  fx/             effects (cracks, shards, sparkles) and the pop counter
  menu/           skin menu, statistics, Wi-Fi screen, flash screen
  duel/           ESP-NOW transport and packet protocol
  web/            minimal HTTP server for the setup page
  util/           small helpers
```

## Adding a skin

Implement the `Skin` interface (`src/skins/Skin.h`) and add one line to
`SkinRegistry.cpp`. The menu, the statistics screen and the web page pick the new
skin up automatically through `counterKey()` and `statsSubCounter()`.

## Gotchas worth knowing

- **Flash must run at 40 MHz in DIO mode.** At 80 MHz some ESP32-C3 modules with
  a noname flash chip (JEDEC-ID `0x464016`) boot and work, but silently refuse to
  erase or write from the application: nothing is persisted (NVS cannot even
  create a namespace). This is already fixed in `platformio.ini`.
- The partition table is custom — one 2.5 MB application partition, **no OTA** —
  so keep the `partitions.csv` from the repository.
- Flashing does not erase NVS: counters, the nickname and Wi-Fi settings survive
  an update.
- The firmware prints one version line to UART0 at boot; on boards whose USB port
  is the native USB-Serial-JTAG you only see it with
  `-D ARDUINO_USB_CDC_ON_BOOT=1`.
- A build from this repository reports version **`2.1-diy`** (the same string sits
  in the `.bin` as `CLICKERFW:2.1-diy`), so a self-built device is easy to tell
  apart from an assembled one, which reports `2.1`.

## Outside this repository

The online leaderboard, device provisioning and the production flasher are **not**
published. The firmware is intentionally self-contained: a device built by hand
works on its own. The leaderboard — and the assembled device — live on the project
website, **[clickerbot.net](https://clickerbot.net/)**.

## License

MIT — see [LICENSE](LICENSE). No third-party code is bundled, but the firmware
uses [U8g2](https://github.com/olikraus/u8g2) (BSD-2-Clause) and the Arduino
ESP32 core.

The device is a DIY project: build it and use it at your own risk.
