# Phase 3 — BLE communications

**Goal:** ESP32 as BLE peripheral; browser as central. No Wi-Fi, no HTTP
server, no on-device page.
**Depends on:** Phase 0 accepted GATT contract, Phase 1 sampling task.
Calibration is intentionally deferred from BLE for now because the sensor
appears factory-calibrated; its reserved control opcodes return unsupported.
**Validation:** `idf.py build` with BT enabled; nRF Connect or Chrome
`about:bluetooth-internals` sees advertise → connect → read/notify; I2C
still works while connected.

## Stack and config

- Enable Bluetooth in `sdkconfig`; keep Wi-Fi disabled.
- Prefer NimBLE (GATT server only).
- Single connection.
- Advertising: the 128-bit service UUID is in the primary packet; the complete
  GAP name `ECHH4_R_Tank` is in the scan response because both do not fit in
  the primary 31-byte advertisement.
- UUID of the custom service in advertise or scan response.
- No BLE mesh, no Classic Bluetooth.

## GATT (from accepted PROTOCOL.md)

Minimum characteristics:

1. **Measurement** — read + notify. Packed snapshot from Phase 0.
2. **Control** — write-with-response. Opcodes for sample / calibration.
3. **Status** — read + notify. Calibration state + last error code.
4. **Info** — read. Firmware version.

Optional standard services later: Device Information (`0x180A`). Not
required for v1.

## Concurrency

- GATT write handler: parse, validate, enqueue, return.
- Sensor task: perform I2C, update snapshot, notify if a client is
  subscribed.
- Never call I2C from the NimBLE host task.
- On disconnect: stop notifications and **cancel any in-progress
  calibration immediately** (locked). Return the sensor to normal WL reads.

## Connection behaviour

- Start advertising after sensor init attempt (advertise even if I2C failed,
  so the UI can show “sensor fault”).
- Restart advertising on disconnect.
- Reject a second central.

## Data policy

- Notify measurement at 1 Hz by default (matches current probe).
- Do not spam; if the value is unchanged, still notify at 1 Hz so the UI
  watchdog knows the link is alive — **or** notify on change plus a 2 s
  heartbeat. Prefer 1 Hz for simplicity.
- Control responses: write-with-response status plus status characteristic
  update. The web client must not infer success from timing.
- Calibration control opcodes remain reserved and return unsupported until the
  owner explicitly reopens calibration work.

## Out of scope

- Wi-Fi, BLE provisioning, phone native apps.
- Pairing/bonding unless the owner requires it.
- High-throughput or multiple services.

## Exit criteria

- BT enabled, Wi-Fi still unused.
- UUIDs match the written protocol.
- Connect / notify / write / disconnect / reconnect verified with a generic
  BLE client even before the HTML app exists.
- Calibration remains untouched by BLE and the existing factory-calibrated
  level readings continue during the whole test.
- Sensor task remains responsive during BLE events.
