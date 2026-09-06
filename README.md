# Tank Level Sensor

ESP-IDF C firmware for a LilyGO TTGO T-Energy board that reads a Zisen/Enzhe
ZCT-YOF07-C001 (also marked ZCT-YLOC1) non-contact liquid-level sensor over I2C
and exposes the sensor through Bluetooth Low Energy (BLE) to a companion Web
Bluetooth page.

The current firmware includes I2C sampling and BLE GATT communications. It probes the
reported sensor address and reads the four bytes described in [SENSOR.md](SENSOR.md)
once per second after the sensor startup delay, then publishes readings to the
remote Web Bluetooth client in `web/index.html`.

This is just a test with a board I had in hand. The definitive one will use an smaller ESP32 RISC V module and will communicate with CAN Bus.

## Project goals

- Use native ESP-IDF C APIs and FreeRTOS. Arduino APIs and Arduino libraries
    are out of scope.
- Read the sensor's continuous 8-bit level value over I2C.
- Make readings and supported sensor actions available through a documented
    BLE GATT service.
- Provide a standalone HTML companion client using the browser Web Bluetooth
    API.
- Support the sensor's three-step calibration workflow through the reported
    vendor I2C handshake, after hardware verification.
- Operate without Wi-Fi. The ESP32 is a BLE peripheral; the HTML page is
    hosted remotely.

## Hardware

The target board is the LilyGO TTGO T-Energy with an ESP32-WROVER-B module.
See [BOARD.md](BOARD.md) for the full board description and pin constraints.

The planned normal-operation wiring is:

| Sensor wire | Sensor pin | Board | Notes |
| --- | --- | --- | --- |
| Red | VCC | 3V3 | The sensor accepts 2.8 V to 5.5 V; use the board's 3.3 V rail for logic compatibility. |
| Black | GND | GND | Common ground is required. |
| Yellow | SDA | GPIO21 | ESP32 I2C data line. |
| Orange | SCL | GPIO22 | ESP32 I2C clock line. |
| White | PRG | Unconnected | The 9.2 V programming input is not part of normal operation. |
| Green | PWM | Unconnected initially | Optional analog output; its characteristics are not yet confirmed. |

Use suitable I2C pull-ups to 3.3 V if they are not already present on the
sensor or carrier board. Do not connect or disconnect the sensor while powered.

## Sensor protocol

The information currently available for the sensor indicates:

- Default 7-bit I2C address: `0x40`.
- The level is an unsigned byte from `0x00` (empty/below zero mark) to `0xFF`
    (full/above full mark).
- A direct read is described as four consecutive bytes starting at register
    `0x00`; byte 0 is the level and bytes 1-3 are diagnostic or threshold data.
- The sensor has a 120 mm effective range and claims approximately +/-3 mm
    accuracy after calibration.

The vendor protocol pack reports that `0x00` is WL and `0x01`-`0x03` are
REG1-REG3. A host read-sample handshake writes `0xCA` to REG1, waits for
REG1=`0xCD`, writes `channel + 0x30`, then waits for the channel echo.
REG2:REG3 is the 16-bit sample. A host threshold-write handshake uses `0x8C`
and writes the sample to REG2:REG3 before selecting the channel. Continuous
calibration uses channels 1/1 for empty, 2/2 for zero-scale, and 1/3 plus
2/4 for full-scale.

These commands are reported by vendor manuals and sample code, but are not
yet verified on this board at 3.3 V and 100 kHz. Factory programming code
using PRG and 9.2 V is out of scope; PRG remains unconnected. REG1-REG3 are
opaque during normal measurement reads until their idle meanings are confirmed.
See [SENSOR.md](SENSOR.md) and [PROTOCOL.md](PROTOCOL.md).

## Calibration

The sensor documentation describes a three-stage calibration process:

1. Empty and dry container: store the empty threshold (`TH1E`).
2. Liquid at the zero-scale mark: store the zero threshold (`TH0Z`). This is
     not the same as a completely dry container.
3. Liquid at the full-scale mark: store the full thresholds (`TH0F` and
     `TH1F`).

The firmware contains the reported calibration state machine in the sensor
layer, but calibration is currently deferred because the sensor appears to
arrive factory-calibrated. BLE calibration commands return unsupported and do
not write the sensor. The future calibration work must still verify the
handshake and threshold persistence without PRG.

## BLE and Web Bluetooth direction

The ESP32 will act as the BLE peripheral and the browser will act as the
central. The future GATT contract should include:

- A stable, documented custom service UUID.
- A read/notify characteristic for the current level and sensor status.
- Write or write-with-response characteristics for supported actions such as
    starting a calibration step.
- Explicit response/error data so the web client does not need to infer state
    from timing.

The planned device name is `ECHH4_R_Tank`, with open GATT and no
pairing. The companion HTML client will use Web Bluetooth, display connection
and sensor state clearly, subscribe to notifications, and guide the operator
through calibration. It will live at `web/index.html` and be served remotely;
the ESP32 will not serve it. Web Bluetooth availability depends on a supported
browser and a secure context (`https://` or `localhost`); opening an arbitrary
local file may not be sufficient in every browser.

## Current repository

```text
.
├── main/                       ESP-IDF firmware
│   ├── i2c_bus.c/.h            I2C bus and device setup
│   ├── sensor.c/.h             Sampling and deferred calibration API
│   ├── ble.c/.h                NimBLE GATT peripheral
│   └── main.c                  Application entry point
├── web/index.html              Remote Web Bluetooth client
├── PROTOCOL.md                 Implemented BLE contract
├── BOARD.md                    TTGO T-Energy reference
├── SENSOR.md                   Sensor protocol and open hardware questions
├── .plan/                      Phased implementation plan
└── AGENTS.md                   Repository instructions
```

## Build, flash, and monitor

Use the ESP-IDF 5.4 environment with the target set to the classic ESP32:

```powershell
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the board's serial port. The sampling task waits at least
600 ms after initialization, then logs the level byte and all four bytes once
per second. An absent, incorrectly wired, or unpowered sensor produces a read
error while the task continues running. The firmware advertises as `H4_R_Tank`
and publishes level/status notifications over BLE.

The sampler uses the native ESP-IDF I2C master driver at 100 kHz with GPIO21 as
SDA, GPIO22 as SCL, and internal pull-ups disabled. Provide external pull-ups
to 3.3 V when the sensor/carrier does not already include them.

## Web client

The ESP32 does not host the page. Serve the repository from a static HTTPS host
or localhost, then open:

```text
http://localhost:5500/web/index.html
```

The page requires a Web Bluetooth-capable Chromium browser. It connects to
`H4_R_Tank`, subscribes to measurement/status notifications, and displays raw
sensor health. Calibration writes remain disabled while factory calibration is
used.

The repository includes a GitHub Pages deployment workflow. After enabling
Pages with **GitHub Actions** as the source in the repository settings, pushes
to `master` publish the page at:

<https://tioruben.github.io/tank-level-meter/>

## Engineering constraints

- Keep the firmware in C using ESP-IDF components and APIs.
- Treat [BOARD.md](BOARD.md) and [SENSOR.md](SENSOR.md) as hardware references,
    while marking uncertain vendor behavior as unverified.
- Keep I2C access, sensor state/calibration, BLE transport, and presentation
    protocol separable so each can be tested independently.
- Do not expose the sensor's programming pin or require 9.2 V during normal
    operation.
- Do not commit generated build output, credentials, or device-specific
    pairing data.

## References

- [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/)
- [Board specification](BOARD.md)
- [Sensor specification](SENSOR.md)
