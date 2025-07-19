#pragma once

#include <stdint.h>
#include "stm32_comm_types.h"

// 构造指令帧
void build_frame(uint8_t cmd_type, uint8_t board_id, uint8_t channel, uint8_t param1, uint8_t param2, uart_frame_t *frame);

// 发送指令帧
void send_frame(uart_frame_t *frame);
