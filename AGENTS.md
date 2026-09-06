# AI Coding Instructions

## Project context

This is an ESP-IDF 5.4 C project targeting the classic ESP32 in a LilyGO TTGO
T-Energy board with an ESP32-WROVER-B module. The product goal is to read a
ZCT-YOF07-C001 / ZCT-YLOC1 non-contact liquid-level sensor over I2C and expose
readings and supported actions over BLE to a Web Bluetooth companion page.

The repository contains a modular I2C sensor-reading baseline. BLE,
calibration, and the web client are not implemented yet. Do not assume those
features already exist.

## Non-negotiable constraints

- Use native ESP-IDF C APIs, FreeRTOS, and ESP-IDF components.
- Do not introduce Arduino, Arduino libraries, or Arduino-style wrappers.
- Target the ESP32 configuration unless the user explicitly changes the board.
- Use GPIO21 for I2C SDA and GPIO22 for I2C SCL unless the hardware
  documentation is deliberately updated with the reason for a change.
- Power the sensor from the 3.3 V rail, connect common ground, and leave PRG
  disconnected during normal operation. Never add 9.2 V generation for normal
  sensor reads.
- Treat the sensor address `0x40` and the four-byte read from register `0x00`
  as reported specifications that require hardware verification.
- Do not invent undocumented sensor registers, calibration commands, status
  meanings, or BLE UUID contracts. Mark unknowns and isolate them behind a
  small interface until confirmed.

## Source of truth

- `BOARD.md` contains board pin and power information.
- `SENSOR.md` contains the current sensor specification and explicitly lists
  missing protocol details.
- `README.md` contains the project goals and current architecture direction.
- Existing source and `sdkconfig` define the current build baseline; inspect
  them before changing component dependencies or configuration.

When these documents disagree with code or measured hardware, stop and make the
conflict explicit. Do not silently choose a value and encode it as fact.

## Implementation guidance

Keep these responsibilities separable:

- I2C transport and transaction/error handling.
- Sensor decoding, health/status, and calibration state machine.
- BLE GATT transport and connection state.
- Web client protocol and presentation.

Use explicit types and named constants for protocol values. Validate lengths,
addresses, state transitions, and return codes. Prefer ESP-IDF logging and
`esp_err_t`-based error propagation over unchecked calls or ad hoc prints.
Avoid blocking the BLE event path with sensor operations; use an appropriate
FreeRTOS task or queue when work can take time.

Calibration is a safety-sensitive workflow. It must be explicit, stateful, and
observable through BLE. Do not claim that calibration works until the vendor
command/register protocol is documented or verified by controlled hardware
testing. Do not trigger a calibration write merely because a client connected.

The Web Bluetooth client should remain a simple HTML/CSS/JavaScript client
unless the user explicitly chooses a framework or build system. Keep its BLE
service and characteristic UUIDs documented and synchronized with firmware.
Remember that Web Bluetooth requires a supported browser and secure context.

## Compiling the Project

- Prefer the ESP-IDF MCP for build, flash, and monitor operations when it is
  available.
- Before using `idf.py` directly, activate the ESP-IDF Python environment so
  the ESP-IDF tools and Python dependencies resolve correctly. On Windows,
  use the ESP-IDF PowerShell environment setup or the project's configured
  ESP-IDF terminal.
- Compile from the repository root with `idf.py build` and keep the target set
  to the classic ESP32 unless the board configuration is deliberately changed.

## Working process

Before editing:

1. Read the relevant local source, `BOARD.md`, and `SENSOR.md`.
2. State the behavior being changed and identify a cheap validation check.
3. Make the smallest change that tests the hypothesis.

After editing:

1. Run the narrowest relevant build, test, or hardware check immediately.
2. For firmware changes, run at least `idf.py build` when the ESP-IDF
   environment is available.
3. For web-client changes, use the project's available browser or JavaScript
   checks and test connection, notification, disconnection, and error states.
4. Report commands that could not be run and why; do not imply hardware
   validation was performed when no sensor or board was available.

Do not reformat unrelated files, change generated build output by hand, add
license headers, commit changes, or create branches unless explicitly asked.
Preserve user changes in a dirty worktree.

## Documentation expectations

Update documentation when changing a pin, protocol, calibration behavior, BLE
contract, build prerequisite, or user workflow. Distinguish verified facts,
reported specifications, and open questions. Keep examples executable and use
PowerShell-friendly commands for the Windows development environment unless a
cross-platform command is needed.
