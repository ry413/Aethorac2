#include <stdio.h>
#include "stm32_tx.h"
#include "esp_log.h"
#include "driver/uart.h"

#define TAG "STM32_TX"

// 构造指令帧
void build_frame(uint8_t cmd_type, uint8_t board_id, uint8_t channel, uint8_t param1, uint8_t param2, uart_frame_t *frame) {
    frame->header = STM32_FRAME_HEADER;           // 帧头
    frame->cmd_type = cmd_type;             // 命令类型
    frame->board_id = board_id;             // 板子 ID
    frame->channel = channel;               // 通道号
    frame->param1 = param1;                 // 参数 1
    frame->param2 = param2;                 // 参数 2
    frame->checksum = calculate_checksum(frame); // 计算校验和
    frame->footer = STM32_FRAME_FOOTER;           // 帧尾
}

// 发送指令帧
void send_frame(uart_frame_t *frame) {
    // 将结构体转换为字节数组
    uint8_t *data = (uint8_t *)frame;
    size_t frame_size = sizeof(uart_frame_t);

    // 发送数据
    uart_write_bytes(UART_NUM, (const char *)data, frame_size);

    // 打印发送的数据
    char hexbuf[frame_size * 3 + 1];
    int pos = 0;
    for (int i = 0; i < frame_size; ++i) {
        pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02X ", data[i]);
    }
    ESP_LOGI(TAG, "发送: %s", hexbuf);
}
