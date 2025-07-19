#pragma once

#include <stdint.h>

#define UART_NUM 2
#define UART_TX_PIN 14
#define UART_RX_PIN 12
#define UART_BAUD_RATE 115200

#define STM32_FRAME_HEADER 0x79
#define STM32_FRAME_FOOTER 0x7C

typedef struct {
    uint8_t header;        // 帧头
    uint8_t cmd_type;      // 命令类型
    uint8_t board_id;      // 板子 ID
    uint8_t channel;       // 通道号
    uint8_t param1;        // 参数 1
    uint8_t param2;        // 参数 2
    uint8_t checksum;      // 校验和
    uint8_t footer;        // 帧尾
} uart_frame_t;

inline uint8_t calculate_checksum(uart_frame_t *frame) {
    uint8_t checksum = 0;
    checksum += frame->header;
    checksum += frame->cmd_type;
    checksum += frame->board_id;
    checksum += frame->channel;
    checksum += frame->param1;
    checksum += frame->param2;

    return checksum & 0xFF;
}