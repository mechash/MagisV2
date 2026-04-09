# MagisV2 — ELRS & OLED

MagisV2 firmware for the PlutoX2 nano drone with **ExpressLRS (CRSF) receiver support** and a **revamped 128x64 OLED display API**.

---

## ELRS (ExpressLRS) Support

### Overview

ELRS integration adds CRSF protocol support on **USART2 (PA3)**, enabling control via any ELRS-compatible transmitter. ESP on USART1 remains available for development.

### Wiring

| ELRS Receiver | Primus X2 v1 |
|---------------|--------------|
| TX | PA3 (USART2 RX) |
| VCC | 5V / 3.3V |
| GND | GND |

### Quick Start

```cpp
void plutoRxConfig ( void ) {
  Receiver_Mode ( Rx_ELRS );
}
```

### AUX Channel Assignment

| Function | Channel | Range | Trigger |
|----------|---------|-------|---------|
| ARM + ANGLE | CH6 (AUX2) | 1300–2100 | Switch HIGH |
| MAG Calibration | CH7 (AUX3) | 1500–2100 | Switch HIGH (optional) |
| Developer Mode | CH8 (AUX4) | 1500–2100 | Switch HIGH |

### Arming Sequence

1. Power on, place drone flat
2. Wait for gyro/accel calibration (LED stops blinking)
3. Throttle stick fully down
4. Flip CH6 (AUX2) HIGH → armed

### CRSF Protocol Details

| Parameter | Value |
|-----------|-------|
| Baud rate | 420,000 |
| Format | 8N1, non-inverted |
| Channels | 16 (11-bit packed) |
| Raw range | 172–1811 |
| RC range | 988–2012 |
| Frame types | RC Channels (0x16), Link Stats (0x14) |

### Receiver Modes Comparison

| | Rx_ESP | Rx_PPM | Rx_ELRS |
|---|--------|--------|---------|
| UART | USART1 | PPM Pin | USART2 |
| Protocol | MSP | PWM | CRSF |
| MAG enforce | Yes | Yes | No (optional via switch) |
| ESP available | No | Yes | Yes |

---

## OLED API

128x64 monochrome OLED display API for the PlutoX2 nano drone.

### Quick Start

```cpp
void plutoInit ( void ) { Oled_Init(); }

void plutoLoop ( void ) {
  Oled_Begin();
  Oled_Text(10, 20, "Hello Pluto!");
  Oled_RobotEyes(0, 0);
  Oled_End();
}

void onLoopFinish ( void ) { Oled_SystemMode(); }
```

No buffers. No memset. No mode switching. Just draw.

### How It Works

**SYSTEM mode** (default) — firmware shows battery, attitude, RC data, flight status automatically. Just call `Oled_Init()` in `plutoInit()`.

**USER mode** — you draw anything. Call `Oled_Begin()` at the start of your frame, draw with `Oled_*` functions, call `Oled_End()` to send to display. Call `Oled_SystemMode()` in `onLoopFinish()` to return to telemetry.

### Simple API

#### Frame

| Function | Description |
|----------|-------------|
| `Oled_Begin()` | Start frame (clears buffer, auto enters USER mode) |
| `Oled_End()` | Send frame to display (only changed pixels) |

#### Text

| Function | Description |
|----------|-------------|
| `Oled_Text(x, y, "string")` | Draw text. 6px/char, 7px tall. |
| `Oled_Number(x, y, 123)` | Draw integer (handles negatives) |

#### Shapes

| Function | Description |
|----------|-------------|
| `Oled_Pixel(x, y)` | Single pixel |
| `Oled_Line(x0, y0, x1, y1)` | Line between two points |
| `Oled_Rect(x, y, w, h)` | Filled rectangle |
| `Oled_RoundRect(x, y, w, h, r)` | Filled rounded rectangle |
| `Oled_Circle(cx, cy, r)` | Filled circle |
| `Oled_RectOutline(x, y, w, h)` | Rectangle border |
| `Oled_RoundRectOutline(x, y, w, h, r)` | Rounded rectangle border |
| `Oled_CircleOutline(cx, cy, r)` | Circle border |
| `Oled_Erase(x, y, w, h)` | Black out a region |

#### Eyes

| Function | Description |
|----------|-------------|
| `Oled_RobotEyes(pupilX, pupilY)` | Two boxy robot eyes. pupilX/Y: -5 to +5 |
| `Oled_RoundEyes(pupilX, pupilY)` | Two round cartoon eyes |
| `Oled_DeadEyes()` | Two X-shaped crash eyes |

#### HUD

| Function | Description |
|----------|-------------|
| `Oled_Joysticks(throttle, yaw, roll, pitch)` | Dual RC joystick display (1000–2000) |
| `Oled_PitchLine(pitch)` | Pitch indicator (-90 to +90 degrees) |
| `Oled_RollLine(roll)` | Roll indicator (-90 to +90 degrees) |

#### Setup

| Function | Description |
|----------|-------------|
| `Oled_Init()` | Enable OLED. Call once in `plutoInit()`. |
| `Oled_SystemMode()` | Return to firmware telemetry. Call in `onLoopFinish()`. |
| `Oled_IsUserMode()` | Returns true if you own the screen |
| `Oled_IsSystemMode()` | Returns true if firmware owns the screen |

### Examples

#### ELRS Channel Monitor

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

#### Robot Eyes That Track RC Sticks

```cpp
void plutoLoop ( void ) {
  Oled_Begin();
  int px = (RcData_Get(RC_YAW) - 1500) / 100;
  int py = -(RcData_Get(RC_PITCH) - 1500) / 100;
  Oled_RobotEyes(px, py);
  Oled_End();
}
```

#### Show Sensor Data

```cpp
void plutoLoop ( void ) {
  Oled_Begin();
  Oled_Text(0, 0, "Pitch:");
  Oled_Number(40, 0, Estimate_Get(Angle, AG_PITCH) / 10);
  Oled_Text(0, 10, "Roll:");
  Oled_Number(36, 10, Estimate_Get(Angle, AG_ROLL) / 10);
  Oled_Text(0, 20, "Yaw:");
  Oled_Number(30, 20, RcData_Get(RC_YAW));
  Oled_End();
}
```

### Advanced API

For users who need full control (custom buffers, erase individual pixels):

```cpp
static uint8_t screen[1024];

void plutoLoop ( void ) {
  memset(screen, 0, 1024);
  Oled_DrawCircle(screen, 64, 32, 20, true);
  Oled_DrawPixel(screen, 50, 50, false);   // erase a pixel
  Oled_Update(screen);
}
```

All `Oled_Draw*` / `Oled_Fill*` functions accept a framebuffer pointer, x/y coordinates, and an `on` flag (true=white, false=black). See [Oled.h](src/main/API/Oled.h) for full documentation with VS Code hover hints.

---

## Documentation

- [OLED API Wiki](docs/OLED_API_WIKI.md) — test snippets for every function, full examples
- [Oled.h](src/main/API/Oled.h) — complete API with doxygen `@param` hints
- [ELRS Integration Report](report/ELRS_Integration_Report.html) — engineering report with challenges, fixes, and architecture

---

## Hardware

| Property | Value |
|----------|-------|
| MCU | STM32F303xC (72 MHz, 40 KB RAM) |
| Display | SSD1306 128x64 monochrome OLED (I2C @ 0x3C) |
| ELRS Port | USART2 (PA3 RX) — 420,000 baud CRSF |
| ESP Port | USART1 (PA9/PA10) — MSP protocol |
| Target | PRIMUS_X2_v1 |

---

## File Structure

```
src/main/rx/crsf.c                      — CRSF protocol driver
src/main/rx/crsf.h                      — CRSF header
src/main/API/RxConfig.h                 — Receiver mode enum (ESP/PPM/CAM/ELRS)
src/main/API-Src/RxConfig.cpp           — Receiver mode configuration
src/main/API/Oled.h                     — OLED Public API (Simple + Advanced)
src/main/API-Src/Oled.cpp               — OLED implementation
src/main/io/oled_display.c              — System telemetry display
src/main/drivers/display_ug2864hsweg01.c — SSD1306 hardware driver
PlutoPilot.cpp                          — User code entry point
docs/OLED_API_WIKI.md                   — Wiki with all test snippets
report/ELRS_Integration_Report.html     — Engineering report
```

---

## Credits

- **Omkar Dandekar** — ELRS/CRSF integration, OLED API design, framebuffer engine
- **Ashish Jaiswal (MechAsh)** — System telemetry display, SSD1306 driver, Serial-IO API
- **Dharna Nar** — Original OledEyes reference design
- **Drona Aviation** — MagisV2 firmware platform
