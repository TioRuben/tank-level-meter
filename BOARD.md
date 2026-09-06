# LilyGO TTGO T-Energy (ESP32-WROVER-B) Technical Specifications & Configuration Guide

This document provides complete technical specifications, pinout mappings, hardware configuration details, and setup instructions for using the **LilyGO TTGO T-Energy** development board with **VS Code** and **ESP-IDF**.

---

## 1. Board Overview

The **LilyGO TTGO T-Energy** is an ESP32-based development board designed specifically for battery-powered and low-power IoT applications. It integrates an onboard **18650 Lithium-Ion battery holder**, charging circuitry, power management controls, and the high-performance **ESP32-WROVER-B** module with 8 MB of external PSRAM.

### Key Specifications

| Specification | Details |
| :--- | :--- |
| **Microcontroller Core** | ESP32 (Xtensa® Dual-Core 32-bit LX6, up to 240 MHz) |
| **SoC Variant** | ESP32-D0WD-V3 / ESP32-D0WDQ6-V3 |
| **Module Type** | **ESP32-WROVER-B** |
| **Embedded Flash** | 4 MB SPI Flash (QSPI) |
| **Embedded PSRAM** | **8 MB Pseudo Static RAM (PSRAM)** (3.3V Mode) |
| **SRAM Internal** | 520 KB SRAM |
| **USB-to-UART Chip** | **CP2104** (Silicon Labs) |
| **Power Management** | Integrated TP4056/HX6610S Lithium Charger + ON/OFF Slide Switch |
| **Battery Socket** | Onboard 18650 Li-Ion Cell Holder (3.7V / 4.2V Max) |
| **Charging Current** | 500mA - 1000mA (via Micro-USB / USB-C depending on board revision) |
| **Wi-Fi Connectivity** | 802.11 b/g/n (up to 150 Mbps) |
| **Bluetooth** | Bluetooth v4.2 BR/EDR and BLE standard |
| **Antenna** | Onboard PCB Antenna / IPEX Connector option |
| **Board Dimensions** | ~91.1 mm × 32.8 mm × 19.9 mm |

---

## 2. Hardware Architecture & Pin Allocation

### High-Voltage & Internal Bus Allocation
* **PSRAM Bus (Occupied):** `GPIO16` and `GPIO17` are connected internally to the 8 MB PSRAM. **Do not use GPIO16/17 for external peripherals.**
* **SPI Flash Bus (Occupied):** `GPIO6`, `GPIO7`, `GPIO8`, `GPIO9`, `GPIO10`, and `GPIO11` are connected to the internal SPI flash memory.
* **Input-Only Pins (No Pull-up/Down):** `GPIO34`, `GPIO35`, `GPIO36` (VP), `GPIO39` (VN).
* **Strapping Pins:** `GPIO0` (Boot mode), `GPIO2` (Auto-download/LED), `GPIO5`, `GPIO12` (MTDI - Voltage strap), `GPIO15` (MTDO).

---

## 3. Pinout Specification

The board features two 19-pin header rows breaking out power and ESP32 GPIOs:

### Header Pin Map

| Header Pin | Label / GPIO | Type | Function / Peripheral Capabilities | Notes / Constraints |
| :---: | :---: | :---: | :--- | :--- |
| **1** | **3V3** | Power | 3.3V Power Output / Input Rail | Regulated output from onboard LDO |
| **2** | **EN** | Input | Reset Button / Chip Enable | Active Low (Pull down to reset) |
| **3** | **VP / GPIO36**| Input | ADC1_CH0, RTC_GPIO0 | **Input-Only**, No internal pull-up |
| **4** | **VN / GPIO39**| Input | ADC1_CH3, RTC_GPIO3 | **Input-Only**, No internal pull-up |
| **5** | **GPIO34** | Input | ADC1_CH6, RTC_GPIO4 | **Input-Only**, General Analog/Digital |
| **6** | **GPIO35** | Input | ADC1_CH7, RTC_GPIO5 | **Input-Only**, General Analog/Digital |
| **7** | **GPIO32** | I/O | ADC1_CH4, TOUCH9, XTAL_32K_P | General I/O, RTC, Touch |
| **8** | **GPIO33** | I/O | ADC1_CH5, TOUCH8, XTAL_32K_N | General I/O, RTC, Touch |
| **9** | **GPIO25** | I/O | DAC1, ADC2_CH8, RTC_GPIO6 | True Analog Output (DAC1), PWM |
| **10** | **GPIO26** | I/O | DAC2, ADC2_CH9, RTC_GPIO7 | True Analog Output (DAC2), PWM |
| **11** | **GPIO27** | I/O | ADC2_CH7, TOUCH7, RTC_GPIO17 | General I/O, SPI, PWM |
| **12** | **GPIO14** | I/O | ADC2_CH6, TOUCH6, MTMS | JTAG TMS / General I/O |
| **13** | **GPIO12** | I/O | ADC2_CH5, TOUCH5, MTDI | Strapping Pin (**Boot fails if pulled High**) |
| **14** | **GPIO13** | I/O | ADC2_CH4, TOUCH4, MTCK | JTAG TCK / General I/O |
| **15** | **GPIO15** | I/O | ADC2_CH3, TOUCH3, MTDO | Strapping Pin (JTAG TD0) |
| **16** | **GPIO2** | I/O | ADC2_CH2, TOUCH2, Boot Strap | Strapping Pin / Onboard Debug LED |
| **17** | **GPIO0** | I/O | ADC2_CH1, TOUCH1, BOOT Button | Strapping Pin (**BOOT Mode**, Active Low) |
| **18** | **GPIO4** | I/O | ADC2_CH0, TOUCH0, RTC_GPIO10 | General I/O, Touch |
| **19** | **GPIO5** | I/O | VSPI SS, RTC_GPIO11 | Strapping Pin (VSPI CS default) |
| **20** | **GPIO18** | I/O | VSPI CLK | Hardware SPI Clock |
| **21** | **GPIO19** | I/O | VSPI MISO | Hardware SPI MISO |
| **22** | **GPIO21** | I/O | I2C SDA | Default I2C Data Line |
| **23** | **GPIO22** | I/O | I2C SCL | Default I2C Clock Line |
| **24** | **GPIO23** | I/O | VSPI MOSI | Hardware SPI MOSI |
| **25** | **RXD0 (GPIO3)**| I/O | UART0 RX | Connected to USB CP2104 TX |
| **26** | **TXD0 (GPIO1)**| I/O | UART0 TX | Connected to USB CP2104 RX |
| **27** | **GND** | Power | Ground Reference | Common System Ground |
| **28** | **5V / VBUS** | Power | 5V Input / USB Power Rail | Direct supply from USB or VBUS |

*Note: ADC2 pins cannot be used for analog sampling when Wi-Fi is actively enabled. Use ADC1 (GPIO32–GPIO39) for analog readings during active Wi-Fi operation.*

---

## 4. ESP-IDF Setup Configuration (`sdkconfig`)

To set up the TTGO T-Energy properly in **ESP-IDF** (VS Code extension), configure your project targets and menuconfig options.

### 1. Set Target Chip
In VS Code Terminal or ESP-IDF Command Prompt:
```bash
idf.py set-target esp32
```

### 2. Required `sdkconfig` Settings

Run `idf.py menuconfig` and apply the following critical settings:

#### A. PSRAM Configuration (Required for ESP32-WROVER-B)
* **Component config** $
ightarrow$ **ESP PSRAM**
  * Enable **Support for external, SPI-connected RAM** (`CONFIG_SPIRAM=y`)
  * SPI RAM access method: **Make RAM allocable via heap_caps_malloc** (`CONFIG_SPIRAM_USE_MALLOC=y`)
  * PSRAM Clock Speed: **40MHz** or **80MHz** (Depending on module stability, 40MHz is safest for initial builds)
  * Mode: **Quad SPI (QSPI)**

#### B. Flash & Memory Settings
* **Component config** $
ightarrow$ **ESP32-specific**
  * Flash Size: **4MB** (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`)
  * Flash SPI Mode: **DIO** or **QIO** (Recommended: **DIO**)
  * Flash SPI Speed: **40MHz** or **80MHz**

#### C. Serial / Upload Settings
* Port Baud Rate: **115200** or **921600** (Supported by CP2104)

---

## 5. VS Code Configuration Files

### `.vscode/settings.json`
```json
{
    "cmake.configureOnOpen": true,
    "idf.adapterTargetName": "esp32",
    "idf.openOcdConfigs": [
        "board/esp32-wrover-kit-3.3v.cfg"
    ],
    "idf.port": "/dev/ttyUSB0", 
    "idf.flashType": "UART"
}
```
*(Replace `/dev/ttyUSB0` with your serial port, e.g., `COM3` on Windows)*

---

## 6. Complete ESP-IDF Sample Test Project

Here is a minimal `main.c` program that demonstrates PSRAM verification, GPIO control, and system initialization in ESP-IDF.

### `main/main.c`

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#define BLINK_GPIO GPIO_NUM_2

static const char *TAG = "T-ENERGY_TEST";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting LilyGO TTGO T-Energy ESP-IDF Initialization...");

    // 1. Log Internal Heap Memory
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "Free Internal SRAM: %d bytes", free_internal);

    // 2. Log External PSRAM Memory
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (free_psram > 0) {
        ESP_LOGI(TAG, "PSRAM Initialized Successfully! Free PSRAM: %d bytes", free_psram);
    } else {
        ESP_LOGE(TAG, "PSRAM Not Detected or Disabled in sdkconfig!");
    }

    // 3. Configure Onboard LED (GPIO2)
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        ESP_LOGI(TAG, "Turning LED ON");
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Turning LED OFF");
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
```

---

## 7. Power Management & Low Power Tips

1. **Battery Operation:** The onboard 18650 cell powers the board via an onboard LDO regulator. Use the integrated slide switch to turn off power completely when stored.
2. **Deep Sleep:** To maximize battery life:
   ```c
   // Deep sleep configuration example
   esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0); // Wake up when GPIO33 goes LOW
   esp_deep_sleep_start();
   ```
3. **ADC Battery Level Reading:** If your board revision includes an internal resistor divider for battery voltage sensing, measure via ADC1 pins (`GPIO34` / `GPIO35`).