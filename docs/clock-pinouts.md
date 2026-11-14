# 303WIFILC01 pinouts and sensor expansion

## Implemented sensor configuration

The firmware's optional **“I have sensor(s) connected”** override supports this combination:

- **Analog brightness:** LDR divider on TOUT/A0, using the bounded circuit below and UI calibration.
- **Digital THP:** BME280 with **SDA on GPIO4/UP** and **SCL on GPIO3/P1.RX**, powered at 3.3 V with external I²C pull-ups. UP is desoldered; DOWN is disabled because brightness is automatic.

GPIO3/RX is preferable to GPIO15/DOWN for the second I²C line: the schematic shows a 1 kΩ pull-down on GPIO15, which fights normal I²C pull-ups. TX remains free for logs. Disconnect SCL while serial flashing if the attached bus interferes with the adapter. The display has a separate bit-banged driver, so the BME280 does not share the TM1650 bus.

## Conclusion

The green 303WIFILC01 has **no dedicated unused-GPIO header**, but it does have usable expansion options:

1. **Easiest connection without removing anything:** use **GPIO3 / U0RXD on P1.RX** for a one-wire sensor such as a DS18B20. P1 also provides 3.3 V and GND. Keep GPIO1/TX for logs, and disconnect the sensor while flashing if it interferes with the serial adapter.
2. **Electrically cleanest option:** repurpose **GPIO4 on the UP/S2 button pads** and remove or stop using that button. GPIO4 does not share the RTC, display, UART, or boot-strapping role used elsewhere on this board.
3. **Best for automatic brightness if fine-pitch soldering is acceptable:** attach an LDR divider directly to **TOUT/A0 (ESP8266EX pin 6)**. A [UA ID blog build](https://blog.uaid.net.ua/esphome-clock/) demonstrates this approach on the same board. It consumes no GPIO, but the analog input must remain within 0–1.0 V.
4. **Possible but needs a bench test:** add a 3.3 V I²C sensor to the existing **GPIO12/GPIO13 TM1650 bus**, accessible around R5/R6 or the TM1650. The display driver is documented as “not real I2C,” so coexistence is not guaranteed by the board documentation.

Do not confuse this board with the **XY-Clock** in the Tasmota discussion. That discussion covers three different boards; the XY-Clock's GPIO9/GPIO10 buttons, buzzer, and KEY connector are not present on the 303WIFILC01. On this board, GPIO6–GPIO11 are connected to the SPI flash chip. [Tasmota discussion](https://github.com/arendst/Tasmota/discussions/15788) · [303 schematic](https://github.com/maarten-pennings/303WIFILC01/blob/c4656664e2b52af7930a15a1f13bd332330f5f56/1-pcbnets/Schematic_303WIFILC01.png)

## P1 programming header

The reverse-engineered board documentation identifies P1 as follows. The labels are below their holes, which makes the header easy to misread. [PCB analysis](https://github.com/maarten-pennings/303WIFILC01/blob/c4656664e2b52af7930a15a1f13bd332330f5f56/1-pcbnets/readme.md) · [flashing wiring](https://github.com/maarten-pennings/303WIFILC01/blob/c4656664e2b52af7930a15a1f13bd332330f5f56/2-fwbackup/readme.md)

| P1 label | Net / MCU function | Sensor usefulness |
| --- | --- | --- |
| RST | Shown unconnected in the schematic | None; the original analysis says reset is missing |
| TX | GPIO1 / U0TXD | Reassignable, but normally retain for serial logs |
| RX | GPIO3 / U0RXD | **Best header-mounted single-wire sensor signal** |
| IO0 | GPIO0, SET button, bootloader select | Avoid for sensors |
| 3V3 | Regulated 3.3 V rail | Sensor supply, within an unverified current/thermal budget |
| GND | Ground | Sensor ground |

GPIO1 and GPIO3 are confirmed as UART TX/RX in Espressif's pin table. [ESP8266EX datasheet, Table 2-1](https://www.espressif.com/sites/default/files/documentation/0a-esp8266ex_datasheet_en.pdf)

**Board-revision caution:** at least one community photo annotates P1 differently. Trust the schematic and the original reverse-engineering documentation, and verify 3.3 V/GND with a multimeter before attaching a sensor.

## Existing GPIO allocation

| GPIO | Existing 303WIFILC01 connection | Availability |
| --- | --- | --- |
| 0 | SET/S1 and P1.IO0; must select the bootloader during flashing | Poor sensor choice |
| 1 | P1.TX / UART0 TX | Available if serial TX is sacrificed |
| 2 | Status LED D1; also a boot-related pin | Not recommended |
| 3 | P1.RX / UART0 RX | **Available with UART/flashing caveats** |
| 4 | UP/S2 button | **Good if UP is repurposed** |
| 5 | DS1302 CE | Used |
| 6–11 | P25Q80H SPI flash | Do not use |
| 12 | TM1650 SCL | Shared-I²C experiment only |
| 13 | TM1650 SDA | Shared-I²C experiment only |
| 14 | DS1302 I/O | Used |
| 15 | DOWN/S3 and 1 kΩ pull-down; boot-strapping pin | Poor sensor choice |
| 16 | DS1302 CLK | Used |
| TOUT/ADC | ESP8266EX pin 6, no connector in the board schematic | Usable by careful direct soldering; 0–1.0 V input |

Sources: [GPIO and RTC mapping](https://github.com/maarten-pennings/303WIFILC01/blob/c4656664e2b52af7930a15a1f13bd332330f5f56/1-pcbnets/readme.md) · [board schematic](https://github.com/maarten-pennings/303WIFILC01/blob/c4656664e2b52af7930a15a1f13bd332330f5f56/1-pcbnets/Schematic_303WIFILC01.png) · [ESP8266EX ADC specification](https://www.espressif.com/sites/default/files/documentation/0a-esp8266ex_datasheet_en.pdf)

## Recommended sensor implementations

### Option A: DS18B20 on P1.RX / GPIO3

Suggested wiring:

```text
P1.3V3  ───────────── DS18B20 VDD
P1.GND  ───────────── DS18B20 GND
P1.RX   ───────────── DS18B20 DATA (GPIO3)
P1.3V3  ─── 4.7 kΩ ── DS18B20 DATA
```

Use externally powered mode, not parasite power. This retains all buttons, the display, the RTC, and TX serial logging. The firmware would need a GPIO3 OneWire adapter, and the sensor should be disconnected while flashing if it loads or fights the USB-serial adapter.

### Option B: DS18B20 or a digital sensor on GPIO4/S2

If the UP button is not needed, its GPIO4 pad offers a cleaner general-purpose input than RX. This avoids UART contention and boot-time serial traffic. It is the preferred option when sacrificing UP is acceptable. A one-wire temperature sensor can use the same circuit as Option A, with DATA connected to the GPIO4 side of S2.

### Option C: BH1750 or BME280 on the display bus

The board has 1 kΩ pull-ups around R5/R6 on the TM1650 SCL/SDA lines. A BH1750 at 0x23/0x5C or a BME280 at 0x76/0x77 would not overlap the TM1650 addresses used by this firmware (0x24 and 0x34–0x37). Nevertheless, address compatibility alone does not prove bus compatibility: the original board analysis explicitly describes the TM1650 interface as not real I²C. Test display updates and sensor reads together before relying on this arrangement.

A BH1750 is attractive for automatic brightness. For temperature/humidity, place the sensor away from the ESP8266, regulator, and display—ideally outside the case on short leads—to reduce self-heating bias.

### Option D: LDR directly on TOUT/A0 for automatic brightness

The [UA ID ESPHome clock article](https://blog.uaid.net.ua/esphome-clock/) shows an LDR and fixed resistor wired to pin 6 of the ESP8266EX on a 303WIFILC01. This validates the physical approach and avoids sacrificing RX, UP, or the display bus. It is a fine-pitch QFN solder joint, however, so it is less accessible than P1.

The exact divider needs care: **TOUT is a 0–1.0 V input on the bare ESP8266EX**, not the 0–3.3 V A0 input found on some ESP8266 development boards with external dividers. A single LDR/resistor divider across 3.3 V can approach 3.3 V at one light extreme and should not be copied without verifying its range.

One conservative starting circuit that bounds the output below 1 V even when the LDR approaches zero resistance is:

```text
3.3 V ─── 33 kΩ ─── LDR ───┬─── TOUT (U2 pin 6)
                           │
                          10 kΩ
                           │
                          GND
```

At the theoretical worst case of a zero-ohm LDR, TOUT is approximately 0.77 V. Bright light produces a higher reading and darkness a lower reading. The resistor values should be tuned for the actual LDR and desired light range. Verify the maximum TOUT voltage with a multimeter before powering the ESP8266.

Mount the LDR facing ambient light, not the LED display. Firmware should also use filtering, hysteresis, and a slow adjustment rate to avoid brightness oscillation from the display's own light.

### Separate I²C on GPIO1/GPIO3

Software I²C could instead use P1.RX as SDA and P1.TX as SCL. This avoids sharing with the TM1650 but consumes both serial pins, exposes the sensor to UART/boot activity, and makes flashing/debugging less convenient. It is therefore below Options A and B.

## Electrical cautions

- Power digital sensors from **3.3 V**, or provide suitable level shifting. The 5 V rail on P3 must not be connected directly to an ESP8266 GPIO.
- The available spare current and AMS1117 thermal headroom have not been established by these sources. Prefer low-power sensors and verify regulator temperature under full display brightness and Wi-Fi load.
- The CR1220 holder powers the RTC only; it is not a sensor supply.
- Use short sensor leads and strain relief. P1 was designed for programming, not repeated mechanical load.
- Verify the exact board revision and P1 power pins before soldering.

## Sources

- [Tasmota discussion #15788 — board variants and 303WIFILC01 identification](https://github.com/arendst/Tasmota/discussions/15788)
- [Tasmota comment confirming P1 flashing with IO0 grounded](https://github.com/arendst/Tasmota/discussions/15788#discussioncomment-4487508)
- [Maarten Pennings: PCB nets, power domains, GPIO mapping, and schematic](https://github.com/maarten-pennings/303WIFILC01/tree/c4656664e2b52af7930a15a1f13bd332330f5f56/1-pcbnets)
- [Maarten Pennings: P1 flashing and serial wiring](https://github.com/maarten-pennings/303WIFILC01/blob/c4656664e2b52af7930a15a1f13bd332330f5f56/2-fwbackup/readme.md)
- [Espressif: ESP8266EX datasheet](https://www.espressif.com/sites/default/files/documentation/0a-esp8266ex_datasheet_en.pdf)
- [UA ID: ESPHome clock with an LDR on TOUT/pin 6](https://blog.uaid.net.ua/esphome-clock/)
