# MagisV2 Firmware — ELRS (CRSF) Integration Report

**Branch:** OLED_REVAMP_V2 | **Target:** PRIMUS_X2_v1  
**Repository:** DronaAviation/MagisV2  
**Author:** Omkar Dandekar (techsavvyomi)  
**Date:** 9 April 2026  
**Firmware:** v3.0.1

---

## 1. Executive Summary

This report documents the integration of **ExpressLRS (ELRS)** receiver support into the **MagisV2** flight controller firmware running on the **Primus X2** flight controller. The work was carried out on the `OLED_REVAMP_V2` branch, which already contained a revamped OLED display subsystem. The ELRS integration adds **CRSF protocol** support, enabling the drone to be controlled via industry-standard ELRS transmitters over a dedicated UART serial link.

The implementation includes a full CRSF frame parser, receiver mode configuration via the PlutoPilot API, runtime-conditional magnetometer calibration handling, and appropriate UART baud rate extensions. All changes are scoped to the `Rx_ELRS` mode and do not affect existing ESP, PPM, or CAM receiver paths.

| Metric | Value |
|--------|-------|
| Files Changed | 15 |
| New Source Files | 3 |
| Lines Added (ELRS) | ~300 |
| RC Channels | 16 |

---

## 2. Background & Motivation

The MagisV2 firmware (based on the Cleanflight/Magis codebase) originally supported three receiver modes:

- **Rx_ESP** — onboard ESP8266 WiFi module (MSP protocol over USART1)
- **Rx_PPM** — standard PPM signal on dedicated input pin
- **Rx_CAM** — WiFi camera module (MSP protocol)

**ExpressLRS (ELRS)** is a modern, open-source, long-range RC link system that uses the **Crossfire (CRSF) protocol** at 420,000 baud. It offers significant advantages over WiFi-based control: lower latency (~5ms at 200Hz), longer range, and standard RC transmitter compatibility. Integrating ELRS enables the Primus X2 to be used with any ELRS-compatible transmitter (RadioMaster, TBS, etc.), making it suitable for outdoor and FPV applications.

---

## 3. System Architecture

### 3.1 Hardware Wiring

```
ELRS Receiver              Primus X2 (STM32)
┌──────────────┐           ┌──────────────────────┐
│  TX ──────────────────── PA3 (USART2 RX)        │
│  RX                      (not connected)         │
│  VCC ─────────────────── 5V / 3.3V              │
│  GND ─────────────────── GND                    │
└──────────────┘           └──────────────────────┘

USART1 (PA9/PA10) → ESP module (MSP) — remains available
USART2 (PA2/PA3)  → ELRS receiver (CRSF @ 420000 baud)
```

### 3.2 Data Flow

```
ELRS TX → ELRS RX Module → CRSF @ 420k baud → USART2 (PA3) → crsfDataReceive() ISR
       → crsfFrameStatus() → CRC8 Validate → Unpack 16ch → rcData[] → Flight Controller
```

---

## 4. CRSF Protocol Implementation

### 4.1 Frame Format

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync / Device Address | `0xC8` (flight controller address) |
| 1 | Frame Length | Number of bytes following (type + payload + CRC) |
| 2 | Frame Type | `0x16` = RC Channels, `0x14` = Link Stats |
| 3..N-1 | Payload | 22 bytes for RC Channels (16ch x 11-bit packed) |
| N | CRC8 | Polynomial 0xD5 (Castagnoli), covers type + payload |

### 4.2 Channel Mapping

| CRSF Ch | RC Channel | Function | CRSF Raw Range | RC Range (mapped) |
|---------|-----------|----------|----------------|-------------------|
| 1 | Roll | Right stick L/R | 172 – 1811 | 988 – 2012 |
| 2 | Pitch | Right stick U/D | 172 – 1811 | 988 – 2012 |
| 3 | Throttle | Left stick U/D | 172 – 1811 | 988 – 2012 |
| 4 | Yaw | Left stick L/R | 172 – 1811 | 988 – 2012 |
| 5 | AUX1 | Available | 172 – 1811 | 988 – 2012 |
| 6 | AUX2 | **ARM + ANGLE** | 172 – 1811 | 988 – 2012 |
| 7 | AUX3 | **MAG Calibration** | 172 – 1811 | 988 – 2012 |
| 8 | AUX4 | **Developer Mode** | 172 – 1811 | 988 – 2012 |
| 9–16 | AUX5–12 | Available for expansion | 172 – 1811 | 988 – 2012 |

### 4.3 Channel Value Conversion

```c
// CRSF value (172..1811) to standard RC value (988..2012)
rcValue = crsfValue * 0.62477f + 880.53f

// Examples:
//   172  →  988  (stick minimum)
//   992  → 1500  (stick center)
//  1811  → 2012  (stick maximum)
```

### 4.4 Serial Configuration

| Parameter | Value |
|-----------|-------|
| Baud Rate | 420,000 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |
| Inversion | Non-inverted |
| Frame Gap Timeout | 4000 us |

---

## 5. Files Changed

### 5.1 New Files

| File | Lines | Purpose |
|------|-------|---------|
| `src/main/rx/crsf.c` | 268 | CRSF protocol driver: ISR callback, CRC8 validation, 16-channel unpacking, link statistics parsing |
| `src/main/rx/crsf.h` | 31 | CRSF header: `crsfInit()`, `crsfFrameStatus()` declarations |
| `src/test/unit/rx_crsf_unittest.cc` | - | Google Test unit test for CRSF frame parsing and CRC validation |

### 5.2 Modified Files

| File | Type | Change Description |
|------|------|-------------------|
| `src/main/rx/rx.h` | Modified | Added `SERIALRX_CRSF = 7` to `SerialRXType` enum |
| `src/main/rx/rx.cpp` | Modified | Wired `crsfInit()` and `crsfFrameStatus()` into `serialRxInit()` and `serialRxFrameStatus()` |
| `src/main/API/RxConfig.h` | Modified | Added `Rx_ELRS` to `rx_mode_e` enum |
| `src/main/API-Src/RxConfig.cpp` | Modified | Full `Rx_ELRS` case: USART2 config, CRSF-tuned thresholds, ARM/ANGLE/MAG/DEV aux setup |
| `src/main/API/Serial-IO.h` | Modified | Added `BAUD_RATE_420000` to baud rate enum |
| `src/main/API-Src/Serial-IO-Uart.cpp` | Modified | Added `case BAUD_RATE_420000: return 420000;` to `getBaud()` |
| `src/main/mw.cpp` | Modified | Skip MAG_ENFORCE arming block when `serialrx_provider == SERIALRX_CRSF` |
| `Makefile` | Modified | Added `rx/crsf.c` to `MAIN_RX` sources |
| `src/test/Makefile` | Modified | Added `rx_crsf_unittest` build target |
| `PlutoPilot.cpp` | Modified | Set `Receiver_Mode(Rx_ELRS)`, clean template |

### 5.3 Deleted Files

| File | Reason |
|------|--------|
| `src/main/API/PlutoPilot.cpp` | Duplicate of root `PlutoPilot.cpp`; removed to avoid conflicts |

---

## 6. Receiver Configuration (Rx_ELRS)

### 6.1 Mode Configuration

```cpp
case Rx_ELRS: {
    AuxChangeEnable = true;
    featureSet ( FEATURE_RX_SERIAL );
    masterConfig.rxConfig.serialrx_provider = 7;          // SERIALRX_CRSF
    masterConfig.serialConfig.portConfigs[1].functionMask
        = FUNCTION_RX_SERIAL;                             // USART2

    // CRSF-tuned validation thresholds
    masterConfig.rxConfig.rx_min_usec = 885;
    masterConfig.rxConfig.rx_max_usec = 2115;
    masterConfig.rxConfig.mincheck    = 1050;
    masterConfig.rxConfig.maxcheck    = 1900;

    // AUX channel assignments
    Receiver_Aux_Config ( Mode_ARM,   Rx_AUX2, 1300, 2100 );
    Receiver_Aux_Config ( Mode_ANGLE, Rx_AUX2,  900, 2100 );
    Receiver_Aux_Config ( Mode_MAG,   Rx_AUX3, 1500, 2100 );

    Receiver_Config_Mode_Dev ( Rx_AUX4, 1500, 2100 );

    ESP_WiFi_Status = true;    // ESP on USART1 remains available
}
```

### 6.2 AUX Channel Assignment

| Function | CRSF Channel | RC Channel | Activation Range | Trigger |
|----------|-------------|-----------|-----------------|---------|
| **ARM + ANGLE** | CH6 | AUX2 | 1300 – 2100 | Switch HIGH (>1300) |
| **MAG Calibration** | CH7 | AUX3 | 1500 – 2100 | Switch HIGH (>1500) |
| **Developer Mode** | CH8 | AUX4 | 1500 – 2100 | Switch HIGH (>1500) |

### 6.3 Comparison with Other Modes

| Parameter | Rx_ESP / Rx_CAM | Rx_PPM | Rx_ELRS |
|-----------|----------------|--------|---------|
| Feature Flag | FEATURE_RX_MSP | FEATURE_RX_PPM | FEATURE_RX_SERIAL |
| UART Port | USART1 | PPM Pin | USART2 |
| Baud Rate | N/A (MSP) | N/A (PWM) | 420,000 |
| MAG AUX | AUX1, 900–1300 | AUX1, 900–1300 | AUX3, 1500–2100 |
| MAG Enforce | Yes (blocks arming) | Yes (blocks arming) | No (optional via switch) |
| ESP Available | No (shared UART) | Yes | Yes |

---

## 7. Challenges Faced & Resolutions

### Challenge 1: Drone Stuck in Magnetometer Calibration at Boot

After initial ELRS integration, the drone would get stuck in magnetometer calibration on every power-up, preventing arming entirely. The green LED blinked indefinitely.

**Root cause:** The original `Rx_ELRS` config inherited PPM-style AUX mappings where MAG was assigned to AUX1 with range 900–1300. Unconfigured ELRS channels default to ~988, which falls inside this range, triggering MAG mode on boot.

**Fix:** (1) Moved MAG to AUX3 with range 1500–2100 so only a deliberate switch flip triggers it. (2) Added runtime skip of `MAG_ENFORCE` in `mw.cpp` for CRSF provider so uncalibrated mag never blocks arming.

### Challenge 2: 420,000 Baud Not Available in UART API

The `UART_Baud_Rate_e` enum only went up to 256,000 baud. CRSF requires exactly 420,000 baud, and there was no way to configure this through the PlutoPilot Serial-IO API.

**Fix:** Added `BAUD_RATE_420000` to the enum in `Serial-IO.h` and the corresponding case in `getBaud()` in `Serial-IO-Uart.cpp`. This also enables future use of 420k baud for other protocols.

### Challenge 3: Arming Failure with ELRS Throttle Values

Even with correct AUX switch positions, the drone would not arm. The throttle-low detection was unreliable with CRSF channel values.

**Root cause:** The default `mincheck` was 1100. CRSF throttle-minimum maps to ~988, which is below 1100 and should work. However, setting explicit CRSF-tuned thresholds (`mincheck = 1050`, `rx_min_usec = 885`, `rx_max_usec = 2115`) in the `Rx_ELRS` config ensured reliable pulse validation and throttle detection across all ELRS packet rates.

### Challenge 4: USART Port Conflict with ESP Module

Initial design placed ELRS on USART1 (PA9/PA10), which is shared with the onboard ESP module. This meant ESP WiFi had to be disabled, losing developer mode access via the app.

**Fix:** Moved ELRS to **USART2** (PA2/PA3, the Unibus port). This keeps ESP on USART1 fully functional (`ESP_WiFi_Status = true`), allowing simultaneous ELRS flight control and ESP connectivity for development and configuration.

### Challenge 5: MAG_ENFORCE Change Affecting All Receiver Modes

Commenting out `#define MAG_ENFORCE` in `target.h` disabled mag calibration enforcement globally, affecting ESP and PPM modes that rely on it.

**Fix:** Restored `MAG_ENFORCE` in `target.h` and added a **runtime check** in `mw.cpp`: `if (serialrx_provider != SERIALRX_CRSF)`. This preserves mag enforcement for ESP/PPM/CAM while skipping it only for ELRS.

---

## 8. Arming Sequence & Pre-flight Checklist

### 8.1 Pre-flight Conditions

| # | Condition | Check | Status |
|---|-----------|-------|--------|
| 1 | Gyro/Accel calibrated | Auto at boot, wait for LED steady | Auto |
| 2 | Drone level | SMALL_ANGLE state — keep flat on surface | Manual |
| 3 | Throttle LOW | rcData[THROTTLE] < 1050 (mincheck) | Manual |
| 4 | Battery OK | Not in WARNING or CRITICAL state | Auto |
| 5 | ELRS link active | Receiver bound and sending frames | Auto |
| 6 | No crash flag | Place level for 300ms to clear | Auto |
| 7 | Mag calibration | **Optional** — flip CH7 > 1500 when needed | Optional |

### 8.2 Arming Steps

```
Power On → Wait Calibration → LED Steady → Throttle Down → Flip CH6 HIGH → ARMED
```

### 8.3 Transmitter Switch Setup

| Switch | Channel | Position LOW (~1000) | Position HIGH (~2000) |
|--------|---------|---------------------|----------------------|
| Switch A | CH6 (AUX2) | Disarmed | **Armed + Angle Mode** |
| Switch B | CH7 (AUX3) | Normal | **MAG Calibration** |
| Switch C | CH8 (AUX4) | Normal | **Developer Mode** |

---

## 9. OLED Subsystem Revamp (Existing Branch Work)

The `OLED_REVAMP_V2` branch also includes a complete revamp of the OLED display subsystem (committed prior to the ELRS work). Key additions:

- **Simple API Layer:** `Oled_Begin()`, `Oled_End()`, `Oled_Text()`, `Oled_Number()`, `Oled_Rect()`, `Oled_Circle()`, `Oled_Line()`
- **Robot Eyes:** `Oled_RobotEyes()`, `Oled_RoundEyes()`, `Oled_DeadEyes()`
- **Telemetry Widgets:** `Oled_Joysticks()`, `Oled_PitchLine()`, `Oled_RollLine()`
- **Batch I2C Writes:** ~13x faster than byte-by-byte transfers
- **Diff-based Updates:** Only changed pixels sent over I2C (non-blocking)
- **System/User Mode:** Ownership control prevents rendering conflicts
- **Armed-state Safety:** Guards on `Oled_Clear()` and mode switches

Files: `Oled.cpp` (1009 lines), `Oled.h` (359 lines), display driver updates, `OLED_API_WIKI.md` documentation.

---

## 10. Testing & Validation

### 10.1 Unit Tests

A Google Test suite (`rx_crsf_unittest.cc`) covers CRSF frame parsing:

- CRC8 table generation and validation
- RC Channels Packed frame unpacking (16 channels)
- Invalid frame rejection (bad CRC, truncated frames)
- Frame gap timeout and re-sync

### 10.2 Hardware Validation

| Test | Method | Result |
|------|--------|--------|
| CRSF frame reception | OLED channel monitor in dev mode | All 16 channels decoded correctly |
| Channel value range | Stick endpoints on transmitter | 988–2012 mapped correctly |
| Arming via AUX2 | Switch flip with throttle low | Arms/disarms reliably |
| MAG cal via AUX3 | Switch flip > 1500 | Calibration triggers on demand |
| Dev mode via AUX4 | Switch flip on transmitter | Enters/exits dev mode |
| ESP coexistence | App connection while ELRS active | ESP on USART1 functional |
| Boot without mag | Power cycle with no mag cal | Arms without mag block (ELRS only) |

### 10.3 Build Output

```
Target:   PRIMUS_X2_v1
Firmware: ELRS_PRIMUS_X2_v1_3.0.1.hex
Format:   Intel HEX (flash via ST-Link / DFU)
```

---

## 11. Future Work

- **CRSF Telemetry (uplink):** Send battery voltage, GPS, attitude back to the transmitter via CRSF telemetry frames
- **Link Quality Integration:** Use parsed RSSI/SNR/LQ from Link Statistics frames for OSD or failsafe decisions
- **ELRS on USART1 Option:** Allow ELRS on USART1 (replacing ESP) for setups that don't need WiFi
- **Altitude Hold & GPS Modes:** Add BARO and GPS AUX channel configs for ELRS when needed
- **OLED Flight HUD:** Combine ELRS channel data with sensor telemetry on OLED during flight

---

## 12. References

- **MagisV2 Repository:** https://github.com/DronaAviation/MagisV2
- **CRSF Protocol Specification:** TBS Crossfire Serial Protocol (CRSF) v3
- **ExpressLRS Documentation:** https://www.expresslrs.org/
- **Cleanflight Source:** Original serial RX implementation (SBUS, Spektrum, SUMD, xBus)
- **STM32F1 Reference Manual:** USART peripheral configuration, baud rate generation
- **SSD1306 Datasheet:** OLED display controller (128x64, I2C interface)

---

*MagisV2 Firmware — ELRS Integration Report — Drona Aviation — April 2026*  
*Branch: OLED_REVAMP_V2 | Target: PRIMUS_X2_v1 | Author: Omkar Dandekar*
