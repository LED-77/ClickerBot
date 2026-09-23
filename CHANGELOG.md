# Changelog

All notable changes to this project are documented in this file.

## [2.1-diy] — first public release

This release is the **offline** build of the device firmware: everything that
belonged to the hosted service was removed, so a self-built device works with no
server at all. The version string carries a `-diy` suffix, so a self-built device
is easy to tell apart from an assembled one (which reports `2.1`).

### Added

- Bilingual comments (Russian + English) throughout the firmware.
- One-line version banner on UART0 in `setup()`: `ClickerBot fw 2.1-diy`.
- Version marker in the binary: `CLICKERFW:2.1-diy`.
- Documentation: hardware wiring, build and flashing instructions, ESP-NOW
  protocol reference.
- GitHub Actions workflow that builds the firmware on every push.

### Changed

- **Flash configuration fixed: 40 MHz + DIO** (`board_build.flash_mode` /
  `board_build.f_flash`). At 80 MHz some ESP32-C3 modules with a noname flash
  chip (JEDEC-ID `0x464016`) boot and run, but cannot erase or write their flash
  from the application: nothing was persisted — counters stayed at zero and Wi-Fi
  settings were lost.
- Statistics screen: after the summary and battery level have been shown it
  returns to the skin by itself (previously it waited for the network sync that
  no longer exists).
- The web setup page no longer offers a "Sync now" button or nickname
  availability checks, and the nickname is now used only for ESP-NOW games.
- Removed the unused `ArduinoJson` dependency.

### Removed

- Device provisioning (secret written into a flash slot, UART `PROV:` command).
- Cloud sync (`SyncClient`, HMAC signing, packet counters) and the hardcoded
  service host. The firmware makes no outgoing network requests.
- `Provision` / `SyncClient` module pair, the `/api/sync` endpoint, the device
  secret and packet number entries in NVS.

## Earlier versions

Development before the public repository (internal versions up to 2.0, including
the first multi-skin UI, the battery gauge and the ESP-NOW games) is not tracked
here.
