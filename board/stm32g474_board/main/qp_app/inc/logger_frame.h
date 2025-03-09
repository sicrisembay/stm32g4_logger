/*
 * logger_frame.h
 *
 *  Created on: Mar 6, 2025
 *      Author: Sicris Rey Embay
 */

#ifndef LOGGER_FRAME_H
#define LOGGER_FRAME_H

#include "stdint.h"
#include "stdbool.h"

#define LEN_FRAME_SOF       (1)
#define LEN_FRAMELEN        (2)
#define LEN_SEQ             (2)
#define LEN_TIMESTAMP       (4)
#define LEN_TYPE_CH         (1)
#define LEN_MSGID           (4)
#define LEN_DLC             (1)
#define LEN_CHECKSUM        (1)

#define MAX_CAN_DATA_LEN    (64)

#define MIN_FRAME_LEN       (LEN_FRAME_SOF + LEN_FRAMELEN + LEN_SEQ + \
                             LEN_TIMESTAMP + LEN_TYPE_CH + LEN_MSGID + \
                             LEN_DLC + LEN_CHECKSUM)
#define MAX_FRAME_LEN       (MIN_FRAME_LEN + MAX_CAN_DATA_LEN)

int32_t FRAME_format_can(
                const uint8_t channel,
                const bool isTx,
                const bool isCanFd,
                uint8_t * const buff,
                const uint16_t bufLen,
                const uint32_t msgId,
                const uint8_t dlc,
                uint8_t const * const pData);


#endif /* LOGGER_FRAME_H */
