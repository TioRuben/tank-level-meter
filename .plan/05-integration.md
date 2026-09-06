# Phase 5 — Integration and documentation

**Goal:** The four product pieces work together, docs match the code, and
leftover hello_world artefacts are gone.
**Depends on:** Phases 1–4. Calibration remains intentionally deferred and is
clearly identified as unsupported in the web UI.

## End-to-end paths

1. Power board + sensor → I2C reads → BLE advertises.
2. Open hosted HTML on HTTPS → Connect → live level.
3. Disconnect in UI and via range/power-off → UI shows disconnected;
   firmware advertises again.
4. Calibration commands remain disabled over BLE; no EEPROM write occurs.
5. Browser disconnect/reconnect path verified during live Web Bluetooth use.
6. Live level notifications verified in Chrome at `http://localhost:5500`.
7. Sensor hardware produced changing level readings; unplug/fault behavior
  remains a future hardware test.

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

## Verified results

- ESP-IDF MCP build passes for the ESP32 target.
- nRF Connect shows the device and receives measurement/status notifications.
- Chrome Web Bluetooth connects to `H4_R_Tank` and displays live readings.
- UUID byte-order mismatch was corrected and the browser now discovers all
  four characteristics.
- The page is served from `http://localhost:5500/web/index.html`.

## Exit criteria

- README can be followed on Windows (PowerShell) to build/flash and to open
  the remote HTML page.
- Unknowns remain labelled unknown.
- Owner can operate read + (if available) calibration without serial.

Phase 5 is complete for the read-only/factory-calibrated v1. Calibration
requires a later controlled hardware verification before being reopened.
