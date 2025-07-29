#pragma once

#include <stdint.h>
#include "stm32_comm_types.h"
#include "lord_manager.h"

// 构造指令帧
void build_frame(uint8_t cmd_type, uint8_t board_id, uint8_t channel, uint8_t param1, uint8_t param2, uart_frame_t *frame);

// 发送指令帧
void send_frame(uart_frame_t *frame);

// 直接发送
inline void sendStm32Cmd(uint8_t cmd_type, uint8_t board_id, uint8_t channel, uint8_t param1, uint8_t param2) {
    uart_frame_t frame;
    build_frame(cmd_type, board_id, channel, param1, param2, &frame);
    send_frame(&frame);
}

// 直接操作继电器
inline void controlRelay(uint8_t channel, uint8_t state) {
    sendStm32Cmd(CMD_RELAY_CONTROL, 0x00, channel, state, 0x00);
    vTaskDelay(pdMS_TO_TICKS(25));
    // 然后直接查询一次, 获得真实物理状态
    sendStm32Cmd(CMD_RELAY_QUERY, 0x00, channel, 0x00, 0x00);
}

// 操作干接点输出
inline void controlDrycontactOut(uint8_t channel, uint8_t state) {
    sendStm32Cmd(CMD_DRYCONTACT_OUT_CONTROL, 0x00, channel, state, 0x00);
}
