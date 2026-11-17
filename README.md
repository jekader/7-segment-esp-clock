# 7 Segment ESP Clock

A PlatformIO firmware for the **303WIFILC01** clock, ported from [maarten-pennings/303WIFILC01](https://github.com/maarten-pennings/303WIFILC01). It retains the original ESP8266, DS1302 RTC, TM1650 four-digit display, and buttons while replacing the application structure with small, non-blocking modules.

## Features

- Restores local time from the DS1302 at boot and periodically writes NTP time back to the RTC.
- Configurable NTP servers, POSIX timezone, sync interval, 12/24-hour display, and flashing or solid colon.
- Optional periodic date display, disabled by default, with configurable duration and interval in seconds.
- Scrolls the assigned IP address once after the first network connection following boot, using digit dots to save space.
- Responsive local web UI with live time, connection, RTC, synchronization, uptime, and last reboot reason.
- Eight native TM1650 brightness levels, controlled from the UI or the brightness button.
- Open captive portal for Wi-Fi setup; no internet-hosted web assets.
- On-demand Wi-Fi network scanning with signal strength and one-click SSID selection.
- Browser-based OTA firmware updates at `/update`.
- Optional MQTT integration with Home Assistant discovery, environmental sensors, display power/brightness control, and button events. Disabled by default.
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

The settings page changes take effect after a short automatic restart. Brightness and display-power changes apply and save immediately. Configuration is stored in LittleFS; Wi-Fi and MQTT passwords are **not encrypted** in flash or in the local configuration page.

### Browser OTA updates

Open **http://CLOCK_IP/update**, select **Firmware**, and upload `.pio/build/303WIFILC01/firmware.bin`. No password is required.

Only upload the filesystem image if you intentionally want to replace the configuration filesystem. The updater restarts the clock after a successful upload. Anyone who can reach the portal can update the firmware, so use it only on a trusted network. Keep USB serial access available for recovery.

The firmware uses a 64 KB LittleFS layout to preserve enough contiguous space for OTA updates.

### MQTT and Home Assistant

Enable MQTT in the portal and configure the broker host, port, optional credentials, base topic, discovery prefix, and sensor interval. The default base topic is `7segclock`, discovery prefix is `homeassistant`, and sensor interval is 30 seconds. Changing MQTT settings restarts the clock.

The clock connects only while station Wi-Fi is available, reconnects automatically, and publishes retained discovery and state data. Each clock is identified by its MAC suffix. With device ID `7seg_a1b2c3` and the default base topic:

- Availability: `7segclock/7seg_a1b2c3/status` (`online`/`offline`, with offline configured as the MQTT last will).
- Sensor telemetry: `7segclock/7seg_a1b2c3/tele/SENSOR`.
- Display power state: `7segclock/7seg_a1b2c3/display/power`; send `ON` or `OFF` to the same topic plus `/set`.
- Brightness state: `7segclock/7seg_a1b2c3/display/brightness`; send an integer from 1 to 8 to the same topic plus `/set`.
- Button events: `7segclock/7seg_a1b2c3/button/set`, `.../button/date`, and `.../button/brightness`. Every button publishes non-retained `PRESS` events; SET also publishes `HOLD` after three seconds.

Home Assistant discovers a display switch, brightness number, button-press device-automation triggers, and whichever BME280 temperature, humidity, pressure, or raw-light sensors are enabled and available. MQTT display state also follows local web and button changes. Automatic LDR brightness takes priority over manual brightness commands. Display power and manual brightness changes are saved. Button events are reported even when the hardware override disables their normal Date/Brightness behavior.

MQTT uses a plain TCP connection without TLS; use a trusted local broker and network. Disabling a sensor clears its retained Home Assistant discovery entry on the next broker connection.

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

## Module layout and messages

- `Config`: validated, persistent application settings.
- `RtcClock`: failure-tolerant DS1302 adapter.
- `Tm1650Display`: pin, segment-remapping, font, and brightness hardware adapter.
- `DisplayController`: owns time/date versus scrolling-message content policy.
- `WebPortal`: web UI, captive DNS, AP lifecycle, and settings writes.
- `MqttHomeAssistant`: broker lifecycle, Home Assistant discovery, sensor/button publishing, and display commands.
- `Sensors`: filtered LDR automatic brightness and BME280 readings.
- `Buttons`: debounced button events and SET hold detection.
- `main.cpp`: Wi-Fi/NTP/RTC orchestration only.

MQTT text-message support is separate from Home Assistant integration and remains a possible future extension:

```cpp
display.showMessage(message, 15000); // scroll for 15 s, then restore clock
display.showMessage(message, 0);     // persistent until clearMessage()
display.clearMessage();
```

The adapter should remain non-blocking and call its own handler from the main loop.

## Hardware research

See [`docs/clock-pinouts.md`](docs/clock-pinouts.md) for the complete P1/GPIO map and evaluated sensor-expansion options. The short version is that GPIO3/RX is the easiest one-wire sensor connection, while GPIO4 at UP/S2 is cleaner if that button can be repurposed.

## CI releases

Pull requests receive a PlatformIO build check and workflow artifacts. Version tags matching `v*.*.*` (for example, `v7.1.0`) also create a GitHub release containing the OTA firmware BIN, matching ELF, and SHA-256 checksums.

## Attribution

Hardware definitions, display segment mapping, font, and DS1302 setup were ported from Maarten Pennings and Utyff's [303WIFILC01 project](https://github.com/maarten-pennings/303WIFILC01).

## License

This project is released into the public domain under [The Unlicense](LICENSE).
