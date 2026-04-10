/*
 * This file is part of Cleanflight and Magis.
 *
 * Cleanflight and Magis are free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight and Magis are distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * CRSF protocol implementation for ExpressLRS (ELRS) receivers.
 *
 * CRSF frame format:
 *   [device_addr] [frame_length] [type] [payload...] [crc8]
 *
 * RC Channels Packed frame (type 0x16):
 *   16 channels, 11 bits each, packed into 22 bytes payload.
 *   Channel range: 172..1811 maps to 988us..2012us.
 *
 * Link Statistics frame (type 0x14):
 *   Contains RSSI, SNR, link quality, TX power, etc.
 *
 * Battery Sensor telemetry (type 0x08):
 *   Sent from FC to ELRS RX after each RC frame, in the inter-frame gap.
 *
 * Serial: 420000 baud, 8N1, non-inverted.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "platform.h"
#include "build_config.h"

#include "drivers/system.h"
#include "drivers/serial.h"
#include "io/serial.h"

#include "rx/rx.h"
#include "rx/crsf.h"

#define CRSF_BAUDRATE           420000

#define CRSF_SYNC_BYTE          0xC8
#define CRSF_MAX_FRAME_SIZE     64
#define CRSF_MAX_CHANNEL        16

#define CRSF_FRAMETYPE_RC_CHANNELS_PACKED   0x16
#define CRSF_FRAMETYPE_LINK_STATISTICS      0x14
#define CRSF_FRAMETYPE_BATTERY_SENSOR       0x08

/* CRSF channel value range */
#define CRSF_CHANNEL_VALUE_MIN  172
#define CRSF_CHANNEL_VALUE_MID  992
#define CRSF_CHANNEL_VALUE_MAX  1811

/* Time between frames — used for frame sync timeout (in microseconds) */
#define CRSF_TIME_NEEDED_PER_FRAME  1100
#define CRSF_TIME_BETWEEN_FRAMES    4000

/* CRC8 lookup table using poly 0xD5 (Castagnoli) as used by CRSF */
static uint8_t crc8Tab[256];
static bool crcTableReady = false;

static void crsfGenerateCrc8Table(void)
{
    for (uint16_t i = 0; i < 256; i++) {
        uint8_t crc = (uint8_t)i;
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x80) ? 0xD5 : 0);
        }
        crc8Tab[i] = crc;
    }
    crcTableReady = true;
}

static uint8_t crsfCrc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc = crc8Tab[crc ^ data[i]];
    }
    return crc;
}

/* Frame buffer */
static uint8_t crsfFrame[CRSF_MAX_FRAME_SIZE];
static bool crsfFrameDone = false;

/* Serial port handle (shared RX + telemetry TX) */
static serialPort_t *crsfPort = NULL;

/* Battery telemetry voltage to send (set externally via crsfSetBatteryVoltage) */
static uint16_t crsfBatteryVoltage = 0;
static bool crsfTelemetryEnabled = false;

/* Decoded channel data */
static uint16_t crsfChannelData[CRSF_MAX_CHANNEL];

/* Link statistics */
static int8_t crsfLinkRssi = 0;
static uint8_t crsfLinkQuality = 0;
static int8_t crsfLinkSnr = 0;

/* Serial receive ISR callback */
static void crsfDataReceive(uint16_t c)
{
    static uint8_t crsfFramePosition = 0;
    static uint32_t crsfFrameStartAt = 0;
    uint32_t now = micros();

    int32_t frameTime = now - crsfFrameStartAt;

    /* If too much time elapsed since last byte, start a new frame */
    if (frameTime > (int32_t)CRSF_TIME_BETWEEN_FRAMES) {
        crsfFramePosition = 0;
    }

    if (crsfFramePosition == 0) {
        /* First byte: sync/device address */
        crsfFrameStartAt = now;
    }

    if (crsfFramePosition < CRSF_MAX_FRAME_SIZE) {
        crsfFrame[crsfFramePosition] = (uint8_t)c;
    }

    crsfFramePosition++;

    /* Check if we have enough data: addr(1) + len(1) + payload(len) */
    if (crsfFramePosition >= 3) {
        uint8_t frameLength = crsfFrame[1]; /* length includes type + payload + crc */
        uint8_t fullFrameLength = frameLength + 2; /* +2 for addr and len bytes */

        if (crsfFramePosition >= fullFrameLength) {
            crsfFrameDone = true;
            crsfFramePosition = 0;
        }
    }

    /* Safety: prevent buffer overflow */
    if (crsfFramePosition >= CRSF_MAX_FRAME_SIZE) {
        crsfFramePosition = 0;
    }
}

/*
 * Unpack 16 x 11-bit channels from 22 bytes of payload.
 * Channels are packed LSB first.
 */
static void crsfUnpackChannels(const uint8_t *payload)
{
    crsfChannelData[0]  = ((uint16_t)payload[0]       | ((uint16_t)payload[1]  << 8)) & 0x07FF;
    crsfChannelData[1]  = ((uint16_t)payload[1]  >> 3 | ((uint16_t)payload[2]  << 5)) & 0x07FF;
    crsfChannelData[2]  = ((uint16_t)payload[2]  >> 6 | ((uint16_t)payload[3]  << 2) | ((uint16_t)payload[4]  << 10)) & 0x07FF;
    crsfChannelData[3]  = ((uint16_t)payload[4]  >> 1 | ((uint16_t)payload[5]  << 7)) & 0x07FF;
    crsfChannelData[4]  = ((uint16_t)payload[5]  >> 4 | ((uint16_t)payload[6]  << 4)) & 0x07FF;
    crsfChannelData[5]  = ((uint16_t)payload[6]  >> 7 | ((uint16_t)payload[7]  << 1) | ((uint16_t)payload[8]  << 9)) & 0x07FF;
    crsfChannelData[6]  = ((uint16_t)payload[8]  >> 2 | ((uint16_t)payload[9]  << 6)) & 0x07FF;
    crsfChannelData[7]  = ((uint16_t)payload[9]  >> 5 | ((uint16_t)payload[10] << 3)) & 0x07FF;
    crsfChannelData[8]  = ((uint16_t)payload[11]      | ((uint16_t)payload[12] << 8)) & 0x07FF;
    crsfChannelData[9]  = ((uint16_t)payload[12] >> 3 | ((uint16_t)payload[13] << 5)) & 0x07FF;
    crsfChannelData[10] = ((uint16_t)payload[13] >> 6 | ((uint16_t)payload[14] << 2) | ((uint16_t)payload[15] << 10)) & 0x07FF;
    crsfChannelData[11] = ((uint16_t)payload[15] >> 1 | ((uint16_t)payload[16] << 7)) & 0x07FF;
    crsfChannelData[12] = ((uint16_t)payload[16] >> 4 | ((uint16_t)payload[17] << 4)) & 0x07FF;
    crsfChannelData[13] = ((uint16_t)payload[17] >> 7 | ((uint16_t)payload[18] << 1) | ((uint16_t)payload[19] << 9)) & 0x07FF;
    crsfChannelData[14] = ((uint16_t)payload[19] >> 2 | ((uint16_t)payload[20] << 6)) & 0x07FF;
    crsfChannelData[15] = ((uint16_t)payload[20] >> 5 | ((uint16_t)payload[21] << 3)) & 0x07FF;
}

/*
 * Send battery telemetry frame immediately (called only after a complete RC frame).
 * This is safe because the ELRS RX expects telemetry in the gap after sending an RC frame.
 */
static void crsfSendBatteryFrame(void)
{
    if (!crsfPort || !crsfTelemetryEnabled) {
        return;
    }

    uint16_t voltage = crsfBatteryVoltage;

    uint8_t frame[12];
    frame[0] = CRSF_SYNC_BYTE;                        /* sync byte 0xC8 */
    frame[1] = 10;                                     /* frame length: type(1) + payload(8) + crc(1) */
    frame[2] = CRSF_FRAMETYPE_BATTERY_SENSOR;          /* type 0x08 */
    frame[3] = (voltage >> 8) & 0xFF;                  /* voltage high byte (deci-volts, big-endian) */
    frame[4] = voltage & 0xFF;                         /* voltage low byte */
    frame[5] = 0;                                      /* current high */
    frame[6] = 0;                                      /* current low */
    frame[7] = 0;                                      /* capacity [23:16] */
    frame[8] = 0;                                      /* capacity [15:8] */
    frame[9] = 0;                                      /* capacity [7:0] */
    frame[10] = 0;                                     /* battery remaining % */
    frame[11] = crsfCrc8(&frame[2], 9);                /* CRC over type + payload */

    for (uint8_t i = 0; i < sizeof(frame); i++) {
        serialWrite(crsfPort, frame[i]);
    }
}

uint8_t crsfFrameStatus(void)
{
    if (!crsfFrameDone) {
        return SERIAL_RX_FRAME_PENDING;
    }
    crsfFrameDone = false;

    uint8_t frameLength = crsfFrame[1];

    /* Sanity check frame length */
    if (frameLength < 2 || frameLength > CRSF_MAX_FRAME_SIZE - 2) {
        return SERIAL_RX_FRAME_PENDING;
    }

    /* CRC check: CRC covers type + payload (everything between len byte and CRC byte) */
    uint8_t crcLen = frameLength - 1; /* type + payload, excluding the CRC byte itself */
    uint8_t crc = crsfCrc8(&crsfFrame[2], (uint8_t)crcLen);
    uint8_t frameCrc = crsfFrame[frameLength + 1]; /* CRC is last byte: addr(1) + len(1) + type+payload(len-1) + crc(1) */

    if (crc != frameCrc) {
        return SERIAL_RX_FRAME_PENDING;
    }

    uint8_t frameType = crsfFrame[2];

    switch (frameType) {
        case CRSF_FRAMETYPE_RC_CHANNELS_PACKED:
            /* Payload starts at byte 3, 22 bytes of channel data */
            crsfUnpackChannels(&crsfFrame[3]);

            /* Send battery telemetry in the gap right after RC frame */
            crsfSendBatteryFrame();

            return SERIAL_RX_FRAME_COMPLETE;

        case CRSF_FRAMETYPE_LINK_STATISTICS:
            /* Parse link statistics for RSSI/LQ reporting */
            crsfLinkRssi = -(int8_t)crsfFrame[3];       /* Uplink RSSI Ant. 1 (dBm, negated) */
            crsfLinkSnr = (int8_t)crsfFrame[6];          /* Uplink SNR (dB) */
            crsfLinkQuality = crsfFrame[7];              /* Uplink Link Quality (%) */
            (void)crsfLinkRssi;
            (void)crsfLinkSnr;
            (void)crsfLinkQuality;
            break;

        default:
            break;
    }

    return SERIAL_RX_FRAME_PENDING;
}

/*
 * Convert CRSF channel value (172..1811) to standard RC value (988..2012).
 * Using linear mapping: rcValue = (crsfValue - 172) * 1024 / 1639 + 988
 * Simplified: rcValue = crsfValue * 0.62477f + 880.53f
 * We use the same approach as SBUS for simplicity.
 */
static uint16_t crsfReadRawRC(rxRuntimeConfig_t *rxRuntimeConfig, uint8_t chan)
{
    UNUSED(rxRuntimeConfig);

    if (chan >= CRSF_MAX_CHANNEL) {
        return 1500;
    }

    /* Map CRSF range [172..1811] to RC range [988..2012] */
    return (uint16_t)((float)crsfChannelData[chan] * 0.62477120f + 880.53f);
}

bool crsfInit(rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback)
{
    UNUSED(rxConfig);

    if (!crcTableReady) {
        crsfGenerateCrc8Table();
    }

    for (uint8_t i = 0; i < CRSF_MAX_CHANNEL; i++) {
        crsfChannelData[i] = CRSF_CHANNEL_VALUE_MID;
    }

    if (callback) {
        *callback = crsfReadRawRC;
    }

    rxRuntimeConfig->channelCount = CRSF_MAX_CHANNEL;

    serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RX_SERIAL);
    if (!portConfig) {
        return false;
    }

    crsfPort = openSerialPort(
        portConfig->identifier,
        FUNCTION_RX_SERIAL,
        crsfDataReceive,
        CRSF_BAUDRATE,
        MODE_RXTX,
        (portOptions_t)(SERIAL_NOT_INVERTED | SERIAL_STOPBITS_1 | SERIAL_PARITY_NO)
    );

    return crsfPort != NULL;
}

/*
 * Update the battery voltage for CRSF telemetry.
 * Called from main loop. The actual TX happens inside crsfFrameStatus()
 * right after an RC frame is received (safe timing).
 */
void crsfSetBatteryVoltage(uint16_t vBatRaw)
{
    crsfBatteryVoltage = vBatRaw;
    crsfTelemetryEnabled = true;
}
