#pragma once

#include <stdint.h>

#define UART_NUM 2
#define UART_TX_PIN 14
#define UART_RX_PIN 12
#define UART_BAUD_RATE 115200

#define STM32_FRAME_HEADER 0x79
#define STM32_FRAME_FOOTER 0x7C

#define CMD_RELAY_CONTROL 0x01      // esp=>stm 控制继电器
#define CMD_RELAY_QUERY 0x02        // esp<=>stm 查询与响应继电器物理状态都用这个

#define CMD_DRYCONTACT_OUT_CONTROL 0x05 // esp=>stm 控制干接点输出

#define CMD_DRYCONTACT_INPUT 0x07   // esp<=stm 干接点输入被触发
#define CMD_DRYCONTACT_INPUT_QUERY 0x08// esp=>stm 查询干接点**输入**的物理状态. 与继电器输出不同, 它们是两种命令
#define CMD_DRYCONTACT_INPUT_RESPONSE 0x09// esp<=stm esp发某指令查询干接点输入状态后, stm的响应

#define CMD_VERSION_RESPONSE 0xFF   // esp<=stm 查询版本号后的响应

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

extern bool global_STM32_log_enable_flag;

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