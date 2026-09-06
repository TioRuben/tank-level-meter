# Phase 5 — Integration and documentation

**Goal:** The four product pieces work together, docs match the code, and
leftover hello_world artefacts are gone.
**Depends on:** Phases 1–4 (Phase 2 may still be stubbed if vendor commands
were never obtained — that must be obvious in README and UI).

## End-to-end paths

1. Power board + sensor → I2C reads → BLE advertises.
2. Open hosted HTML on HTTPS → Connect → live level.
3. Disconnect in UI and via range/power-off → UI shows disconnected;
   firmware advertises again.
4. Calibration happy path using `0xCA`/`0x8C` (only if handshake verified).
5. Calibration out-of-order commit rejected.
6. Browser disconnect mid-calibration → firmware cancels, UI resets wizard.
7. Sensor unplugged while BLE connected → health fault in UI, no crash.
8. Power-cycle after calibration → level mapping still valid without PRG.

## Docs to update when implementation is done

| File | Update |
| --- | --- |
| `README.md` | Current tree, no-Wi-Fi, BLE name, link to protocol, how to host HTML |
| `PROTOCOL.md` | Created in Phase 0/3; keep in sync with HTML |
| `SENSOR.md` | Verified vs still-unknown; calibration commands if found |
| `AGENTS.md` | Baseline is no longer hello_world |
| `BOARD.md` | Only if pins/power actually change (they should not) |

## Cleanup

- Remove or replace `pytest_hello_world.py`.
- Do not commit `build/`, `sdkconfig.old`, or generated binaries
  (already gitignored).
- Do not hand-edit `build/` output.

## Non-goals still in force

- Wi-Fi, ESP32 HTTP server, OTA, cloud.
- Arduino.
- 9.2 V / PRG programming path.
- Invented register maps.

## Suggested follow-ups (after v1, optional)

- GPIO2 LED: advertising / connected / calibrating / fault.
- Device Information service.
- Battery voltage if the T-Energy revision’s divider pin is identified.
- Light smoothing of level.
- Lower power: sensor sleep + BLE advertising interval (needs vendor sleep
  command, also currently unknown).
- NimBLE throughput / connection-interval tuning only if notifications stall.

## Exit criteria

- README can be followed on Windows (PowerShell) to build/flash and to open
  the remote HTML page.
- Unknowns remain labelled unknown.
- Owner can operate read + (if available) calibration without serial.
