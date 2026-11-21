# Sensor connections

The 303WIFILC01 does not expose spare GPIOs on its programming header. Its UP and DOWN pads can nevertheless be repurposed once routine clock control has moved to the web portal or Home Assistant. The firmware's sensor override trades those two normal button functions for environmental inputs while retaining **SET** for boot-time portal access and recovery.

Disconnect power before changing wiring. The ESP8266 and BME280 must use **3.3 V logic and power**, not 5 V.

## Connection summary

| Signal | Clock connection | Purpose |
| --- | --- | --- |
| BME280 SDA | UP/S2 pad, GPIO4 | I²C data |
| BME280 SCL | P1.RX, GPIO3 | I²C clock |
| BME280 VCC | P1.3V3 | 3.3 V supply |
| BME280 GND | P1.GND | Common ground |
| LDR divider output | TOUT/A0, ESP8266 pin 6 | Analog ambient-light measurement |
| LDR divider ground | P1.GND | Common ground |

P1.RX/GPIO3 is also used for serial flashing and serial input. If a connected sensor prevents flashing, disconnect its SCL line temporarily. GPIO1/TX remains available for logs. GPIO15/DOWN is used only by the stock brightness button and is not an I²C line; the sensor override disables its normal action.

## BME280 temperature, humidity, and pressure

The firmware auto-detects a BME280 at I²C address **0x76 or 0x77** and uses a 100 kHz software-selected I²C bus on the pins above.

Wire the sensor as follows:

```text
BME280 VCC  ───────── P1.3V3 (3.3 V)
BME280 GND  ───────── P1.GND
BME280 SDA  ───────── UP/S2 pad (GPIO4)
BME280 SCL  ───────── P1.RX (GPIO3)
```

Use external I²C pull-ups to **3.3 V**, typically **4.7 kΩ** from SDA to 3.3 V and from SCL to 3.3 V. A module with suitable pull-ups already fitted may not need additional resistors; check its schematic before adding more.

The original UP button should not remain connected to SDA, because pressing it would pull the data line low. Desolder or disconnect UP/S2 before attaching the sensor. Mount the BME280 away from the ESP8266, voltage regulator, and LED display, preferably outside the case or with ventilation, to reduce self-heating and display-heating effects.

Some breakout boards include a 5 V regulator and level shifting, but compatibility depends on the specific module. This does not make the bare ESP8266 or bare BME280 5 V tolerant. If unsure, power the breakout from 3.3 V and verify its documentation.

### Enable in the portal

1. Power off and complete the wiring.
2. Power on and open the clock's web portal.
3. Enable **I have sensor(s) connected**.
4. Enable **BME280 temperature / humidity / pressure sensor**.
5. Save and restart.

The portal and Home Assistant then expose temperature (°C), humidity (%), and pressure (hPa). If the sensor is not detected, check address, SDA/SCL order, grounding, pull-ups, and power before changing the firmware.

## LDR automatic brightness

TOUT/A0 on the bare ESP8266 is **not** the 0–3.3 V A0 input found on some development boards. It accepts **0–1.0 V maximum**. Connecting a conventional LDR divider directly to 3.3 V can exceed that limit.

A conservative starting circuit used by this project is:

```text
3.3 V ─── 33 kΩ ─── LDR ───┬─── TOUT/A0 (pin 6)
                           │
                          10 kΩ
                           │
                          GND
```

With an ideal zero-ohm LDR, this divider limits TOUT to approximately 0.77 V. Real component tolerances and different LDRs change the output range, so confirm the maximum voltage before relying on the circuit and adjust resistor values as needed. Do not apply 5 V to TOUT.

TOUT is a fine-pitch ESP8266 pin rather than a convenient header connection. This modification requires careful soldering, short insulated leads, strain relief, and a final continuity/short-circuit check. Do not attempt it without appropriate tools and experience.

Mount the LDR facing ambient light, away from the LED display and direct clock illumination where practical. Otherwise the display can influence the reading and cause brightness oscillation.

### Enable and calibrate

1. Power off and complete the divider wiring.
2. Power on and open the portal.
3. Enable **I have sensor(s) connected** and **LDR on TOUT/A0 — automatic brightness**.
4. Cover the LDR or place it in the intended darkest environment. Read the raw value in the portal and use a nearby value for **LDR dark calibration**.
5. Illuminate it with the intended bright environment and use that raw value for **LDR bright calibration**.
6. Save and restart.

Dark calibration must be lower than bright calibration. The firmware maps readings within this range to levels 1–8 and constrains readings outside it. Automatic brightness takes priority over portal, button, and MQTT brightness commands; automatic changes do not repeatedly rewrite saved settings. The raw LDR value also appears in the portal and, when MQTT is enabled, in Home Assistant telemetry.

## Verification and recovery

- The portal's **Environment** card shows detected BME280 values and the raw light reading.
- MQTT publishes only sensors that are enabled and available; absent BME280 hardware produces no environmental entities.
- Button events remain visible over MQTT even though normal UP/DOWN actions are disabled.
- Disconnect sensor SCL from P1.RX if it interferes with serial flashing or boot diagnostics.
- Keep **SET** functional and USB serial access available for configuration and recovery.

For the complete pin map, circuit rationale, alternatives, and external references, see [`clock-pinouts.md`](clock-pinouts.md).
