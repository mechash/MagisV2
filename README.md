# MagisV2 — ELRS Support

ExpressLRS (CRSF) receiver integration for the **Primus X2 flight controller** running MagisV2 firmware.

**Firmware target:** `PRIMUS_X2_v1`

---

## Overview

Adds CRSF protocol support on **USART2 (PA3)**, enabling control via any ELRS-compatible transmitter. ESP on USART1 remains available for development.

## Wiring

| ELRS Receiver | Primus X2       |
| ------------- | --------------- |
| TX            | PA3 (USART2 RX) |
| VCC           | 5V / 3.3V       |
| GND           | GND             |

## Quick Start

```cpp
void plutoRxConfig ( void ) {
  Receiver_Mode ( Rx_ELRS );
}
```

## AUX Channel Assignment

| Function        | Channel    | Range      | Trigger                |
| --------------- | ---------- | ---------- | ---------------------- |
| ARM + ANGLE     | CH6 (AUX2) | 1300–2100 | Switch HIGH            |
| MAG Calibration | CH7 (AUX3) | 1500–2100 | Switch HIGH (optional) |
| Developer Mode  | CH8 (AUX4) | 1500–2100 | Switch HIGH            |

## Arming Sequence

1. Power on, place drone flat
2. Wait for gyro/accel calibration (LED stops blinking)
3. Throttle stick fully down
4. Flip CH6 (AUX2) HIGH → armed

## CRSF Protocol

| Parameter   | Value                                 |
| ----------- | ------------------------------------- |
| Baud rate   | 420,000                               |
| Format      | 8N1, non-inverted                     |
| Channels    | 16 (11-bit packed)                    |
| Raw range   | 172–1811                             |
| RC range    | 988–2012                             |
| Frame types | RC Channels (0x16), Link Stats (0x14) |

## Receiver Modes

|             | Rx_ESP | Rx_PPM  | Rx_ELRS                  |
| ----------- | ------ | ------- | ------------------------ |
| UART        | USART1 | PPM Pin | USART2                   |
| Protocol    | MSP    | PWM     | CRSF                     |
| MAG enforce | Yes    | Yes     | No (optional via switch) |

## Channel Monitor (PlutoPilot.cpp)

Use the OLED joystick display to monitor ELRS channel values in developer mode:

```cpp
void plutoRxConfig ( void ) { Receiver_Mode ( Rx_ELRS ); }
void plutoInit ( void ) { Oled_Init(); }

void plutoLoop ( void ) {
  Oled_Begin();
  Oled_Text(4, 0, "ELRS MONITOR");
  Oled_Joysticks(
    RcData_Get(RC_THROTTLE), RcData_Get(RC_YAW),
    RcData_Get(RC_ROLL), RcData_Get(RC_PITCH)
  );
  Oled_Text(0, 52, "ARM");  Oled_Number(20, 52, RcData_Get(RC_AUX2));
  Oled_Text(68, 52, "DEV"); Oled_Number(88, 52, RcData_Get(RC_AUX4));
  Oled_End();
}

void onLoopFinish ( void ) { Oled_SystemMode(); }
```

---

## Documentation

- [ELRS Integration Report](report/ELRS_Integration_Report.md) — engineering report with challenges, fixes, and architecture
- [OLED API Wiki](docs/OLED_API_WIKI.md) — OLED display API reference

## Hardware

| Property          | Value                                |
| ----------------- | ------------------------------------ |
| Flight Controller | Primus X2                            |
| Firmware Target   | PRIMUS_X2_v1                         |
| MCU               | STM32F303xC (72 MHz, 40 KB RAM)      |
| ELRS Port         | USART2 (PA3 RX) — 420,000 baud CRSF |
| ESP Port          | USART1 (PA9/PA10) — MSP protocol    |

## File Structure

```
src/main/rx/crsf.c              — CRSF protocol driver
src/main/rx/crsf.h              — CRSF header
src/main/API/RxConfig.h          — Receiver mode enum (ESP/PPM/CAM/ELRS)
src/main/API-Src/RxConfig.cpp    — Receiver mode configuration
PlutoPilot.cpp                   — User code entry point
report/ELRS_Integration_Report.html — Engineering report
```

---

## Contributing

Fork this branch for any new improvements and let us know by opening a pull request.
