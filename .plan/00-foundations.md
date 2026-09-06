# Phase 0 — Foundations

**Goal:** Make later phases implementable without inventing hardware behaviour.
**Code:** none in this planning pass. When this phase is executed, only
config, docs, and skeleton interfaces — no fake calibration writes.

Vendor I2C is now **reported** (not invented): see
[vendor-i2c-notes.md](vendor-i2c-notes.md). It still needs a hardware proof.

## 0.1 Confirm what is already true

From the current tree:

- I2C probe exists and is the only runtime behaviour.
- Pins and 100 kHz match [BOARD.md](../BOARD.md) / [SENSOR.md](../SENSOR.md).
- Bluetooth is compiled out.
- Wi-Fi is unused and should stay unused.

Cheap check later: serial log of a 4-byte read at `0x40`. If that already
works on hardware, treat address and read transaction as verified and update
[SENSOR.md](../SENSOR.md) accordingly.

## 0.2 Architecture to keep separate

```text
app_main
  ├── i2c_bus        open/close, tx/rx, timeouts, esp_err_t
  ├── sensor         decode, health, calibration state machine
  ├── app_ble        GAP/GATT, advertising, notify, write handlers
  └── (remote HTML)  Web Bluetooth central; not on-device
```

Rules:

- Sensor I2C never runs inside a BLE GATT callback. Use a task + queue.
- Calibration vendor bytes live in one translation unit behind
  `sensor_calibrate_step(step_id)`.
- HTML and firmware share named constants documented in a future
  `PROTOCOL.md`. Until that file exists, UUIDs are **proposed**, not fact.

## 0.3 sdkconfig direction (when implementation starts)

- Keep target `esp32`.
- Enable Bluetooth; choose **NimBLE** unless there is a reason for Bluedroid.
- Do **not** enable Wi-Fi, Wi-Fi provisioning, or HTTP server.
- Leave PSRAM off until a measured need (BLE buffers, logs) appears.
- Flash 4 MB / DIO remains as on the T-Energy unless `sdkconfig` already
  differs and is known-good.

## 0.4 Proposed BLE contract (draft — owner must accept)

Do not encode this in firmware until accepted. Values below are a starting
proposal so Phase 3/4 are not blocked by “do not invent UUIDs” with no
alternative.

| Item | Proposal |
| --- | --- |
| Device name | `ECHH4 Right Tank Level` (locked) |
| Appearance | Unknown / generic |
| Security | Open GATT, no bonding (locked). |
| Connections | 1 central |
| Service UUID | `0000a100-0000-1000-8000-00805f9b34fb` (16-bit `0xA100` in Bluetooth base) |
| Measurement characteristic | `0xA101` read + notify, binary snapshot |
| Control characteristic | `0xA102` write-with-response, command byte(s) |
| Status characteristic | `0xA103` read + notify, calibration/health state |
| Info characteristic | `0xA104` read, firmware version string or packed version |

If 16-bit custom UUIDs in the Bluetooth base are undesirable, switch all four
to random 128-bit UUIDs **once** and freeze them in `PROTOCOL.md`.

### Draft measurement snapshot (little-endian)

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | Protocol version (`1`) |
| 1 | 1 | Level `0x00`–`0xFF` |
| 2 | 3 | Raw bytes 1–3 (opaque until decoded) |
| 5 | 1 | Health flags |
| 6 | 1 | Calibration state |
| 7 | 2 | Sequence / sample counter |

Health flags (proposal): bit0 I2C ok, bit1 consecutive-fail, others reserved.

### Draft control commands

| Opcode | Meaning |
| ---: | --- |
| `0x00` | No-op / ping |
| `0x01` | Start calibration session |
| `0x02` | Commit empty step (`TH1E`) |
| `0x03` | Commit zero step (`TH0Z`) |
| `0x04` | Commit full step (`TH0F`/`TH1F`) |
| `0x05` | Cancel calibration |
| `0x06` | Request immediate sample |

Firmware must reject unknown opcodes and out-of-order calibration commits.

## 0.5 Calibration protocol (no longer a documentation blocker)

The owner pack supplies register list + `0xCA`/`0x8C` handshake. Phase 2
may implement that sequence as **reported**.

Still required before claiming calibration works:

1. Handshake ACK on this sensor (`REG1 == 0xCD`, then channel echo).
2. Thresholds survive power-cycle **without** PRG/9.2 V.
3. After handshake, restore WL pointer (`0x00`) so normal reads still work.

If (2) fails, do not implement factory programming. Update SENSOR.md with
measured results. BLE opcodes stay stable even if I2C details are tuned.

## 0.6 Documentation to add when this phase is executed

- `PROTOCOL.md` — BLE UUIDs, binary layouts, opcodes, calibration states.
- Update `README.md` current-repository tree and “no Wi-Fi” statement.
- Update `AGENTS.md` baseline (I2C probe, not hello_world).
- Keep [SENSOR.md](../SENSOR.md) unknowns explicit.

## 0.7 Exit criteria

- Decisions in [decisions.md](decisions.md) remain the default.
- BLE UUID/layout draft accepted or revised in writing (`PROTOCOL.md` when
  coding starts).
- Calibration uses reported `0xCA`/`0x8C`; PRG path forbidden.
- Wi-Fi stays off; HTML is `web/index.html` hosted remotely.
