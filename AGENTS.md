# AGENTS.md

## Platform

This is an Arduino project targeting the **SEEED XIAO ESP32-C3** board.

## Build Environment

- **Board**: Select `XIAO_ESP32C3` in Arduino IDE
- **ESP32 Board Library**: Espressif esp32 version **2.0.14** (not higher due to display compatibility)

## Required Libraries (via Arduino Library Manager)

- TFT_eSPI (2.5.34)
- EasyButton (2.0.3)
- NimBLE Arduino (2.3.6)
- PNGdec (1.1.6)

## Hardware

- 1.8" Round GC9A01 display
- Charlieplexed LEDs
- MX switches with LEDs (interrupt driven)
- Battery powered with USB-C charging

## Important Guidelines

### Interrupt Service Routines (ISRs)

- **Do NOT use timers inside ISRs** - this will cause crashes or undefined behavior
- Keep ISRs as short as possible - set flags and handle logic in the main loop
- Avoid blocking operations, memory allocation, or Serial prints in ISRs

### BLE (NimBLE)

- Documentation: https://h2zero.github.io/NimBLE-Arduino/
- NimBLE is a lightweight BLE stack - prefer it over the default ESP32 BLE library

### Display Notes

- TFT_eSPI requires a custom `User_Setup.h` in `~/Documents/Arduino/libraries/TFT_eSPI`
- See README.md for the display configuration

## Code Style

- Follow existing patterns in radar.ino
- Use meaningful variable names
- Keep functions focused and small
