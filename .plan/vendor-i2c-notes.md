# Vendor I2C notes (reported, not yet hardware-verified)

Source: owner-provided pack
`C:\Users\tioru\Downloads\sensor_liquido`
(ZCT-YLOC1 IIC handbook V00, communication diagrams, application guide V3,
STC-51 / STM32 sample code).

Do **not** copy those manuals into this repo. This file is a working
reconstruction for firmware. Mark every value as **reported** until a
controlled test on the TTGO + sensor confirms it.

## Registers (handbook)

| Address | Name | Role |
| --- | --- | --- |
| `0x00` | WL | Level `0x00` (at/below zero) … `0xFF` (at/above full) |
| `0x01` | REG1 | Handshake / channel echo |
| `0x02` | REG2 | Sample high byte |
| `0x03` | REG3 | Sample low byte |

7-bit address `0x40` (write `0x80`, read `0x81`). Sequential access after a
register pointer write.

Current firmware already does: write `0x00`, read 4 bytes. That matches WL +
REG1–3. Bytes 1–3 are **not** a documented live status bitmap during normal
reads; during calibration they are the handshake mailbox. Keep them opaque in
the measurement snapshot.

## Timing (handbook)

- ~600 ms after power-on before the chip is ready (environment capacitance).
- Capacitance sample ~every 4.8 ms; debounced level ~every 48 ms.
- After a channel command, sample complete in ~10 ms (communication note).
- Demo code polls handshake up to 20 times with ~500 µs delay.

Firmware should wait for init on boot and must not run this handshake inside
a BLE callback.

## PWM (handbook; unused)

Period 100 µs, duty `N/256` for level `N`. Leave the green wire disconnected.

## Continuous-level channel map (communication note)

| Physical step | Read sample from | Write threshold to |
| --- | --- | --- |
| Empty / dry (`TH1E`) | channel 1 | channel 1 |
| Zero-scale (`TH0Z`) | channel 2 | channel 2 |
| Full-scale (`TH0F` + `TH1F`) | channel 1 then 2 | channel 3 then 4 |

Application guide V3: zero-scale **must not** be a dry tank.

## Read-sample handshake (`0xCA`) — reported

Host MCU, device `0x40`:

1. Write REG1 (`0x01`) = `0xCA`.
2. Repeatedly read 3 bytes from current pointer (REG1..REG3) until REG1 ==
   `0xCD` (handshake OK) or timeout.
3. Write REG1 = `channel + 0x30` (channel 1 → `0x31`).
4. Poll 3-byte reads until REG1 == `channel` (not `channel+0x30`).
5. Sample = `REG2 << 8 | REG3`.

STC demo writes only the channel byte at REG1. STM32 demo writes extra dummy
bytes after the channel; prefer the **shorter STC sequence** unless hardware
fails, then compare.

## Write-threshold handshake (`0x8C`) — reported

1. Write REG1 = `0x8C`.
2. Poll until REG1 == `0xCD`.
3. Write REG2/REG3 = sample (16-bit big-endian) at register `0x02`.
4. Write REG1 = `channel + 0x30`.
5. Poll until REG1 == `channel`.

This is the path that the communication diagram calls “write the sampled
value as the new calibration value” into the sensor.

## Three-step host procedure (to implement)

1. **Empty:** vessel dry → read ch1 → set ch1.
2. **Zero:** liquid at ~3 mm from strip bottom → read ch2 → set ch2.
3. **Full:** liquid at ~2 mm below upper pad → read ch1 → set ch3; read ch2
   → set ch4.

Each BLE “commit step” runs that pair (or pair-of-pairs) and reports success
only if every handshake completes.

## Explicitly out of scope (factory programming)

STC/STM32 `dealprg` / `StartPGM` / `WriteSingleWordToMCU`:

- Toggles **VPP / 9.2 V on PRG**.
- Writes MCU program memory around `0x0DA0`.

That is factory firmware programming, not host calibration. Never implement
it on the T-Energy. PRG stays floating.

**Risk:** the demo board also calls `dealprg` after `SetThreshold`. The
application guide tells a customer MCU only to send commands and store
thresholds in the sensor EEPROM. Hardware must prove that `0x8C` alone
persists across power-off. If it does not, stop — do not add 9.2 V.

## Read-level vs calibration mailbox

After any REG1 write, the demo resets the pointer with a zero-length write
to register `0x00` so the next direct read starts at WL. After calibration,
firmware should restore normal WL reads (write `0x00` then read 4 bytes, as
today).

## What is still unknown after the vendor pack

- Handshake confirmed on **this** module at 3.3 V / 100 kHz / GPIO21-22.
- Whether REG1–3 during idle reads are leftover handshake, zeros, or useful.
- Whether `0x8C` persists without `dealprg`.
- I2C max clock (stay at 100 kHz).
- Exact timeout that is reliable (start from demo: 20 × 500 µs, plus 10 ms
  after channel select; lengthen if flaky).
