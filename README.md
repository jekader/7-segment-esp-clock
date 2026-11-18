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
- Optional BME280 environmental monitoring and LDR automatic brightness using repurposed button connections.

## Installation

Prebuilt images are available on the [releases page](https://github.com/jekader/7-segment-esp-clock/releases), so installing the build toolchain is optional.

| Release file | Purpose |
| --- | --- |
| `7-segment-esp-clock-firmware.bin` | Flashable ESP8266 firmware. Upload this file through an existing clock's `/update` page, or write it at address `0x0` with `esptool`. |
| `7-segment-esp-clock-firmware.elf` | Matching linked image with debugging symbols. Use it for diagnostics, address-to-source lookup, and emulation; do not upload it through the OTA page. |
| `SHA256SUMS` | SHA-256 checksums for the BIN and ELF files. |

To verify both downloads on Linux (or another system with GNU coreutils), place all three files in the same directory and run:

```sh
sha256sum --check SHA256SUMS
```

For a clock already running firmware with browser OTA support, open **http://CLOCK_IP/update**, select **Firmware**, and upload `7-segment-esp-clock-firmware.bin`. The clock restarts after a successful update. In the portal's **Firmware update** section, **Check latest release** uses the browser's Internet connection to query GitHub, shows the installed and latest versions, and links directly to the matching OTA BIN. Download that file and select it in the updater. The releases page remains available if GitHub cannot be reached.

For a first installation over USB serial, put the clock into its ESP8266 flashing bootloader: disconnect power, hold **SET**, reconnect power, and release **SET** after the status LED blinks once. Then run (adjusting the port as needed):

```sh
esptool --port /dev/ttyUSB0 write-flash 0x0 7-segment-esp-clock-firmware.bin
```

Before replacing original firmware, consider making a backup:

```sh
esptool --port /dev/ttyUSB0 read-flash 0 0x100000 original-firmware.bin
```

Flashing this hardware can damage or brick the clock; proceed at your own risk and keep USB serial access available for recovery.

## Hardware

This firmware targets the unmodified 303WIFILC01:

| Function | Hardware |
| --- | --- |
| MCU | ESP8266, 1 MB flash (`esp01_1m`, 64 KB LittleFS) |
| RTC | DS1302: CLK GPIO16, IO GPIO14, CE GPIO5 |
| Display | TM1650 over I²C: SDA GPIO13, SCL GPIO12 |
| SET button | GPIO0, active low |
| Date/UP button | GPIO4, active low |
| Brightness/DOWN button | GPIO15, active high |
| Status LED | GPIO2, active low |
| Optional BME280 | SDA GPIO4 (UP), SCL GPIO3 (P1.RX), 3.3 V |
| Optional LDR | TOUT/A0 (ESP8266EX pin 6), 0–1.0 V maximum |

## Configuration

On first boot, or when Wi-Fi remains unavailable for 30 seconds, the clock starts an open AP named `7SEG-XXXXXX`. Connect to it and open **http://192.168.4.1/**. Captive-network detection should open this page automatically.

Holding **SET** for three seconds also enables the AP. The AP stops automatically whenever a station Wi-Fi connection is established; use the assigned station IP to access the portal afterward.

The settings page changes take effect after a short automatic restart. Brightness and display-power changes apply and save immediately. Configuration is stored in LittleFS; Wi-Fi and MQTT passwords are **not encrypted** in flash or in the local configuration page.

### Browser OTA updates

Open **http://CLOCK_IP/update** and select an image type and file. The default **Firmware** option accepts a release `7-segment-esp-clock-firmware.bin` or local `.pio/build/303WIFILC01/firmware.bin` image (`.bin` or `.bin.gz`). Do not upload the ELF or checksum file. Keep the page open and the clock powered on until it reboots; the page and UART log both report the upgrade's progress and result.

The advanced **LittleFS filesystem** option replaces the 64 KB configuration partition. Only select it when intentionally replacing Wi-Fi, MQTT, and clock settings. Build a compatible image with `pio run --target buildfs` and select `.pio/build/303WIFILC01/littlefs.bin`; this is not a normal firmware update.

Anyone who can reach the portal can update the firmware, so use it only on a trusted network.

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

### Connecting sensors: reusing controls for useful I/O

The 303WIFILC01 has no spare GPIO pins on its programming header. However, its UP and DOWN buttons have limited everyday value once the clock can be configured and controlled through the web portal and Home Assistant. Repurposing their connections gives the clock useful environmental inputs without adding another controller. SET remains available for setup and recovery.

Enable **“I have sensor(s) connected”** in the portal and select the installed hardware. This disables the normal UP and DOWN button actions; corresponding MQTT button events are still reported. Supported configurations are:

- **BME280:** SDA on the UP/GPIO4 pad, SCL on P1.RX/GPIO3, external I²C pull-ups, and 3.3 V power.
- **LDR automatic brightness:** a bounded divider connected directly to the ESP8266's TOUT/A0 input, configurable dark/bright calibration, and a **0–1.0 V maximum input**.

See **[Sensor connections](docs/sensor-connections.md)** for wiring diagrams, installation steps, precautions, and calibration. More electrical background is in [Clock pinouts and sensor research](docs/clock-pinouts.md).

### Timezone and DST

The POSIX timezone field accepts values such as:

- `UTC0` (UTC with no DST)
- `CET-1CEST,M3.5.0,M10.5.0/3` (Central Europe with automatic DST)
- `EST5EDT,M3.2.0,M11.1.0` (US Eastern with automatic DST)

Complete daylight-saving rules switch automatically; fixed-offset settings do not switch.

### Buttons

- **SET (hold 3 s):** open the configuration AP until station Wi-Fi connects.
- **UP/date (press):** toggle between time and day/month when the sensor override is disabled.
- **DOWN/brightness (press):** cycle through levels 1–8 and save the selected level when automatic brightness is disabled.

## Building from source

Install [PlatformIO](https://platformio.org/install), clone the repository, and run:

```sh
pio run
pio run --target upload
pio device monitor
```

The build writes `.pio/build/303WIFILC01/firmware.bin`, which can be uploaded through the browser OTA page. Hand-built images identify themselves as **v0.0.0**. CI replaces this with the exact Git tag (for example, `v7.1.0`) when building a release and verifies that the tag is embedded in the ELF.

### CI releases

Pull requests receive a PlatformIO build check and downloadable workflow artifacts. Version tags matching `v*.*.*` (for example, `v7.1.0`) also create a GitHub release containing the OTA BIN, matching ELF, and SHA-256 checksums. These are the files linked from [Installation](#installation).

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

## Development

This code was built using **Kimi K3.1 Pro**.

## Attribution

Hardware definitions, display segment mapping, font, and DS1302 setup were ported from Maarten Pennings and Utyff's [303WIFILC01 project](https://github.com/maarten-pennings/303WIFILC01).

## License

This project is released into the public domain under [The Unlicense](LICENSE).
