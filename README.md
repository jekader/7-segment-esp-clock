# 7 Segment ESP Clock

A PlatformIO firmware for the **303WIFILC01** clock, ported from [jekader/dCLC](https://github.com/jekader/dCLC). It retains the original ESP8266, DS1302 RTC, TM1650 four-digit display, and buttons while replacing the application structure with small, non-blocking modules.

## Features

- Restores local time from the DS1302 at boot and periodically writes NTP time back to the RTC.
- Configurable NTP servers, POSIX timezone, sync interval, 12/24-hour display, and flashing or solid colon.
- Optional periodic date display, disabled by default, with configurable duration and interval in seconds.
- Scrolls the assigned IP address once after the first network connection following boot.
- Responsive local web UI with live time, connection, RTC, and synchronization status.
- Eight native TM1650 brightness levels, controlled from the UI or the brightness button.
- Open captive portal for Wi-Fi setup; no internet-hosted web assets.
- On-demand Wi-Fi network scanning with signal strength and one-click SSID selection.
- Browser-based OTA firmware updates at `/update`.
- Automatic configuration AP after 30 seconds without Wi-Fi, or on demand by holding **SET** for three seconds.
- Full printable seven-segment ASCII font and a scrolling, timed message interface for future integrations.
- Optional hardware override for an LDR on A0 (automatic brightness) and a BME280 temperature/humidity/pressure sensor.

## Hardware

This firmware targets the unmodified 303WIFILC01:

| Function | Hardware |
| --- | --- |
| MCU | ESP8266, 1 MB flash (`esp01_1m`, 64 KB LittleFS) |
| RTC | DS1302: CLK GPIO16, IO GPIO14, CE GPIO5 |
| Display | TM1650 over I²C: SDA GPIO13, SCL GPIO12 |
| SET button | GPIO0, active low |
| Date button | GPIO4, active low |
| Brightness button | GPIO15, active high |
| Status LED | GPIO2, active low |
| Optional BME280 | SDA GPIO4 (UP), SCL GPIO3 (P1.RX), 3.3 V |
| Optional LDR | TOUT/A0 (ESP8266EX pin 6), 0–1.0 V maximum |

## Build and upload

Install [PlatformIO](https://platformio.org/install), then:

```sh
pio run
pio run --target upload
pio device monitor
```

Before replacing original firmware, consider backing up the flash (adjust the serial port):

```sh
esptool -p /dev/ttyUSB0 read_flash 0 0x100000 original-firmware.bin
```

To enter the ESP8266 flashing bootloader, disconnect power, hold **SET**, reconnect power, and release **SET** after the status LED blinks once. Flashing this hardware can damage or brick the clock; proceed at your own risk.

## Configuration

On first boot, or when Wi-Fi remains unavailable for 30 seconds, the clock starts an open AP named `7SEG-XXXXXX`. Connect to it and open **http://192.168.4.1/**. Captive-network detection should open this page automatically.

Holding **SET** for three seconds also enables the AP. The AP stops automatically whenever a station Wi-Fi connection is established; use the assigned station IP to access the portal afterward.

The settings page changes take effect after a short automatic restart. Brightness changes apply and save immediately. Configuration is stored in LittleFS; the Wi-Fi password is **not encrypted** in flash or in the local configuration page.

### Browser OTA updates

Open **http://CLOCK_IP/update**, select **Firmware**, and upload `.pio/build/303WIFILC01/firmware.bin`. No password is required.

Only upload the filesystem image if you intentionally want to replace the configuration filesystem. The updater restarts the clock after a successful upload. Anyone who can reach the portal can update the firmware, so use it only on a trusted network. Keep USB serial access available for recovery.

The firmware uses a 64 KB LittleFS layout to preserve enough contiguous space for OTA updates.

### Optional sensor hardware override

The unmodified board is the default: UP toggles the date and DOWN changes brightness. After modifying the hardware, enable **“I have sensor(s) connected”** in the UI and select the installed sensors. This disables UP and DOWN while preserving SET for portal access.

- **LDR:** connect the bounded 0–1.0 V divider documented in `docs/clock-pinouts.md` to TOUT/A0. Automatic brightness uses filtered readings and the UI's dark/bright calibration values. Automatic changes do not rewrite flash.
- **BME280:** desolder UP and connect SDA to GPIO4 at its pad. Connect SCL to GPIO3 on P1.RX, power the sensor at 3.3 V, and provide external I²C pull-ups. GPIO3/RX is used by serial flashing, so disconnect SCL if it interferes with flashing; TX logging remains available. GPIO15 was deliberately not selected because its 1 kΩ pull-down makes it a poor I²C line.

Temperature, humidity, pressure, light level, and BME280 detection status appear in the web UI.

The POSIX timezone field accepts values such as:

- `UTC0`
- `CET-1CEST,M3.5.0,M10.5.0/3` (Central Europe)
- `EST5EDT,M3.2.0,M11.1.0` (US Eastern)

### Buttons

- **SET (hold 3 s):** open the configuration AP until station Wi-Fi connects.
- **Date (press):** toggle between time and day/month.
- **Brightness (press):** cycle through levels 1–8 and save the selected level.

## Module layout and future MQTT/messages

- `Config`: validated, persistent application settings.
- `RtcClock`: failure-tolerant DS1302 adapter.
- `Tm1650Display`: pin, segment-remapping, font, and brightness hardware adapter.
- `DisplayController`: owns time/date versus scrolling-message content policy.
- `WebPortal`: web UI, captive DNS, AP lifecycle, and settings writes.
- `Sensors`: filtered LDR automatic brightness and BME280 readings.
- `Buttons`: debounced button events and SET hold detection.
- `main.cpp`: Wi-Fi/NTP/RTC orchestration only.

A future MQTT adapter can display ASCII without changing the display driver or clock rendering logic:

```cpp
display.showMessage(message, 15000); // scroll for 15 s, then restore clock
display.showMessage(message, 0);     // persistent until clearMessage()
display.clearMessage();
```

The adapter should remain non-blocking and call its own `loop()` from the main loop. MQTT is deliberately not enabled yet, so the current firmware has no broker credentials or network dependency beyond NTP.

## Hardware research

See [`docs/clock-pinouts.md`](docs/clock-pinouts.md) for the complete P1/GPIO map and evaluated sensor-expansion options. The short version is that GPIO3/RX is the easiest one-wire sensor connection, while GPIO4 at UP/S2 is cleaner if that button can be repurposed.

## Attribution

Hardware definitions, display segment mapping, font, and DS1302 setup were ported from `jekader/dCLC`, which credits Maarten Pennings and Utyff and their [303WIFILC01 work](https://github.com/maarten-pennings/303WIFILC01).
