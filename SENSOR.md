# Sensor Specification Sheet: ZCT-YOF07-C001 / ZCT-YLOC1 Non-Contact Flexible Continuous Liquid Level Sensor

## 1. Overview & General Description
The **ZCT-YOF07-C001** (also referred to as **ZCT-YLOC1** on the PCB/FPC artwork) is a non-contact, capacitive-based continuous liquid level sensor manufactured by **Shenzhen Zisen Innovation Electronics Co., Ltd. / Enzhe Electronics**. 

Constructed on a flexible printed circuit (FPC) substrate with a 3M adhesive backing (0.4 mm thickness), it attaches directly to the outer surface of non-metallic containers (glass, plastic, acrylic, ceramic, etc.). It continuously senses fluid height inside the vessel without direct contact, delivering liquid level readings digitally over an **I2C interface** or via an analog **PWM voltage output**.

---

## 2. Technical Specifications

| Parameter | Specification | Notes / Conditions |
| :--- | :--- | :--- |
| **Model / Part Number** | ZCT-YOF07-C001 / ZCT-YLOC1 | |
| **Sensing Technology** | Non-contact Capacitive Sensing | Flexible FPC pad design |
| **Operating Voltage ($V_{DD}$)** | 2.8 V to 5.5 V DC (3 V – 5.5 V nominal) | 9.2 V required *only* on PRG pin during initial firmware programming |
| **Operating Current** | ~2 mA @ 5 V DC<br>~1 mA @ 3 V DC | **Sleep Mode:** ~8 µA |
| **Output Formats** | **Digital:** I2C (7-bit address default `0x40`)<br>**Analog:** PWM / Voltage Output | Real-time level comparison output ($0	ext{x}00 
ightarrow 0	ext{xFF}$) |
| **Measurement Range** | 120 mm effective range | Customizable length on request |
| **Level Accuracy / Error** | $\pm 3	ext{ mm}$ | Proper 3-point calibration required |
| **Response Hysteresis** | $\pm 0.25\%	ext{ FS}$ | |
| **Container Wall Thickness** | $< 5	ext{ mm}$ | Non-metallic containers only |
| **Spacer / Container Material** | Non-metallic | Plastic, Glass, Acrylic, Ceramic, etc. |
| **Compatible Liquids** | Water, Distilled Water, Milk, Juice, Wine, Oils, Alcohol, Coffee, Glue, Ink, Disinfectants, Chemical Solutions | Non-conductive & conductive liquids |
| **Operating Temperature** | $-20^\circ	ext{C}$ to $+85^\circ	ext{C}$ | Material temperature rating up to $200^\circ	ext{C}$ |
| **Protection Rating** | IP62 | Sealed electronics area |
| **Physical Dimensions** | $120	ext{ mm} 	imes 12	ext{ mm}$ | Thickness: Total ~2.5 mm (electronic gel area), 0.4 mm adhesive base |
| **Weight** | ~3 g | |

---

## 3. Physical Dimensions & Mechanical Design

* **Total Length:** 120 mm (Effective sensing range)
* **Width:** 12 mm
* **Total Thickness:** 2.5 mm at the encapsulated IC section; 0.4 mm adhesive backing.
* **Markings:**
  * **Zero Scale Line (Low Mark):** Located ~3 mm from the bottom edge of the FPC sensing strip.
  * **Full Scale Line (High Mark):** Located ~2 mm from the top encapsulation border / upper pad boundary.

---

## 4. Connector Pinout & Wiring Definition

The sensor uses a **5-pin** or **6-pin** JST-style / ribbon wiring harness:

| Pin # | Wire Color (Photo Ref) | Pin Name | Description |
| :---: | :---: | :---: | :--- |
| **1** | White | **PRG** | Firmware Programming Line (9.2 V applied during factory programming; leave floating / unconnected in normal MCU operation) |
| **2** | Red | **VCC ($V_{DD}$)** | Power Supply Positive (+2.8 V to +5.5 V DC) |
| **3** | Black | **GND ($V_{SS}$)** | Power Supply Ground (0 V reference) |
| **4** | Yellow | **SDA** | I2C Data Line (Requires external 4.7 kΩ pull-up to VCC if not present on MCU board) |
| **5** | Orange | **SCL** | I2C Clock Line (Requires external 4.7 kΩ pull-up to VCC if not present on MCU board) |
| **6** | Green | **PWM** | PWM Voltage Output (6-pin version; characteristics require confirmation) |

---

## 5. Protocol & Communication Details

### I2C Specifications
* **7-Bit Device Address:** `0x40` (Write: `0x80`, Read: `0x81`)
* **Data Byte Format:** The output reading is an 8-bit unsigned integer ranging from `0x00` (Empty / Below Zero Mark) to `0xFF` (Full Scale / Above Full Mark).
* **Direct Read Operation:**
  Reading 4 consecutive bytes starting from Register `0x00`:
  * `Buffer[0]`: Current Real-Time Liquid Level Value (`0x00` – `0xFF`).
  * `Buffer[1]` - `Buffer[3]`: REG1-REG3. During calibration these are the
    host handshake mailbox; their idle/status meaning is not documented.

The vendor I2C protocol handbook identifies the registers as:

| Register | Address | Reported function |
| --- | --- | --- |
| WL | `0x00` | Live level byte |
| REG1 | `0x01` | Handshake command / channel response |
| REG2 | `0x02` | Sample high byte |
| REG3 | `0x03` | Sample low byte |

The chip reportedly needs approximately 600 ms after power-on before normal
operation. The firmware uses 100 kHz, which is the conservative starting
point; the vendor maximum I2C frequency remains unconfirmed.

### Host calibration handshake (reported, not hardware-verified)

Vendor communication notes and sample code describe these runtime I2C
operations without using PRG:

1. **Read a channel sample:** write `0xCA` to REG1; poll until REG1 is `0xCD`;
   write `channel + 0x30` to REG1; poll until REG1 echoes the channel; read
   REG2:REG3 as a big-endian 16-bit sample.
2. **Write a threshold:** write `0x8C` to REG1; poll until REG1 is `0xCD`;
   write the big-endian sample to REG2:REG3; write `channel + 0x30` to REG1;
   poll until REG1 echoes the channel.

For continuous level calibration, the reported channel mapping is:

| Calibration state | Read channel | Write channel |
| --- | ---: | ---: |
| Empty / `TH1E` | 1 | 1 |
| Zero scale / `TH0Z` | 2 | 2 |
| Full scale / `TH0F` | 1 | 3 |
| Full scale / `TH1F` | 2 | 4 |

The firmware implements these commands behind an explicit calibration state
machine, but they must still be verified on the actual sensor at 3.3 V before
being considered production-ready. Factory programming routines involving
PRG/VPP and 9.2 V are not part of this project and must never be used for
calibration.

---

## 6. Calibration Procedure (3-Point Threshold Calibration)

To achieve accurate readings across different vessel wall thicknesses and liquid dielectric constants, 4 internal thresholds across 2 sensing channels (`CX0` and `CX1`) must be stored into the onboard EEPROM.

### 3-Step Calibration Sequence

1. **Step 1: Empty Container (Empty State - $TH_{1E}$)**
   * Ensure the container is completely dry and empty.
   * Send the Empty Calibration Command to the sensor (or trigger Key 1 once on the demo board).
   * Writes the $TH_{1E}$ threshold into internal EEPROM.

2. **Step 2: Low Mark / Zero Scale (Zero State - $TH_{0Z}$)**
   * Fill liquid exactly up to the **Zero Scale Line** (~3 mm above the bottom edge of the sensor strip).
   * *Note:* Zero scale state **cannot** be completely dry; a small initial amount of liquid at the baseline is required for correct capacitance initialization.
   * Send the Zero Scale Calibration Command (or trigger Key 1 twice on the demo board).
   * Writes the $TH_{0Z}$ threshold into internal EEPROM.

3. **Step 3: Full Scale Mark (Full State - $TH_{0F}$ & $TH_{1F}$)**
   * Fill liquid up to the **Full Scale Line** (~2 mm below the upper electronic gel boundary).
   * Send the Full Scale Calibration Command (or trigger Key 1 three times on the demo board).
   * Writes the $TH_{0F}$ and $TH_{1F}$ thresholds into internal EEPROM.

---

## 7. Mounting & Installation Guidelines

1. **Keep Clearance around Sensing Area:** Maintain at least **10 mm clearance** from any external metal components, conductive chassis, or wiring around the sensor strip to prevent false capacitive coupling.
2. **Surface Attachment:** Ensure the non-metallic container surface is clean and dry. Press the sensor strip firmly against the vessel outer wall, ensuring no air bubbles are trapped beneath the FPC adhesive.
3. **Curved Vessels:** For cylindrical or curved tanks, conform the flexible FPC smoothly around the curve.
4. **Hot-Plugging Notice:** Do **not** connect or disconnect the sensor while powered. Always power down before changing wiring connections.

---

## 8. Missing Information & Recommended Technical Clarifications

To ensure complete production and software integration readiness, the following additional details should be confirmed with the manufacturer or verified via hardware testing:

1. **Hardware verification of the host handshake:**
   * Confirm `0xCA`/`0x8C`, REG1 acknowledgements, channel mapping, and
     threshold persistence across power-off without PRG/9.2 V.
2. **I2C Speed & Timing:**
   * Maximum supported I2C clock rate (Standard Mode 100 kHz vs. Fast Mode 400 kHz).
3. **Idle status meaning:**
   * Meaning of REG1-REG3 during ordinary four-byte level reads.
4. **PWM Output Characteristics:**
   * Frequency, logic levels, and resolution for the 6-pin PWM version. The
     vendor handbook reports a 100 us period and duty `N/256`, still unverified
     on this sensor assembly.
5. **Interrupt Line Functionality:**
   * Whether the green/PWM pin also operates as a threshold alert output.
6. **Container Wall Limits:**
   * Minimum wall thickness and maximum recommended dielectric constant ($ arepsilon_r$) constraints for thick plastic or double-walled glass containers.