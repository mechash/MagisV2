/*******************************************************************************
 #  Copyright (c) 2025 Drona Aviatino                                          #
 #  All rights reserved.                                                       #
 #  -------------------------------------------------------------------------  #
 #  Author: Ashish Jaiswal (MechAsh) <AJ>                                      #
 #  Project: MagisV2                                                           #
 #  File: \ssd1306.h                                                           #
 #  Created Date: Sat, 25th Jan 2025                                           #
 #  Brief:                                                                     #
 #  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  #
 #  Last Modified: Tue, 5th May 2026                                           #
 #  Modified By: AJ                                                            #
 #  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  #
 #  HISTORY:                                                                   #
 #  Date      	By	Comments                                                   #
 #  ----------	---	---------------------------------------------------------  #
 *******************************************************************************/
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * GLOBAL FLAGS & DATA
 * ============================================================================ */

extern bool devmode;
extern bool OledEnable;

typedef enum {
  OLED_MODE_SYSTEM = 0,
  OLED_MODE_USER
} oled_mode_e;

extern oled_mode_e oledMode;

/* ============================================================================
 * STARTUP & SYSTEM RENDERING
 * ============================================================================ */

void OledStartUpInit ( void );
void OledStartUpPage ( void );
void OledDisplaySystemData ( void );
void OledDisplayNav ( void );
void OledDisplayData ( void );

/* ============================================================================
 * MODE & OWNERSHIP CONTROL
 * ============================================================================ */

void Oled_SystemMode ( void );
void Oled_UserMode ( void );

bool Oled_IsUserMode ( void );
bool Oled_IsSystemMode ( void );

/* ============================================================================
 * HARDWARE / BUFFER CONTROL
 * ============================================================================ */

void Oled_display_Clear ( void );
void Oled_display_Update ( uint8_t *buf );

/* ============================================================================
 * BASIC DRAWING PRIMITIVES
 * ============================================================================ */

void Oled_DrawPixel ( uint8_t *screen, uint8_t x, uint8_t y, bool on );
void Oled_DrawHLine ( uint8_t *screen, uint8_t x, uint8_t y, uint8_t length, bool on );
void Oled_DrawVLine ( uint8_t *screen, uint8_t x, uint8_t y, uint8_t length, bool on );
void Oled_DrawLine ( uint8_t *screen, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on );

void Oled_DrawText ( uint8_t *screen, uint8_t x, uint8_t y, const char *text );
void Oled_DrawNumber ( uint8_t *screen, uint8_t x, uint8_t y, int16_t number );

/* ============================================================================
 * SHAPES (OUTLINE & FILLED)
 * ============================================================================ */

void Oled_DrawRect ( uint8_t *screen, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on );
void Oled_FillRect ( uint8_t *screen, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on );

void Oled_DrawRoundedRect ( uint8_t *screen, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t radius, bool on );
void Oled_FillRoundedRect ( uint8_t *screen, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t radius, bool on );

void Oled_DrawCircle ( uint8_t *screen, uint8_t cx, uint8_t cy, uint8_t radius, bool on );
void Oled_FillCircle ( uint8_t *screen, uint8_t cx, uint8_t cy, uint8_t radius, bool on );

/* ============================================================================
 * HUD & SPECIALIZED GRAPHICS
 * ============================================================================ */

void Oled_DrawRCJoysticks ( uint8_t *screen, uint16_t throttle, uint16_t yaw, uint16_t roll, uint16_t pitch );
void Oled_DrawPitchIndicator ( uint8_t *buf, int16_t pitch );
void Oled_DrawRollIndicator ( uint8_t *buf, int16_t roll );

typedef enum {
  OLED_ARROW_UP,
  OLED_ARROW_DOWN,
  OLED_ARROW_LEFT,
  OLED_ARROW_RIGHT
} oled_arrow_dir_t;

void Oled_DrawArrow ( uint8_t *screen, uint8_t cx, uint8_t cy, uint8_t size, oled_arrow_dir_t dir, bool on );

/* ============================================================================
 * EYE GRAPHICS PRESETS
 * ============================================================================ */

void Oled_DrawEye ( uint8_t *buf, uint8_t cx, uint8_t cy, uint8_t radius, int8_t pupilOffsetX, int8_t pupilOffsetY, bool filled );
void Oled_DrawXEye ( uint8_t *screen, uint8_t cx, uint8_t cy, uint8_t size );

void Oled_DrawBoxyEye ( uint8_t *buf, uint8_t cx, uint8_t cy, uint8_t w, uint8_t h, uint8_t r, int8_t pupilOffsetX, int8_t pupilOffsetY, bool filled );
void Oled_DrawBoxyXEye ( uint8_t *screen, uint8_t cx, uint8_t cy, uint8_t w, uint8_t h, uint8_t r );

#ifdef __cplusplus
}
#endif
