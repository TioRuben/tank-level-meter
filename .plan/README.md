# Tank Level Sensor — Phased Plan

This folder is the implementation plan only. No firmware or web-client code
should be written until a phase is explicitly started.

## Product outcome

ESP-IDF C firmware on a LilyGO TTGO T-Energy (ESP32-WROVER-B) will:

1. Read the ZCT-YOF07-C001 / ZCT-YLOC1 sensor over I2C.
2. Drive an explicit three-step calibration workflow using the vendor I2C
   handshake (`0xCA` read-sample / `0x8C` write-threshold). Never use PRG or
   9.2 V.
3. Expose readings, health, and calibration actions over BLE GATT as
   `ECHH4_R_Tank`.
4. Be operated from a single `web/index.html` file hosted on a remote HTTPS
   (or localhost) server using Web Bluetooth.

Wi-Fi is out of scope. The ESP32 will not host the HTML page.
Locked choices: [decisions.md](decisions.md). Vendor I2C reconstruction:
[vendor-i2c-notes.md](vendor-i2c-notes.md).

## Current verified baseline

| Item | Status |
| --- | --- |
| ESP-IDF 5.4 C project, target `esp32` | Present |
| I2C master on GPIO21 (SDA) / GPIO22 (SCL) at 100 kHz | Present in `main/main.c` |
| Probe of address `0x40`, 4-byte read from register `0x00` | Present (reported spec; treat as hardware-verified only if logs confirm it) |
| BLE enabled in `sdkconfig` | NimBLE peripheral enabled; Wi-Fi unused |
| PSRAM enabled | **Not enabled** |
| Calibration commands | **Reported** in vendor pack (`0xCA`/`0x8C`); not hardware-verified |
| BLE UUID contract | Implemented in [PROTOCOL.md](../PROTOCOL.md) and firmware |
| Web client | Implemented in `web/index.html`; live Chrome test passed |
| Wi-Fi requirement | None; should remain disabled |
| GAP name | Locked: `ECHH4_R_Tank` |
| BLE pairing | Locked: open GATT, no bonding |
| Disconnect mid-calibration | Locked: cancel immediately |

The repository has moved beyond the original hello-world baseline. Firmware,
BLE, and the remote web client are implemented; calibration remains deferred.

## Phase index

| Phase | File | Goal | Can start now? |
| --- | --- | --- | --- |
| 0 | [00-foundations.md](00-foundations.md) | Architecture, BLE contract freeze, Wi-Fi off | Complete |
| 1 | [01-sensor-reading.md](01-sensor-reading.md) | Structured, periodic, health-aware sensor reads | Complete; hardware readings observed |
| 2 | [02-calibration.md](02-calibration.md) | 3-step state machine using reported `0xCA`/`0x8C` | Implemented but deferred from BLE; hardware persistence unverified |
| 3 | [03-ble.md](03-ble.md) | BLE peripheral, GATT service, no Wi-Fi | Complete; nRF Connect verified |
| 4 | [04-web-client.md](04-web-client.md) | `web/index.html` for remote HTTPS host | Complete; Chrome localhost verified |
| 5 | [05-integration.md](05-integration.md) | End-to-end checks and documentation | Complete; calibration deferred |

## Hard constraints

- Native ESP-IDF C, FreeRTOS, ESP-IDF components. No Arduino.
- SDA = GPIO21, SCL = GPIO22, sensor VCC = 3.3 V, common GND, PRG floating.
- Never generate 9.2 V for normal operation.
- Do not invent undocumented sensor registers, calibration opcodes, or status
  byte meanings. Isolate unknowns behind a small interface.
- Do not auto-start calibration on BLE connect.
- Keep I2C, sensor/calibration, BLE, and HTML protocol separable.
- HTML is one file, no framework, served from a remote HTTPS (or localhost)
  host. The ESP32 does not serve it.

## Missing information (must resolve)

See also [SENSOR.md](../SENSOR.md) section 8 and
[vendor-i2c-notes.md](vendor-i2c-notes.md).

### Remaining hardware proofs (not protocol invention)

1. **`0xCA`/`0x8C` on this board.** Vendor diagrams and STC/STM32 samples
   describe the handshake. It is still **unverified** at 3.3 V / 100 kHz on
   the T-Energy. Phase 2 implements it behind a small interface and must
   prove persistence **without** PRG/9.2 V.
2. **Idle meaning of bytes 1–3.** During calibration they are REG1–3
   (mailbox). During normal WL reads they are not documented as a status
   bitmap. UI shows raw hex until proven otherwise.
3. **Power-on delay.** Handbook: ~600 ms before the chip is ready. The
   current probe does not wait; add this in Phase 1.

### Decided in this session

See [decisions.md](decisions.md): open GATT, name `ECHH4_R_Tank`,
HTML in `web/index.html`, cancel calibration on BLE disconnect, no Wi-Fi.

### Still optional / later

- Notification period (proposal: 1 Hz; handbook level updates ~48 ms).
- GPIO2 LED for connection/calibration/fault.
- Firmware version on Info characteristic (recommended).
- Battery ADC (T-Energy divider pin **not documented** for this revision).
- I2C clock above 100 kHz.
- PWM pin (handbook: 100 µs period, duty `N/256`) — leave disconnected.
- Second device name if a left tank is added.

## Suggested improvements (recommended, not required)

1. **Implement calibration from the vendor handshake, then verify on hardware.**
   Do not copy `dealprg` / VPP. If `0x8C` does not persist across power-off,
   stop and report — that is a product blocker, not a reason to add 9.2 V.
2. **Disable Wi-Fi in `sdkconfig`** and enable only BLE. This saves RAM and
   avoids ADC2 / coexistence issues that do not apply here anyway.
3. **Prefer NimBLE** over Bluedroid for a GATT-only peripheral (smaller, enough
   for this service). Confirm during Phase 0; Bluedroid is acceptable if NimBLE
   tooling is inconvenient.
4. **Freeze a written BLE binary contract** in `PROTOCOL.md` before coding
   characteristics. Keep UUIDs identical in firmware and `web/index.html`.
   GAP name is `ECHH4_R_Tank`; it is sent in the scan response alongside the
   service UUID in the primary advertisement.
5. **Split `main.c` into components** (`i2c_bus`, `sensor`, `app_ble`) with a
   FreeRTOS sampling task and a queue/event group so BLE callbacks never block
   on I2C.
6. **Send a packed binary snapshot** (level, raw bytes, health, calibration
   state, sequence/uptime) rather than JSON over GATT.
7. **Calibration as a state machine** with `idle / step1 / step2 / step3 /
   complete / failed / cancelled`, operator confirm, and no EEPROM write except
   on an explicit step command.
8. **Onboard LED** as a cheap operator signal: disconnected / connected /
   calibrating / I2C fault.
9. **Keep PWM unused** until there is a measured need. I2C is the source of
   truth.
10. **Do not enable PSRAM unless BLE or logging actually needs it.** The
    WROVER-B has 8 MB, but current `sdkconfig` leaves SPIRAM off; turning it on
    is a separate, tested change.
11. **Replace `pytest_hello_world.py`** once the firmware no longer prints
    `Hello world!`.
12. **Host the HTML on HTTPS** (or `localhost`). `file://` often cannot use
    Web Bluetooth.

## Validation rule for every phase

State the behaviour change, then use the cheapest check:

- Firmware: `idf.py build` at minimum; serial monitor for I2C/BLE logs when
  hardware is attached.
- Web: connect, notify, disconnect, reject bad calibration, and error display
  on a supported browser over HTTPS/`localhost`.
- Never claim hardware or calibration success without a board/sensor or a
  confirmed vendor command.
