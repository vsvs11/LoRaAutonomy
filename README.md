# 📡 LoRaAutonomy

> A lightweight, modular operating environment and autonomous mesh networking stack for ESP32 and RP2040 microcontrollers running FreeRTOS / ESP-IDF.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP--IDF%20%7C%20FreeRTOS-green.svg)](https://github.com/espressif/esp-idf)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32%20%2F%20RP2040%20%2F%20LoRa-orange.svg)]()

---

## 🎯 Overview

**LoRaAutonomy** is an open-source decentralized networking stack designed for resilient, off-grid telemetry and packet routing.

Unlike monolithic firmware solutions, LoRaAutonomy focuses on:
- **Modular Microkernel Core:** Strict task lifecycle management (`create_task`, `kill_task`, `kill_subsystem`).
- **Subsystem Isolation:** Core subsystems run in decoupled FreeRTOS tasks to prevent network stack panics from crashing peripheral handlers.
- **Low Overhead:** Pure C implementation targeting deterministic execution on resource-constrained embedded targets.

---

## 🛠 Target Hardware

* **Primary MCU:** ESP32 / ESP32-S3 (ESP-IDF)
* **RF Transceivers:** Semtech SX1262 / SX1276 / SX1278 (SPI)
* **Operating System:** FreeRTOS (SMP configuration on dual-core targets)

---

## 📂 Project Structure

```text
├── docs/             # Protocol frames, task lifecycle, and architecture specifications
├── src/
|   ├── drivers/
|   |   ├── ili9341.c     #ILI9341 display driver
|   |   ├── ili9341.h     # ILI9341 display interface
|   |   ├── sx1276.c      #LoRa driver
|   |   └── sx1276.h      #LoRa interface
│   ├── kernel.c          # Subsystem task scheduler and memory manager
│   ├── kernel.h          # Kernel interface and lifecycle declarations
│   ├── list.c / .h       # Active task tracking structures
│   ├── subsystems.h      # Subsystem masks and definitions
│   └── main.c            # Hardware init and core bootloader
├── platformio.ini        # PlatformIO build matrix
└── README.md
```

---

## 🚀 Roadmap & Current Status

**Current Stage: Early Core Development**

- [x] Basic task dispatcher and subsystem registry (`kill_subsystem`, task list traversal)
- [ ] SPI hardware abstraction layer for Semtech LoRa transceivers
- [ ] Network frame specification (Addressing, Hop limits, CRC16)
- [ ] Dynamic packet routing algorithm for multi-hop mesh
- [ ] RP2040 UART/SPI inter-chip bridge protocol
- [ ] Non-volatile configuration storage (NVS)

---

## 🔧 Building from Source

### Prerequisites
* [PlatformIO Core (CLI)](https://platformio.org/) or VS Code with the PlatformIO extension.
* ESP-IDF toolchain.

### Build & Flash
```bash
# Clone the repository
git clone [https://github.com/YOUR_USERNAME/LoRaAutonomy.git](https://github.com/YOUR_USERNAME/LoRaAutonomy.git)
cd LoRaAutonomy

# Build firmware
pio run

# Upload to connected target
pio run --target upload

# Open serial monitor
pio device monitor
```

---

## 🤝 Contributing

Contributions, bug reports, and architectural RFCs are welcome!

1. Fork the Project.
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`).
3. Commit your Changes (`git commit -m 'feat: Add support for SX1262'`).
4. Push to the Branch (`git push origin feature/AmazingFeature`).
5. Open a Pull Request.

Please check [docs/](docs/) before submitting changes affecting the network packet structure.

---

## 💖 Support the Project

If you find this project interesting and want to support hardware prototyping (custom PCBs, LoRa transceivers, testing gear):

* **DonationAlerts:** [donationalerts.com/r/vsvs11](https://www.donationalerts.com/r/vsvs11)
---

## 💬 Community & Devlog

* **Telegram Devlog:** [t.me/lora_autonomy](https://t.me/lora_autonomy) *(Development logs, hardware testing, and discussions)*
* **License:** Distributed under the MIT License.
