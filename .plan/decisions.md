# Decisions (this planning session)

Locked unless the owner changes them.

| Topic | Decision |
| --- | --- |
| Transport | BLE only. No Wi-Fi, no HTTP on the ESP32. |
| HTML hosting | Remote HTTPS/`localhost` server. Firmware does not serve the page. |
| HTML in repo | Yes: `web/index.html` (copy that file to the host). |
| BLE security | Open GATT, no pairing/bonding. |
| Advertising / GAP name | `ECHH4 Right Tank Level` |
| BLE connections | 1 central (assumed). |
| Calibration on BLE disconnect | **Cancel the session immediately.** |
| Calibration I2C | Use the vendor host-MCU handshake reconstructed in [vendor-i2c-notes.md](vendor-i2c-notes.md). Do **not** use PRG / 9.2 V / `dealprg`. |
| Persistence check | After implementing 0xCA/0x8C, power-cycle and confirm thresholds survive. If they do not, stop and report; do not add 9.2 V. |

## Still open (non-blocking)

- Unique names if a left-hand tank is added later (`ECHH4 Left Tank Level` or similar).
- Firmware version string on the Info characteristic (recommended: yes).
- Onboard GPIO2 LED as connection/calibration indicator (recommended).
- NimBLE vs Bluedroid (recommended: NimBLE).
- Whether 16-bit custom UUIDs in the Bluetooth base (`0xA100`…) are acceptable, or random 128-bit UUIDs are preferred.
