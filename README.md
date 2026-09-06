# Tank Level Sensor

ESP-IDF C firmware for a LilyGO TTGO T-Energy board that reads a Zisen/Enzhe
ZCT-YOF07-C001 (also marked ZCT-YLOC1) non-contact liquid-level sensor over I2C
and exposes the sensor through Bluetooth Low Energy (BLE) to a companion Web
Bluetooth page.

The current firmware includes an I2C connection smoke test. It probes the
reported sensor address and reads the four bytes described in [SENSOR.md](SENSOR.md)
once per second after the sensor startup delay. BLE and the web client are
still planned work.

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

The production firmware will expose calibration as explicit, stateful BLE
actions and report progress, errors, and completion. The reported `0xCA` and
`0x8C` host handshakes are isolated behind the sensor layer and must be
verified on hardware before calibration is considered working. Calibration is
cancelled if the BLE client disconnects and is protected from accidental
activation.

## BLE and Web Bluetooth direction

The ESP32 will act as the BLE peripheral and the browser will act as the
central. The future GATT contract should include:

- A stable, documented custom service UUID.
- A read/notify characteristic for the current level and sensor status.
- Write or write-with-response characteristics for supported actions such as
    starting a calibration step.
- Explicit response/error data so the web client does not need to infer state
    from timing.

The planned device name is `ECHH4 Right Tank Level`, with open GATT and no
pairing. The companion HTML client will use Web Bluetooth, display connection
and sensor state clearly, subscribe to notifications, and guide the operator
through calibration. It will live at `web/index.html` and be served remotely;
the ESP32 will not serve it. Web Bluetooth availability depends on a supported
browser and a secure context (`https://` or `localhost`); opening an arbitrary
local file may not be sufficient in every browser.

## Current repository

```text
.
├── CMakeLists.txt              ESP-IDF project definition
├── sdkconfig                   ESP-IDF 5.4 project configuration
├── main/
│   ├── CMakeLists.txt          Main component definition
│   └── main.c                  Current I2C sensor probe entry point
├── BOARD.md                    TTGO T-Energy board and pin reference
├── SENSOR.md                   Sensor specification and known gaps
├── PROTOCOL.md                 Draft BLE and sensor integration contract
├── .plan/                      Phased implementation plan
├── AGENTS.md                   Instructions for AI coding agents
└── README.md                  This project overview
```

├── CMakeLists.txt              Main component definition
├── sdkconfig                   ESP-IDF 5.4 project configuration
├── main/
│   ├── CMakeLists.txt          Main component definition
│   ├── i2c_bus.c/.h            I2C bus and device setup
│   ├── sensor.c/.h             Sampling task and latest snapshot API
│   └── main.c                  Application entry point
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the board's serial port. After flashing, monitor the serial
output. The sampling task waits at least 600 ms after initialization, then logs
the level byte and all four bytes once per second. An absent, incorrectly wired,
or unpowered sensor produces a read error while the task continues running. This
does not verify calibration commands or BLE behavior.

The sampler uses the native ESP-IDF I2C master driver at 100 kHz with GPIO21 as
SDA, GPIO22 as SCL, and internal pull-ups disabled. Provide external pull-ups
to 3.3 V when the sensor/carrier does not already include them.

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
