#include "esp_log.h"
#include "board_output.h"
#include "stm32_comm_types.h"
#include "stm32_tx.h"

#define TAG "BOARD_OUTPUT"

void BoardOutput::connect() {
    // if (current_state == State::CONNECTED) {
    //     ESP_LOGI(TAG, "板%d 通道%d 本就闭合, return", host_board_id, channel);
    //     return;
    // }

    uart_frame_t frame;
    uint8_t cmd_type = 0x01;
    if (type == OutputType::RELAY) {
        cmd_type = 0x01;
    } else if (type == OutputType::DRY_CONTACT) {
        cmd_type = 0x05;
    }
    uint8_t param1 = 0x01;

    build_frame(cmd_type, host_board_id, channel, param1, 0x00, &frame);
    send_frame(&frame);
    current_state = State::CONNECTED;
    ESP_LOGI(TAG, "板%d 通道%d 已闭合\n", host_board_id, channel);
}

void BoardOutput::disconnect() {
    // if (current_state == State::DISCONNECTED) {
    //     ESP_LOGI(TAG, "板%d 通道%d 本就断开, return", host_board_id, channel);
    //     return;
    // }
    
    uart_frame_t frame;
    uint8_t cmd_type = 0x01;
    if (type == OutputType::RELAY) {
        cmd_type = 0x01;
    } else if (type == OutputType::DRY_CONTACT) {
        cmd_type = 0x05;
    }
    uint8_t param1 = 0x00;

    build_frame(cmd_type, host_board_id, channel, param1, 0x00, &frame);
    send_frame(&frame);
    current_state = State::DISCONNECTED;
    ESP_LOGI(TAG, "板%d 通道%d 已断开\n", host_board_id, channel);
}