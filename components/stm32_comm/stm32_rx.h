#pragma once
#include "stm32_comm_types.h"

// 初始化stm32串口
void uart_init_stm32();
void handle_response(uart_frame_t *frame);  // 暴露给测试模式用一下