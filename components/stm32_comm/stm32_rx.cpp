#include "esp_log.h"
#include "driver/uart.h"
#include "stdint.h"

#include "stm32_comm_types.h"
#include "stm32_rx.h"
#include "stm32_tx.h"
#include "rs485_comm.h"
// #include "board_config.h"
// #include "manager_base.h"
#include "room_state.h"
#include "lord_manager.h"

#define TAG "STM32_RX"

// 打印数据包的内容
static void print_response(const uart_frame_t *frame) {
    ESP_LOGI(TAG, "收到: %02X %02X %02X %02X %02X %02X %02X %02X",
            frame->header, frame->cmd_type, frame->board_id, frame->channel, frame->param1, frame->param2, frame->checksum, frame->footer);
}

void handle_response(uart_frame_t *frame) {
    print_response(frame);

    switch (frame->cmd_type) {
        case 0x02: // 继电器响应
            ESP_LOGI(TAG, "继电器查询响应: 板%d 通道%d 当前状态 %d", frame->board_id, frame->channel, frame->param1);
            LordManager::instance().updateChannelState(frame->board_id, frame->channel, frame->param1);
            break;
        case 0x04: // 调光响应
            ESP_LOGI(TAG, "调光响应：板%d 通道%d 当前亮度 %d", frame->board_id, frame->channel, frame->param1);
            break;
        case 0x07: { // 干接点输入上报
            uint8_t board_id = frame->board_id;
            uint8_t channel_num = frame->channel;
            uint8_t state = frame->param1;

            LordManager::instance().executeInputAction(board_id, channel_num, state);

            // // 测试模式拦截输入
            // if (is_test_mode()) {
            //     uart_frame_t frame;
            //     build_frame(0x01, board_id, channel_num, state, 0x00, &frame);
            //     send_frame(&frame);
            //     return;
            // }



            // if (!get_alive()) {
            //     uint8_t alive_channel = LordManager::getInstance().getAliveChannel();
            //     uint8_t door_channel = LordManager::getInstance().getDoorChannel();
            //     if (channel_num != alive_channel && channel_num != door_channel) {
            //         ESP_LOGI(TAG, "输入[%d] 非alive_channel[%d], 非door_channel[%d], 拒绝响应", channel_num, alive_channel, door_channel);
            //         return;
            //     }
            // }
            
            // // 遍历所有指定通道的BoardInput, 也就是说, 有可能有多个channel不同的BoardInput, 这里要执行所有同为指定通道的输出
            // auto& all_boards = BoardManager::getInstance().getAllItems();
            // for (const auto& [board_id, board] : all_boards) {
            //     auto& inputs = board->inputs;
            //     for (auto& input : inputs) {
            //         // 找到目标通道
            //         if (input.channel == channel_num) {
            //             // 判断此输入类型
            //             if (input.type != InputType::INFRARED) {
            //                 // 如果不是红外的话, 就执行指定电平的行为
            //                 InputType input_level;
            //                 if (state == 0x01) {
            //                     input_level = InputType::HIGH;
            //                 } else {
            //                     input_level = InputType::LOW;
            //                 }
            //                 if (input.type == input_level) {
            //                     input.execute();
            //                 }
            //             }
            //             // 否则当然是进入红外逻辑了
            //             else {
            //                 input.execute_infrared(state);
            //             }
            //         }
            //     }
            // }
            break;
        }
        case 0x09: { // 干接点输入查询响应
            ESP_LOGI(TAG, "干接点输入查询响应: 板%d 通道%d 状态 %s", frame->board_id, frame->channel, frame->param1 ? "开" : "关");
            // if (frame->channel == LordManager::instance().getAliveChannel()) {
            //     if (frame->param1) {
            //         printf("轮询检测到处于插卡状态, 设置为插卡状态\n");
            //         ESP_LOGI(TAG, "轮询检测到处于插卡状态, 设置为插卡状态");
            //         set_alive(true);
            //         add_state("入住");

            //     } else if (frame->param1 == 0x00) {
            //         printf("轮询检测到处于拔卡状态, 再次进行拔卡动作组\n");
            //         ESP_LOGI(TAG, "轮询检测到处于拔卡状态, 再次进行拔卡动作组");
            //         uart_frame_t remove_card_cmd;
            //         build_frame(0x07, frame->board_id, frame->channel, frame->param1, 0x00, &remove_card_cmd);
            //         handle_response(&remove_card_cmd);
            //     }
            // }
            break;
        }
        case 0xFF: { // 查询版本号的响应
            uint8_t firmware_ver_1 = frame->board_id;
            uint8_t firmware_ver_2 = frame->channel;
            uint8_t board_ver_1 = frame->param1;
            uint8_t board_ver_2 = frame->param2;
            ESP_LOGI(TAG, "固件版本号: %x.%x 板子型号: %x.%x", firmware_ver_1, firmware_ver_2, board_ver_1, board_ver_2);
            break;
        }
        default:
            ESP_LOGE(TAG, "未知响应命令类型: 0x%02X", frame->cmd_type);
            break;
    }
}

// 接收任务
void stm32_receive_task(void *pvParameters) {
    uint8_t byte;
    enum {
        WAIT_FOR_HEADER,
        RECEIVE_DATA,
    } state = WAIT_FOR_HEADER;

    uart_frame_t frame;
    uint8_t *frame_ptr = (uint8_t *)&frame;
    int byte_index = 0;
    size_t frame_size = sizeof(uart_frame_t);

    ESP_LOGI(TAG, "已创建stm32接收任务");
    while (1) {
        // 读取单个字节
        int len = uart_read_bytes(UART_NUM, &byte, 1, portMAX_DELAY);
        if (len > 0) {
            switch (state) {
                case WAIT_FOR_HEADER:
                    if (byte == STM32_FRAME_HEADER) {
                        state = RECEIVE_DATA;
                        byte_index = 0;
                        frame_ptr[byte_index++] = byte;
                    }
                    break;

                case RECEIVE_DATA:
                    frame_ptr[byte_index++] = byte;
                    if (byte_index == frame_size) {
                        // 接收到完整的数据包
                        if (frame.footer == STM32_FRAME_FOOTER && frame.checksum == calculate_checksum(&frame)) {
                            // 处理数据包
                            handle_response(&frame);
                        } else {
                            ESP_LOGE(TAG, "数据包错误");
                        }
                        state = WAIT_FOR_HEADER; // 无论成功或失败，都重新等待下一帧
                    }
                    break;
            }
        } else if (len < 0) {
            ESP_LOGE(TAG, "UART 读取错误: %d", len);
        }
    }
}

void uart_init_stm32() {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    // 配置串口参数
    uart_param_config(UART_NUM, &uart_config);
    // 设置串口引脚
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // 安装驱动程序
    uart_driver_install(UART_NUM, 2048, 0, 0, NULL, 0);

    ESP_LOGI(TAG, "UART[%d] initialized", UART_NUM);
    xTaskCreate(stm32_receive_task, "stm32_receive_task", 4096, nullptr, 3, nullptr);
}