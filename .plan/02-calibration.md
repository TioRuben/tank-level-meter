# Phase 2 — Sensor calibration

**Goal:** Expose the vendor 3-point calibration as an explicit, observable
state machine using the reported `0xCA`/`0x8C` handshake.
**Depends on:** Phase 1 snapshot/health API and [vendor-i2c-notes.md](vendor-i2c-notes.md).
**Validation:** `idf.py build`; serial logs of handshake; with hardware, each
commit only after the matching liquid level is set; power-cycle persistence
check **without** PRG.

## Vendor procedure (from SENSOR.md — process only)

1. Empty and dry container → store `TH1E`.
2. Liquid at zero-scale mark (~3 mm from strip bottom) → store `TH0Z`.
   Completely dry is **not** valid for this step.
3. Liquid at full-scale mark → store `TH0F` and `TH1F`.

Demo board Key 1 ×1/×2/×3 is the **operator** sequence. The host MCU
equivalent is the I2C handshake in [vendor-i2c-notes.md](vendor-i2c-notes.md),
not GPIO and not PRG.

## If handshake fails on hardware

Keep the state machine, return a distinct error (`ESP_ERR_INVALID_RESPONSE`
or similar), and **do not** fall back to `dealprg` / 9.2 V. HTML shows the
failure. Reverse-engineering with a logic analyzer is the next step, not
invented registers.

## State machine (firmware)

```text
idle
  -- start --> wait_empty
  -- commit_empty --> wait_zero      (I2C write empty cmd)
  -- commit_zero  --> wait_full      (I2C write zero cmd)
  -- commit_full  --> complete       (I2C write full cmd)
any wait_* -- cancel --> idle
any commit fail --> failed
BLE disconnect --> idle (cancelled)     // locked decision
```

Rules:

- Start is explicit (BLE/HTML), never implied by connect.
- Commit of step N is rejected unless current state is the matching wait.
- Failed/cancelled require a new `start`.
- **BLE disconnect cancels immediately** (locked).
- Optional extra timeout still useful if the browser stays connected but the
  operator walks away (proposal 10 minutes per step).
- Log each transition with reason.
- After each handshake (success or fail), restore register pointer to WL
  (`0x00`) so measurement reads keep working.

## Safety

- Calibration changes persistent sensor EEPROM. Treat as safety-sensitive.
- HTML should require a confirm checkbox/button per step (“vessel is empty
  and dry”, etc.).
- Firmware still validates order even if the UI is bypassed (nRF Connect).
- Rate-limit commits (ignore repeats within a short window).

## Mapping to BLE (once Phase 3 exists)

- Control opcodes `0x01`–`0x05` from Phase 0 draft.
- Status characteristic carries current state + last error.
- Measurement notifications continue during calibration so the operator can
  see live level while filling.

## Hardware test plan (when opcodes exist)

1. Dry empty → commit empty → `0xCA` ch1, `0x8C` ch1 → log ACK.
2. Fill to zero mark → commit zero → `0xCA` ch2, `0x8C` ch2 → log ACK.
3. Fill to full mark → commit full → read ch1/set ch3, read ch2/set ch4.
4. Sweep liquid and confirm WL tracks `0x00`→`0xFF`.
5. Power-cycle **without touching PRG**; confirm calibration persisted in
   the sensor. If it did not, stop and report.

ESP32 NVS copy of thresholds is **not** required. Never call factory
`dealprg`.

## Exit criteria

- Handshake implemented from vendor notes and either verified on hardware
  or reported as failed without inventing a second protocol.
- Illegal transitions rejected.
- No write on connect.
- Serial (and later BLE) show state, not just “done”.
