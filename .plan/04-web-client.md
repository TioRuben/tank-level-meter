# Phase 4 — Remote Web Bluetooth client

**Goal:** One HTML file (inline CSS + JS) that connects to the ESP32, shows
level/health, and walks calibration.
**Depends on:** Frozen `PROTOCOL.md` / Phase 3 UUIDs. The first client now
targets the working read-only path; calibration controls remain deferred.
**Not on the ESP32.** Served from a remote HTTPS server or `localhost`.

## Constraints

- Single file, no framework, no build step.
- Web Bluetooth (`navigator.bluetooth`).
- Supported browsers only (Chromium desktop / Android Chrome; not Safari or
  Firefox as of this plan).
- Secure context required. Document that `file://` is unreliable.

Repo path when implemented: `web/index.html` (copied to the remote host
as-is). Owner confirmed this file lives in the repository.

## UI (small, operator-focused)

1. **Connection**
   - Connect / Disconnect
   - Browser support warning if Web Bluetooth is missing
   - Device name + connection state
2. **Reading**
   - Level as 0–255 and percent (`level/255`)
   - Optional estimated mm (`* 120`), labelled approximate
   - Raw bytes 1–3 in hex
   - Health (I2C ok / fail count / stale)
   - Last update age (detect silent link death)
3. **Calibration status**
  - Show current firmware calibration state
  - Explain that calibration is deferred while factory calibration is used
  - Keep calibration writes disabled until the vendor handshake is verified
4. **Log**
   - Short on-page event log (connect, notify, errors, opcodes)

No charts required for v1.

## Client logic

- Request device with `filters` on the service UUID and/or
  `namePrefix: 'H4_R_'` (full name `ECHH4_R_Tank`) **and**
  `optionalServices` as needed.
- After connect: get service, start measurement + status notifications,
  read info once.
- Writes use `writeValueWithResponse`.
- Handle disconnect (`gattserverdisconnected`) and reset UI.
- Do not busy-poll; use notifications.
- Keep UUID constants in one JS object at the top of the file, copied from
  `PROTOCOL.md`.

## Errors to surface

- User cancelled chooser
- GATT connect failed
- Characteristic missing (firmware/HTML UUID mismatch)
- Write rejected
- Notifications stopped
- Sensor health fault while connected

## Hosting

- ESP32 does not serve this file.
- Remote static host (any HTTPS static server) is enough.
- No backend API; the browser talks only to the ESP32 over BLE.

## Validation

- Manual: connect, see 1 Hz updates, disconnect, reconnect.
- Calibration buttons disabled unless state allows that step.
- Unsupported-calibration path displays the firmware error.
- Check on a phone **and** desktop Chrome if possible (Web Bluetooth
  availability differs).

## Exit criteria

- One HTML file in repo, UUIDs synchronized.
- README explains HTTPS/`localhost` and supported browsers.
- No Wi-Fi/HTTP code added to firmware to support the page.

## Current implementation

`web/index.html` now implements connect/disconnect, measurement and status
notifications, firmware-info reading, raw REG1-3 display, health/staleness
display, and harmless ping/sample controls. It uses the `H4_R_Tank` device
name prefix and the UUIDs in [PROTOCOL.md](../PROTOCOL.md).
