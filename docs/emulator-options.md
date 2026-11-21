# Open-source emulation options

## Conclusion

This firmware is for **ESP8266**, not ESP32: `platformio.ini` selects
`espressif8266@4.2.1` and the `esp01_1m` board (Arduino core 3.1.2).

The closest open-source, Wokwi-like option found is
[`digitake/esp8266-emulator`](https://github.com/digitake/esp8266-emulator) (MIT).
It runs an unmodified ESP8266 `firmware.bin` in an LX106 interpreter and provides
a local browser GUI with GPIO interaction, a signal trace, UART, boot stages,
and reset/flash controls. It also has a headless mode and timed input scripts.
It was developed around the same PlatformIO platform and Arduino core version as
this project. See its [README](https://github.com/digitake/esp8266-emulator/blob/c4c840d59b6081c01cab56c6079fc089243c294d/README.md)
and [current scope](https://github.com/digitake/esp8266-emulator/blob/c4c840d59b6081c01cab56c6079fc089243c294d/docs/STATUS.md).

It is useful for testing this firmware's boot path, configuration persistence,
GPIO/button handling, crash behavior, and general application logic. It is **not
a complete 303WIFILC01 board emulator**: no TM1650, DS1302, BME280, or usable LDR
model is provided, and its optional virtual network is not a Wi-Fi radio or
joinable SoftAP.

The project is very new and has no published releases, so pin a tested commit
and validate important behavior on hardware. This review used commit
`c4c840d59b6081c01cab56c6079fc089243c294d`.

## Verified with this firmware

The emulator build and its full `make check` suite passed. The existing firmware
image was then run for five emulated seconds at maximum speed with strict ROM
checking:

```sh
git clone https://github.com/digitake/esp8266-emulator.git
cd esp8266-emulator
git checkout c4c840d59b6081c01cab56c6079fc089243c294d
make

# Get setup()/loop() addresses from the matching ELF.
python3 -I scripts/elfsyms.py /path/to/7-segment-esp-clock/.pio/build/303WIFILC01/firmware.elf

# Substitute the two addresses printed by elfsyms.py.
build/esp8266emu \
  --flash /path/to/7-segment-esp-clock/.pio/build/303WIFILC01/firmware.bin \
  --sym-setup 0x4020a138 --sym-loop 0x4020a3d8 \
  --speed 0 --max-seconds 5 --no-stdin --quiet --strict \
  --uart-log /tmp/clock.uart --report /tmp/clock-report.json
```

The firmware reached `setup()` and `loop()` and printed:

```text
7 Segment Clock 0.2.0 (Nov 13 2026 10:12:41)
config: no saved configuration; using defaults
display: TM1650 not responding
rtc: initialization failed
wifi: no credentials; starting configuration AP
portal: CLOCK-EEB001 at 192.168.4.1
setup: complete
```

The run ended at `loop` after 4,637 `loop()` calls and 400,014,980 guest
instructions, with no reset or halt. These are deterministic emulator results,
not timings that should be expected from hardware. The missing display/RTC
responses are expected because those external devices are not modeled.

## GUI quick start

Install dependencies on Debian/Ubuntu (the first three packages are sufficient
for offline use; the remaining packages enable experimental networking):

```sh
sudo apt update
sudo apt install -y build-essential python3 git pkg-config \
  libslirp-dev libglib2.0-dev
git clone https://github.com/digitake/esp8266-emulator.git
cd esp8266-emulator
git checkout c4c840d59b6081c01cab56c6079fc089243c294d
bash run-gui.sh \
  --bin /path/to/7-segment-esp-clock/.pio/build/303WIFILC01/firmware.bin \
  --elf /path/to/7-segment-esp-clock/.pio/build/303WIFILC01/firmware.elf
```

Open http://127.0.0.1:8266/. The GUI supports clicking/driving GPIOs and has
scenario commands such as `1000 in 0 0` to drive GPIO0 low at one emulated
second. Relevant clock pin mappings are GPIO0 SET (active low), GPIO4 date
(active low), GPIO15 brightness (active high), and GPIO2 status LED
(active low); see `src/Buttons.cpp` and `src/main.cpp`.

## Feature fit

| Firmware area | Emulator usefulness / limit |
| --- | --- |
| ESP8266 boot and application logic | **Verified working** with the current image. |
| Serial output and reset behavior | Supported and verified in the smoke test. |
| Buttons and status LED | GPIO0–16 input driving, UART and pin traces are supported. A custom GPIO profile can label the clock controls. |
| LittleFS configuration | NOR flash programming/erasing and persistent full-flash state are implemented. Exact clock save/reload behavior was not separately tested. |
| TM1650 display | **Not modeled.** Its bit-banged transactions appear only as GPIO activity; there are no rendered digits and no device ACK. |
| DS1302 RTC | **Not modeled.** RTC initialization fails as it would with disconnected hardware. |
| Optional BME280 and LDR | No external BME280 model or validated ADC stimulus model; these paths need hardware or additional emulator development. |
| Station web server and NTP | An experimental libslirp virtual station supports Arduino 3.1.2/lwIP2 with a matching BIN/ELF profile. Not verified for this clock. |
| Initial/captive SoftAP | No joinable Wi-Fi SoftAP or captive-client simulation. A real AP association, WPA, scanning, roaming, and RF behavior are outside scope. |
| Electrical and timing behavior | Not modeled accurately: one instruction per cycle, ideal clocks, instant flash writes, and no electrical simulation. |

The network limits are documented in the emulator's [networking design](https://github.com/digitake/esp8266-emulator/blob/c4c840d59b6081c01cab56c6079fc089243c294d/docs/NETWORKING.md).
In particular, its adapter currently models one station interface. Testing this
clock's station-mode web UI would require preconfigured firmware/flash; the
current no-credentials default starts only the SoftAP path.

## Other projects considered

### Wokwi esp8266js

[`wokwi/esp8266js`](https://github.com/wokwi/esp8266js) is an MIT-licensed LX106
simulator from Wokwi. Its README demonstrates only an ESP8266 RTOS SDK
`hello_world`; its CI checks that example. The source includes GPIO, UART,
timers, SPI flash, ADC and other partial blocks, but no working Wi-Fi stack or
external clock/display device models. It is an interesting CPU/emulator base,
not a ready end-to-end target for this firmware.

Sources: [README and example workflow](https://github.com/wokwi/esp8266js/tree/c7945c38012dd11a2b36190296ec64ff96ac69fa),
[peripheral construction](https://github.com/wokwi/esp8266js/blob/c7945c38012dd11a2b36190296ec64ff96ac69fa/src/esp8266.ts),
[CI firmware test](https://github.com/wokwi/esp8266js/blob/c7945c38012dd11a2b36190296ec64ff96ac69fa/.github/workflows/test-hello-world.yml).

### Espressif QEMU fork

[Espressif's QEMU fork](https://github.com/espressif/qemu) is GPL open source,
but its supported targets are **ESP32, ESP32-S3, and ESP32-C3**, not ESP8266.
It therefore cannot load this firmware as its supported target. Even for the
ESP32 targets, the official feature matrix lists Wi-Fi and I2C as unsupported;
Ethernet is supplied through an artificial OpenCores controller instead.

Source: [official target and feature matrix](https://github.com/espressif/esp-toolchain-docs/blob/main/qemu/README.md).

### Renode and SimulIDE

[Renode](https://github.com/renode/renode) is a capable open-source embedded
simulation framework, but its platform descriptions do not include an ESP8266
SoC; adding accurate ESP8266 ROM/SDK and peripheral behavior would be a major
port. [SimulIDE](https://github.com/arcachofo/SimulIDE-dev) is a graphical
circuit simulator whose supported firmware families are PIC, AVR and Arduino,
not this ESP8266 target. Neither is a drop-in option for the existing BIN.

Sources: [Renode platform CPU descriptions](https://github.com/renode/renode/tree/master/platforms/cpus),
[SimulIDE README](https://github.com/arcachofo/SimulIDE-dev/blob/master/README.md).

### Native simulation instead of binary emulation

[`afnid/espsim`](https://github.com/afnid/espsim) compiles ESP8266 application
code into a native Linux process. Its own README says it was developed for
NONOS SDK 1.2/1.3 and explicitly does not simulate off-chip devices. It does not
run this Arduino image unmodified. For new tests, a PlatformIO `native`
environment with small Arduino/hardware fakes would be more maintainable for
pure policies (glyphs, date formatting, configuration validation, message
scrolling, and brightness mapping), but would complement rather than replace an
emulator or hardware test.

Sources: [espsim README](https://github.com/afnid/espsim/blob/master/README.md),
[PlatformIO Native](https://github.com/platformio/platform-native).

## Recommendation

Use `digitake/esp8266-emulator` for fast boot, crash, GPIO and headless
regression checks. Use host-native unit tests for deterministic pure logic, and
retain a real 303WIFILC01 board for TM1650/DS1302 timing, display appearance,
portal association, NTP, analog readings, and sensor validation. Building a
faithful open-source full-clock simulation would require adding device models
to the emulator, so it is not currently a turnkey Wokwi-equivalent for the whole
board.
