# Tank Level Sensor Protocol Draft

This is the Phase 0 contract for the future BLE peripheral and Web Bluetooth
client. UUIDs and payloads remain draft until the BLE phase freezes them.

## Device

- BLE peripheral name: `ECHH4_R_Tank`
- Security: open GATT, no pairing or bonding
- Connections: one central
- Wi-Fi: disabled and not used
- HTML: remote `web/index.html`; never served by the ESP32

## Draft GATT

Base UUID: `0000xxxx-0000-1000-8000-00805f9b34fb`

| Attribute | Draft UUID | Properties |
| --- | --- | --- |
| Service | `0000a100-0000-1000-8000-00805f9b34fb` | Primary |
| Measurement | `0000a101-0000-1000-8000-00805f9b34fb` | Read, notify |
| Control | `0000a102-0000-1000-8000-00805f9b34fb` | Write with response |
| Status | `0000a103-0000-1000-8000-00805f9b34fb` | Read, notify |
| Info | `0000a104-0000-1000-8000-00805f9b34fb` | Read |

The BLE phase must verify that the advertising name and custom UUID format are
usable by the target Web Bluetooth browser before freezing this contract.

## Measurement snapshot draft

Little-endian fields:

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | Protocol version (`1`) |
| 1 | 1 | WL level (`0x00`-`0xFF`) |
| 2 | 3 | Raw REG1-REG3 bytes, opaque during idle reads |
| 5 | 1 | Health flags |
| 6 | 1 | Calibration state |
| 7 | 2 | Sample sequence |

Health bit 0 means the latest I2C read succeeded. Bit 1 means there have been
consecutive read failures. Other bits are reserved.

## Control draft

| Opcode | Meaning |
| ---: | --- |
| `0x00` | Ping |
| `0x01` | Start calibration |
| `0x02` | Commit empty step |
| `0x03` | Commit zero-scale step |
| `0x04` | Commit full-scale step |
| `0x05` | Cancel calibration |
| `0x06` | Request immediate sample |

Calibration commits are rejected unless the state machine expects that step.
The calibration opcodes are reserved in the Phase 3 transport and currently
return unsupported because calibration is intentionally deferred while the
factory calibration is relied on. Firmware never writes sensor calibration
registers from a BLE callback; future calibration work must be queued for the
sensor layer.

## Sensor integration reference

The sensor-side I2C details are maintained in [SENSOR.md](SENSOR.md) and the
working reconstruction in [.plan/vendor-i2c-notes.md](.plan/vendor-i2c-notes.md).
