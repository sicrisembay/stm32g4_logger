/*
 * logger_frame.c
 *
 *  Created on: Mar 6, 2025
 *      Author: Sicris Rey Embay
 */

#include "stddef.h"
#include "../inc/logger_frame.h"
#include "board_api.h"

#define FRAME_SOF           (0xFF)

#define FRAME_TYPE_CAN_CC_RX    (0)
#define FRAME_TYPE_CAN_CC_TX    (1)
#define FRAME_TYPE_CAN_FD_RX    (2)
#define FRAME_TYPE_CAN_FD_TX    (3)

typedef union {
    uint8_t all;
    struct {
        uint8_t type:5;
        uint8_t channel:3;
    } bit;
} frame_type_t;

static uint16_t seq = 0;

int32_t FRAME_format_can(
                const uint8_t channel,
                const bool isTx,
                const bool isCanFd,
                uint8_t * const buff,
                const uint16_t bufLen,
                const uint32_t msgId,
                const uint8_t dlc,
                uint8_t const * const pData)
{
    if(buff == NULL) {
        return -1;
    }

    if(channel > 2) {
        return -1;
    }

    if((dlc > 0) && (pData == NULL)) {
        return -1;
    }

    if(dlc > 64) {
        return -1;
    }

    const uint16_t frame_len = dlc + MIN_FRAME_LEN;
    const uint32_t timestamp = HighResTimer_get_tick();

    if(bufLen < frame_len) {
        /* frame does not fit into buff */
        return -1;
    }

    buff[0] = FRAME_SOF;

    buff[1] = frame_len & 0x00FF;
    buff[2] = (frame_len >> 8) & 0x00FF;

    buff[3] = seq & 0x00FF;
    buff[4] = (seq >> 8) & 0x00FF;
    seq++;

    buff[5] = timestamp & 0x00FF;
    buff[6] = (timestamp >> 8) & 0x00FF;
    buff[7] = (timestamp >> 16) & 0x00FF;
    buff[8] = (timestamp >> 24) & 0x00FF;

    frame_type_t frameType;
    if(isCanFd) {
        if(isTx) {
            frameType.bit.type = FRAME_TYPE_CAN_FD_TX;
        } else {
            frameType.bit.type = FRAME_TYPE_CAN_FD_RX;
        }
    } else {
        if(isTx) {
            frameType.bit.type = FRAME_TYPE_CAN_CC_TX;
        } else {
            frameType.bit.type = FRAME_TYPE_CAN_CC_RX;
        }
    }
    frameType.bit.channel = channel;
    buff[9] = frameType.all;

    buff[10] = msgId & 0x00FF;
    buff[11] = (msgId >> 8) & 0x00FF;
    buff[12] = (msgId >> 16) & 0x00FF;
    buff[13] = (msgId >> 24) & 0x00FF;

    buff[14] = dlc;
    memcpy(&buff[15], pData, dlc);

    int32_t sum = 0;
    for(int i = 0; i < (frame_len - 1); i++) {
        sum += buff[i];
    }
    buff[frame_len - 1] = (uint8_t)((-sum) & 0x00FF);

    return frame_len;
}
