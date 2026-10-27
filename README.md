# SCD40 CO₂ monitor for Apple Home

An ESP32 + Sensirion SCD40 that shows up in the iPhone **Home app** as a native
HomeKit accessory, using [HomeSpan](https://github.com/HomeSpan/HomeSpan).
No server, bridge or third-party app is needed: the ESP32 talks HomeKit
directly over Wi-Fi.

What appears in Home:

| Sensor | Value |
|---|---|
| Carbon Dioxide | level (ppm), peak level since boot, "CO₂ Detected" alert |
| Temperature | °C (Home converts to your units) |
| Humidity | % RH |
| Air Quality | Excellent … Poor, derived from CO₂ |

## Hardware

- ESP32-WROOM-32D DevKit (`esp32dev`)
- Sensirion SCD40 breakout (I²C)

| SCD40 | ESP32 |
|-------|-------|
| SDA   | GPIO21 |
| SCL   | GPIO22 |
| VDD   | 3V3 |
| GND   | GND |

On the DevKit:

- **Blue LED (GPIO2)**: HomeSpan status (blink patterns for Wi-Fi/pairing state).
- **EN** button: reset. Use this to restart the board.
- **BOOT** button (GPIO0): HomeSpan control button. **Holding it for 3 s
  enters a setup menu** that can start a Wi-Fi access point or erase pairings.
  A short tap does nothing.

## Build & flash

```sh
pio run -t upload && pio device monitor
```

Notes on `platformio.ini`:

- Uses the community [pioarduino](https://github.com/pioarduino/platform-espressif32)
  platform: HomeSpan 2.x requires Arduino-ESP32 core 3.x, which the official
  PlatformIO `espressif32` platform doesn't ship.
- Uses the `min_spiffs.csv` partition table: the firmware (~1.5 MB) doesn't fit
  the default 1.25 MB app partition. This layout still leaves room for OTA.

Re-flashing keeps Wi-Fi credentials and the HomeKit pairing (they live in NVS).

## Wi-Fi setup

Credentials are not in the code; they're entered once and stored on the device.

1. Open the serial monitor (`pio device monitor`).
2. Type `W` and press Enter. HomeSpan scans and lists nearby networks.
3. At `WiFi SSID:` type the network name (or its number from the list), Enter.
4. At `WiFi PSK:` type the password, Enter.
5. Wait for `WiFi Connected!` and an IP address.

If you can't see what you type, use `pio device monitor --echo`.

**The ESP32 only supports 2.4 GHz Wi-Fi (802.11 b/g/n).** If your network doesn't
appear in the scan:

- Routers that combine bands under one name ("Prioritize 5 GHz", band steering)
  can hide the 2.4 GHz side. Splitting the bands (e.g. `MyWiFi-24` and
  `MyWiFi-5`) fixes it.
- The 2.4 GHz band must not be Wi-Fi 6 (802.11ax) only, should use channel
  1–11, and use WPA2 (not WPA3-only).

Your iPhone can stay on the 5 GHz network: both bands are the same home network
as long as the 2.4 GHz one is **not a guest/IoT network** and **client/AP
isolation is off**.

## Pairing with Apple Home

> Before flashing, change `PAIRING_CODE` in `src/main.cpp` to your own 8 digits:
> otherwise every build of this repo shares the same code. HomeSpan rejects
> trivial codes such as `12345678` or `11111111`.

1. Home app → **+** → **Add Accessory**.
2. Tap **More options…** → **CO2 Monitor**.
3. Tap **Add Anyway** at the "uncertified accessory" warning (normal for DIY).
4. Enter the setup code **314-15-926** (`PAIRING_CODE` in `src/main.cpp`).
5. Pick a room and names, then **Continue** through to the end.

## Using it

- **Where's the CO₂?** Temperature, humidity and air quality show their values
  in the room's status row. Apple shows the CO₂ sensor only as a state
  ("Sensor" / "Detected"). **Tap it to see the ppm and peak level.**
  This is how the Home app displays CO₂ sensors, not a firmware issue.
- **Siri:** "What's the carbon dioxide level?"
- **At-a-glance widget:** in Shortcuts, create a shortcut with *Get the state of*
  → Carbon Dioxide Sensor → *Carbon Dioxide Level* → *Show Result*, and add it
  to the Home Screen.
- **Remote access, notifications and automations** (e.g. "when CO₂ is detected,
  turn on the fan") require a home hub: Apple TV or HomePod. Without one, Home
  shows "No Remote Access" and works only on your home network.

## Behavior & tuning

Constants in `src/Services.h`:

| Constant | Default | Meaning |
|---|---|---|
| `CO2_ALERT_HIGH_PPM` | 1200 | "CO₂ Detected" turns on above this |
| `CO2_ALERT_LOW_PPM` | 1000 | …and clears below this (hysteresis) |
| `UPDATE_INTERVAL_MS` | 10000 | min time between HomeKit updates of a value |

Air quality from CO₂: < 600 Excellent, < 800 Good, < 1000 Fair, < 1400 Inferior,
otherwise Poor (`AirQualitySensor::fromCo2`).

The sensor produces a reading every 5 s; each one is logged to the serial monitor.

## Calibration

### Temperature offset

The SCD40 heats itself slightly and subtracts a **temperature offset** stored
in its own EEPROM (it survives power cycles and moves with the sensor). Humidity
is derived from the compensated temperature, so a wrong offset skews both. For
example, a leftover 18 °C offset made this sensor read 9 °C / 100 % RH in a
23 °C room.

At every boot the firmware prints the stored settings:

```
SCD40: temp offset=6.4 C, altitude=0 m, ASC=on
```

and, if the offset differs from `TEMPERATURE_OFFSET_C` in `src/Scd40.cpp` by
more than 0.5 °C, rewrites and persists it (only then, as EEPROM writes are
limited).

To re-tune for your setup:

1. Place a reference thermometer next to the sensor.
2. Let both settle for ~15 min after boot. Readings start high after a
   restart and drift down for a few minutes.
3. New offset = current offset + (SCD40 reading − reference).
   E.g. 4.0 + (25.3 − 22.9) = 6.4.
4. Set `TEMPERATURE_OFFSET_C`, flash, and check the boot log for
   `temperature offset corrected … (saved)`.

### CO₂

Automatic self-calibration (ASC) is on. It assumes the sensor sees fresh air
(~420 ppm) at least once a week, so air the room regularly. Readings settle
within about a week.

## Serial commands

HomeSpan's CLI in the serial monitor (`?` lists everything):

| Command | Action |
|---|---|
| `W` | configure Wi-Fi |
| `s` | status: IP, Wi-Fi band/RSSI, paired controllers |
| `d` | dump the HomeKit accessory database |
| `H` | delete HomeKit pairings (to re-pair) |
| `E` | erase everything (Wi-Fi + pairings) |

## Troubleshooting

| Symptom | Fix |
|---|---|
| Network not in the Wi-Fi scan | Needs 2.4 GHz b/g/n; see [Wi-Fi setup](#wi-fi-setup). |
| CO2 Monitor not listed under *More options…* | Check the ESP32 has an IP (`s`); make sure the 2.4 GHz network isn't a guest network or isolated; restart with EN; as a test, put the iPhone on the 2.4 GHz network while pairing. |
| Accessory shows "No Response" | Check the serial log for Wi-Fi. If pairing was lost, remove the accessory in Home, type `H`, and pair again. |
| `i2cWrite(): ... ESP_ERR_INVALID_STATE` at boot | Harmless. The SCD40 intentionally doesn't acknowledge the wake-up command. |
| Temperature high right after boot | Normal; it settles within a few minutes. |
| Temperature/humidity wrong after settling | Re-tune the [temperature offset](#temperature-offset). |
| SCD40 errors in the log | Check wiring (SDA 21, SCL 22, 3V3, GND). |

## Project layout

| File | Purpose |
|---|---|
| `platformio.ini` | Platform, board, partitions, libraries (HomeSpan, Sensirion I2C SCD4x) |
| `src/main.cpp` | HomeSpan setup: pins, pairing code, accessory definition |
| `src/Scd40.h`, `src/Scd40.cpp` | SCD40 driver wrapper: periodic reads, cached values, boot-time settings check |
| `src/Services.h` | HomeKit services (CO₂, temperature, humidity, air quality) |

## License

MIT, see [LICENSE](LICENSE).
