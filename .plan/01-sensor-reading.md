# Phase 1 — Sensor reading

**Goal:** Replace the inline probe in `main/main.c` with a reusable I2C
transport and a sampling path that later BLE code can consume.
**Depends on:** Phase 0 architecture only. Does **not** need calibration
opcodes or BLE.
**Validation:** `idf.py build`; with hardware, serial log of level + 4 bytes
and a clear error on NACK.

## In scope

- I2C bus init on GPIO21/22, 100 kHz, internal pull-ups **disabled**
  (external 4.7 kΩ to 3.3 V as documented).
- Device address `0x40`, register `0x00`, 4-byte `transmit_receive`.
- **Wait ≥600 ms after power/init** before the first WL read (handbook).
- Named constants; `esp_err_t` propagation; `ESP_LOG*`.
- Decode:
  - `level` = byte 0 / WL (unsigned 0–255).
  - `raw[1..3]` = REG1–3, stored, not interpreted during idle reads.
- Health:
  - last `esp_err_t`
  - consecutive failure count
  - “never successfully read” vs “lost after success”
- FreeRTOS task: sample once per second (same as current probe) unless a
  later BLE interval overrides it.
- Thread-safe latest-sample snapshot for BLE/UI (mutex or copy-on-read).

## Out of scope

- Calibration writes.
- BLE.
- PWM pin.
- Filtering, unless noise on hardware forces a later follow-up.
- Inventing millimetre conversion as a firmware truth. If the UI wants mm,
  compute `level / 255 * 120` as a **display estimate** only, documented as
  approximate.

## Behaviour change vs today

Today `app_main` loops forever and logs. After Phase 1, `app_main` starts the
bus + sampling task and still logs, but other modules can call
`sensor_get_latest()`.

## Suggested file split (when coding)

```text
main/
  main.c
  i2c_bus.c / i2c_bus.h
  sensor.c / sensor.h
```

Keep it inside `main/` until a second component is justified. Do not add
Arduino wrappers.

## Error handling

- Init failure: log and do not spin a silent success path.
- Read timeout/NACK: increment fail count, keep last good sample marked stale.
- Do not crash the task; BLE will need a live task even when the sensor is
  unplugged.

## Exit criteria

- Build succeeds.
- Latest snapshot API exists.
- Bytes 1–3 remain opaque in code comments until mapped.
- [SENSOR.md](../SENSOR.md) updated if hardware verifies or contradicts the
  4-byte read.
