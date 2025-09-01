#include "esp_log.h"
#include "driver/uart.h"
#include "stdint.h"

#include "stm32_comm_types.h"
#include "stm32_rx.h"
#include "rs485_comm.h"
#include "room_state.h"
#include "lord_manager.h"
#include "stm32_tx.h"
#include <curtain.h>

#define TAG "STM32_RX"

bool global_STM32_log_enable_flag = false;

// 打印数据包的内容
static void print_response(const uart_frame_t *frame) {
    ESP_LOGI(TAG, "收到: %02X %02X %02X %02X %02X %02X %02X %02X",
            frame->header, frame->cmd_type, frame->board_id, frame->channel, frame->param1, frame->param2, frame->checksum, frame->footer);
}

void handle_response(uart_frame_t *frame) {
    if (global_STM32_log_enable_flag) {
        print_response(frame);
    }

    switch (frame->cmd_type) {
        case CMD_RELAY_QUERY: // 继电器响应
            LordManager::instance().updateRelayPhysicsState(frame->channel, frame->param1);
            break;
        case 0x04: // 调光响应
            ESP_LOGI(TAG, "调光响应：通道%d 当前亮度 %d", frame->channel, frame->param1);
            break;
        case CMD_DRYCONTACT_INPUT: { // 干接点输入上报
            uint8_t channel_num = frame->channel;
            uint8_t state = frame->param1;
            
            // 测试模式拦截输入
            if (is_test_mode()) {
                uart_frame_t frame;
                build_frame(0x01, 0x00, channel_num, state, 0x00, &frame);
                send_frame(&frame);
                return;
            }

            static auto& lord = LordManager::instance();
            // 因为配置上可以为一个输入通道配置无限个配置行, 所以一个通道号会对应多个ChannelInput实例
            auto channel_input_ptrs = lord.getAllChannelInputByChannelNum(channel_num);
            if (channel_input_ptrs.empty()) {
                ESP_LOGW(TAG, "未配置输入通道[%u]", channel_num);
                return;
            }

            // 检测此通道是否能在拔卡时使用
            std::set<InputTag> tags = channel_input_ptrs.front()->getTags();
            if (!lord.getAlive() && !tags.contains(InputTag::REMOVE_CARD_USABLE)) {
                ESP_LOGI(TAG, "输入[%d], 在拔卡时拒绝响应", channel_num);
                return;
            }

            // 是门磁的话就更新门状态
            if (tags.contains(InputTag::IS_DOOR_CHANNEL)) {
                if (state) {
                    lord.onDoorClosed();
                } else {
                    lord.onDoorOpened();
                }

                // 如果插拔卡通道是红外类型, 门磁动作就会当作是检测到了一下
                auto* alive_channel = lord.getAliveChannel();
                if (alive_channel && (alive_channel->trigger_type == TriggerType::INFRARED ||
                    alive_channel->trigger_type == TriggerType::INFRARED_TIMEOUT)) {
                        vTaskDelay(pdMS_TO_TICKS(50));
                        uart_frame_t wakeup_infrared_cmd;
                        build_frame(0x07, 0x00, alive_channel->channel, 0x00, 0x00, &wakeup_infrared_cmd);
                        handle_response(&wakeup_infrared_cmd);
                }
            }
            // 是门铃的话要判断处不处于勿扰状态
            else if (tags.contains(InputTag::IS_DOORBELL_CHANNEL)) {
                if (exist_state("勿扰")) {
                    return;
                }
            }

            // 执行它
            for (auto* channel_input_ptr : channel_input_ptrs) {
                if (channel_input_ptr->trigger_type == TriggerType::LOW_LEVEL ||
                    channel_input_ptr->trigger_type == TriggerType::HIGH_LEVEL) {
                    TriggerType input_type;
                    if (state == 0x01) {
                        input_type = TriggerType::HIGH_LEVEL;
                    } else {
                        input_type = TriggerType::LOW_LEVEL;
                    }
                    if (channel_input_ptr->trigger_type == input_type) {
                        channel_input_ptr->execute();
                    }
                } else if (channel_input_ptr->trigger_type == TriggerType::INFRARED) {
                    bool ignore = false;
                    for (auto* curtain : lord.getDevicesByType<Curtain>()) {
                        auto curtainState = curtain->getState();
                        if (curtainState == CurtainState::OPENING || curtainState == CurtainState::CLOSING) {
                            ignore = true;
                            break;
                        }
                    }
                    if (ignore) {
                        ESP_LOGI(TAG, "窗帘正在动作, 忽略红外");
                    } else {
                        channel_input_ptr->execute_infrared(state);
                    }
                }
            }
            break;
        }
        case CMD_DRYCONTACT_INPUT_RESPONSE: { // 干接点输入查询响应
            LordManager::instance().updateDrycontactInputPhysicsState(frame->channel, frame->param1);
            break;
        }
        case CMD_VERSION_RESPONSE: { // 查询版本号的响应
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