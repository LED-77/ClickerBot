# Building and flashing

The project is a plain [PlatformIO](https://platformio.org/) project — no custom
scripts, no vendored sources. Everything needed is in the repository.

## Requirements

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/) 6.x
  (`pip install platformio`, or the VS Code PlatformIO extension)
- Python 3.8+ (comes with PlatformIO)
- An ESP32-C3 board with **4 MB flash** and a USB connection

Dependencies are fetched automatically: `olikraus/U8g2 @ ^2.34.14` plus the
`espressif32` platform and the Arduino framework.

## Build

```bash
pio run              # build
pio run -t clean     # wipe the build directory
```

Artifacts land in `.pio/build/esp32-c3-devkitm-1/`:

| File | Flash offset | Size (v2.1-diy) |
|---|---|---|
| `bootloader.bin` | `0x0` | ~12 KB |
| `partitions.bin` | `0x8000` | 3 KB |
| `firmware.bin` | `0x10000` | ~900 KB |

Current resource usage of the application: **883 KB of the 2.5 MB** app partition
(33.7 %), **49 KB RAM** (15.1 %).

The build reports version `2.1-diy` on the UART banner, and the same string sits
in the binary as `CLICKERFW:2.1-diy` (useful when reporting a problem).

## Flash over USB

```bash
pio run -t upload
pio device monitor -b 115200
```

If the upload times out waiting for the chip, put the board into download mode by
hand: hold the menu (`BOOT`) button, press `RESET`, release `RESET`, then release
`BOOT`. The firmware helps with this too — holding the menu button for 5 s draws a
static "ready to flash" screen that survives `RESET` (see
[HARDWARE.md](HARDWARE.md#flashing-the-chip)).

## Flash without PlatformIO (`esptool`)

```bash
esptool.py --chip esp32c3 --baud 460800 write_flash \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x0     bootloader.bin \
  0x8000  partitions.bin \
  0x10000 firmware.bin
```

Or build a single image and flash it at `0x0`:

```bash
esptool.py --chip esp32c3 merge_bin -o clickerbot-2.1-diy-merged.bin \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin

esptool.py --chip esp32c3 --baud 460800 write_flash 0x0 clickerbot-2.1-diy-merged.bin
```

Both variants leave the `nvs` partition untouched, so click counters, the
nickname and saved Wi-Fi networks survive the update. Use `erase_flash` only if
you actually want to reset the device to factory defaults.

## Partition layout

`partitions.csv` merges the two OTA slots of the stock `default` scheme into a
single 2.5 MB application partition (the firmware has no OTA support):

| Name | Type | Offset | Size |
|---|---|---|---|
| `nvs` | data/nvs | `0x9000` | 20 KB |
| `otadata` | data/ota | `0xe000` | 8 KB |
| `app0` | app/factory | `0x10000` | 2.5 MB |
| `spiffs` | data/spiffs | `0x290000` | 1.4 MB (unused, reserved) |

> Do not swap in another partition table: the application is linked for this
> layout, and a smaller app partition will no longer fit the build.

## The 40 MHz + DIO rule

`platformio.ini` forces:

```ini
board_build.flash_mode = dio
board_build.f_flash = 40000000L
```

**Do not raise this to 80 MHz.** ESP32-C3 modules with a noname flash chip
(JEDEC-ID `0x464016`, 4 MB were seen in the wild) boot perfectly at 80 MHz, but
from the application they can no longer erase or write the flash: the device
runs, though nothing is persisted — NVS cannot even create a namespace, so
counters stay at zero and Wi-Fi settings are never saved. ROM reads in that state
can also hang the chip until it resets.

40 MHz in DIO mode is the most compatible configuration and is the same one the
ROM bootloader uses, which makes it a safe default for handmade hardware.

## Continuous integration

`.github/workflows/build.yml` builds the firmware on every push and pull request
and uploads `firmware.bin` as a build artifact. If a change breaks the build, the
CI status makes it obvious before anyone flashes a device.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Device works, but counters/Wi-Fi settings are never saved | the flash-chip issue above — check `board_build.flash_mode` / `f_flash` and the actual JEDEC-ID with `esptool.py flash_id` |
| `Timed out waiting for packet header` on upload | the chip is not in download mode: hold `BOOT`, press `RESET`, release, then retry |
| Nothing appears in the serial monitor | `Serial` is UART0 (TX on `GPIO21`); with a native USB port add `-D ARDUINO_USB_CDC_ON_BOOT=1` |
| Display stays black | check `SDA = GPIO5`, `SCL = GPIO6` and the I2C address (`0x3C` by default, `0x3D` on some modules) |
| Battery always shows 100 % or 0 % | the divider mid-point must go to `GPIO0`, the divider's low side to `GPIO20` |
| Device reboots in a loop after a deep-sleep wake | the cell is nearly empty — see the critical-level handling in `Config.h` |
