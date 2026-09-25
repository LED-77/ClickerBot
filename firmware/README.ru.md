# Готовый образ прошивки

Бинарники для тех, кто не хочет ставить PlatformIO. Образ здесь **единый**:
загрузчик, таблица разделов и приложение в одном файле, заливается по адресу
`0x0`.

English version: **[README.md](README.md)**

| Файл | Прошивка | Размер | SHA-256 |
|---|---|---|---|
| `clickerbot-2.1-diy-merged.bin` | 2.1-diy | 985 568 Б | см. `SHA256SUMS` |

## Как прошить

Через [`esptool`](https://docs.espressif.com/projects/esptool/en/latest/esp32c3/installation.html)
(`pip install esptool`) на Linux, macOS или Windows:

```bash
esptool.py --chip esp32c3 --baud 460800 write_flash 0x0 clickerbot-2.1-diy-merged.bin
```

Совсем без Python? Тот же файл прошивает **Flash Download Tool** от Espressif
(GUI под Windows): выберите чип `ESP32-C3`, добавьте `.bin` по адресу `0x0`,
поставьте `DIO` / `40MHz` / `4MB` и нажмите Start.

Если чип не определяется, переведите его в режим загрузчика вручную: удержите
кнопку меню (`BOOT`), нажмите и отпустите `RESET`, затем отпустите `BOOT`.
Прошивка помогает: удержание кнопки меню 5 с перед сбросом рисует статичный экран
«готов к прошивке», который висит на OLED всю заливку.

## Проверка скачанного файла

```bash
sha256sum -c SHA256SUMS        # Linux / macOS
```

```powershell
# Windows PowerShell
$h = (Get-FileHash .\clickerbot-2.1-diy-merged.bin -Algorithm SHA256).Hash.ToLower()
(Get-Content .\SHA256SUMS).Split(" ")[0] -eq $h
```

## Полезно знать

- Прошивка этого образа **не** стирает NVS: счётчики кликов, ник и сохранённые
  сети Wi-Fi переживают обновление. Чтобы начать с чистого устройства, сначала
  выполните `esptool.py --chip esp32c3 erase_flash`.
- Образ собран ровно из этого репозитория (версия `2.1-diy`), поэтому в баннере
  UART он сообщает `2.1-diy`.
- Хотите собрать самому? Смотрите [../docs/BUILD.ru.md](../docs/BUILD.ru.md).
