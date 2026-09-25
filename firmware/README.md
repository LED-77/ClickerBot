# Ready-to-flash firmware

Русская версия: **[README.ru.md](README.ru.md)**

Prebuilt binaries for anyone who does not want to install PlatformIO. The image
here is a **merged** one: bootloader, partition table and the application in a
single file, flashed at offset `0x0`.

| File | Firmware | Size | SHA-256 |
|---|---|---|---|
| `clickerbot-2.1-diy-merged.bin` | 2.1-diy | 985 568 B | see `SHA256SUMS` |

## Flash it

With [`esptool`](https://docs.espressif.com/projects/esptool/en/latest/esp32c3/installation.html)
(`pip install esptool`) on Linux, macOS or Windows:

```bash
esptool.py --chip esp32c3 --baud 460800 write_flash 0x0 clickerbot-2.1-diy-merged.bin
```

No Python at all? Espressif's **Flash Download Tool** (Windows GUI) can flash the
same file: pick the chip `ESP32-C3`, add the `.bin` at address `0x0`, set
`DIO` / `40MHz` / `4MB` and press Start.

If the chip is not detected, put it into the download mode by hand: hold the menu
(`BOOT`) button, press and release `RESET`, then release `BOOT`. The firmware
helps here — holding the menu button for 5 s before the reset draws a static
"ready to flash" screen that stays on the OLED during the whole upload.

## Verify the download

```bash
sha256sum -c SHA256SUMS        # Linux / macOS
```

```powershell
# Windows PowerShell
$h = (Get-FileHash .\clickerbot-2.1-diy-merged.bin -Algorithm SHA256).Hash.ToLower()
(Get-Content .\SHA256SUMS).Split(" ")[0] -eq $h
```

## Good to know

- Flashing this image does **not** erase NVS: click counters, the nickname and
  saved Wi-Fi networks survive an update. To start from a clean device, run
  `esptool.py --chip esp32c3 erase_flash` first.
- The image is built from exactly this repository (version `2.1-diy`), so it
  reports `2.1-diy` in the UART banner.
- Prefer building it yourself? See [../docs/BUILD.md](../docs/BUILD.md).
