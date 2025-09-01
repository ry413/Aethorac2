#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "esp_timer.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "rs485_comm.h"
#include "lord_manager.h"
#include "commons.h"
#include "network.h"
#include "air_conditioner.h"
#include "stm32_tx.h"
#include "stm32_rx.h"
#include "identity.h"

#define TAG "RS485"

bool global_RS485_log_enable_flag = false;

// 测试模式
static bool test_mode = false;
static TaskHandle_t oracle_task_handle = NULL;  // 在测试模式下, 周期上报IP的任务
void periodic_oracle_task(void *arg);
// 接收wifissid和pass分包的一些东西
// SSID状态
static bool recv_ssid_ing = false;
static char wifi_ssid[64] = {0};
static uint8_t ssid_offset = 0;
static int64_t ssid_recv_start_time = 0;
static uint8_t expected_ssid_packet = 0;

// PASS状态
static bool recv_pass_ing = false;
static char wifi_pass[64] = {0};
static uint8_t pass_offset = 0;
static int64_t pass_recv_start_time = 0;
static uint8_t expected_pass_packet = 0;

static QueueHandle_t rs485Queue = nullptr;

void uart_init_rs485() {
    uart_config_t uart_config = {
        .baud_rate = RS485_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(RS485_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_PORT, RS485_TX_PIN, RS485_RX_PIN, RS485_DE_GPIO_NUM, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART_PORT, RS485_BUFFER_SIZE, RS485_BUFFER_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    ESP_LOGI(TAG, "UART[%d] Initialized", RS485_UART_PORT);

    // 发送队列
    rs485Queue = xQueueCreate(RS485_QUEUE_LEN, sizeof(rs485_bus_cmd));
    if (rs485Queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create RS485 command queue");
        return;
    }

    // 心跳任务
    xTaskCreate([] (void* param) {
        while (true) {
            std::array<uint8_t, 8> code = LordManager::instance().getHeartbeatCode();
            sendRS485CMD(std::vector<uint8_t>(code.begin(), code.end()));
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }, "495ALIVE task", 2048, NULL, 3, NULL);

    // 空调查询任务
    xTaskCreate([](void* param) {
        auto& ids = AirConGlobalConfig::getInstance().air_ids;

        auto it = ids.begin();
        while (true) {
            // 若集合为空, 只是等待后重试, 这样可以支持热修改配置时加的空调
            if (ids.empty()) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                it = ids.begin();
                continue;
            }

            // 迭代器跑到末尾就回到开头, 实现循环
            if (it == ids.end()) {
                it = ids.begin();
            }

            // 再检查一次
            if (it != ids.end()) {
                uint8_t id = *it;
                generate_response(AIR_CON, AIR_CON_INQUERE, 0x00, id, 0x00);
                ++it; // 指向下一个, 下一轮 2000ms 再发
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }, "AirIDs_poll_task", 2048, nullptr, 3, nullptr);

    // 最终发送队列中的485指令的任务
    xTaskCreate([](void* param) {
        rs485_bus_cmd cmd;
        while (true) {
            if (xQueueReceive(rs485Queue, &cmd, portMAX_DELAY) == pdPASS) {
                uart_write_bytes(RS485_UART_PORT, reinterpret_cast<const char*>(cmd.data), cmd.len);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }, "485_send_bus", 4096, nullptr, 5, nullptr);
    
    // 接收任务
    xTaskCreate([](void* param) {
        uint8_t byte;
        int frame_size = 8;
        enum {
            WAIT_FOR_HEADER,
            RECEIVE_DATA,
        } state = WAIT_FOR_HEADER;

        uint8_t buffer[frame_size];
        int byte_index = 0;

        while (1) {
            // 读取单个字节
            int len = uart_read_bytes(RS485_UART_PORT, &byte, 1, portMAX_DELAY);
            if (len > 0) {
                switch (state) {
                    case WAIT_FOR_HEADER:
                        if (byte == RS485_FRAME_HEADER) {
                            state = RECEIVE_DATA;
                            byte_index = 0;
                            buffer[byte_index++] = byte;
                        } else {
                            ESP_LOGE(TAG, "错误帧头: 0x%02x", byte);
                        }
                        break;

                    case RECEIVE_DATA:
                        buffer[byte_index++] = byte;
                        if (byte_index == frame_size) {
                            if (buffer[0] == RS485_FRAME_HEADER &&
                                buffer[frame_size - 1] == RS485_FRAME_FOOTER)
                            handle_rs485_data(buffer, frame_size);
                            state = WAIT_FOR_HEADER; // 无论成功或失败，都重新等待下一帧
                        }
                        break;
                }
            } else if (len < 0) {
                ESP_LOGE(TAG, "UART 读取错误: %d", len);
            }
        }
        vTaskDelete(NULL);
    }, "485Receive task", 8192, NULL, 7, NULL);
}

void sendRS485CMD(const std::vector<uint8_t>& data) {
    rs485_bus_cmd cmd;
    if (data.size() > RS485_CMD_MAX_LEN) {
        ESP_LOGE(TAG, "Data size (%d) exceeds RS485_CMD_MAX_LEN (%d), truncating", data.size(), RS485_CMD_MAX_LEN);
        cmd.len = RS485_CMD_MAX_LEN;
    } else {
        cmd.len = data.size();
    }
    memcpy(cmd.data, data.data(), cmd.len);
    if (xQueueSend(rs485Queue, &cmd, pdMS_TO_TICKS(3000)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send command to queue");
    }
}

bool is_test_mode() {
    return test_mode;
}

uint8_t calculate_checksum(const std::vector<uint8_t>& data) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < 6; ++i) {
        checksum += data[i];
    }
    return checksum & 0xFF;
}

// 终极处理函数
void handle_rs485_data(uint8_t* data, int length) {
    uint8_t checksum = calculate_checksum(std::vector<uint8_t>(data, data + 6));
    if (data[6] != checksum) {
        ESP_LOGE(TAG, "校验和错误: %d", checksum);
        char hexbuf[8 * 3 + 1]; // 每个字节两位+空格，最后一个\0
        int pos = 0;
        for (size_t i = 0; i < 8; ++i) {
            pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02X ", data[i]);
        }
        ESP_LOGE(TAG, "数据包: %s", hexbuf);
        return;
    }

    if (global_RS485_log_enable_flag) {
        char hexbuf[8 * 3 + 1];
        int pos = 0;
        for (size_t i = 0; i < 8; ++i) {
            pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02X ", data[i]);
        }
        ESP_LOGI(TAG, "收到: %s", hexbuf);
    }
        
    // ******************** 判断功能码 ********************
    static auto& lord = LordManager::instance();
    // 开关上报
    if (data[1] == SWITCH_REPORT) {
        // 百分比调光上报
        if (data[2] == 0x01) {
            lord.handleDimming(data[3], data[4], data[5]);
            return;
        }
        lord.handlePanel(data[3], data[4], data[5]);
    }
    // 语音控制
    else if (data[1] == VOICE_CONTROL) {
        lord.handleVoiceCmd(data);
    }
    // 空调响应
    else if (data[1] == AIR_CON) {
        uint8_t cmd_type = data[2];
        // 空调状态上报
        if (cmd_type == AIR_CON_REPORT) {
            lord.updateAirState(data[4], data[5]);
        }
    }
    // 红外温感发来的指令
    else if (data[1] == INFRARED_CONTEROLLER) {
        // 上报室温
        if (data[2] == 0x00) {
            lord.updateRoomTemp(data[3], data[4]);
        }
        // 响应查询, 返回空调红外码库编码
        else if (data[2] == 0x11) {
            
        }
    }
    // 神谕
    else if (data[1] == ORACLE) {
        // 处理接收wifi ssid和pass包, 一堆包, data[2]为0x7?的都是
        if (is_test_mode() && data[2] >= 0x71 && data[2] <= 0x7F) {
            uint8_t packet_id = data[2];
            bool done = false;
            
            if (!recv_ssid_ing && data[2] != 0x71) {
                // 非法起始包
                ESP_LOGW(TAG, "SSID接收未初始化却收到非首包: 0x%02X", packet_id);
                return;
            }

            if (packet_id == 0x71) {
                recv_ssid_ing = true;
                ssid_offset = 0;
                memset(wifi_ssid, 0, sizeof(wifi_ssid));
                ssid_recv_start_time = esp_timer_get_time();
                expected_ssid_packet = 0x71;
            }

            // 包顺序检查
            if (packet_id != expected_ssid_packet) {
                ESP_LOGW(TAG, "SSID包顺序错误: 收到 0x%02X, 预期 0x%02X", packet_id, expected_ssid_packet);
                recv_ssid_ing = false;
                ssid_offset = 0;
                memset(wifi_ssid, 0, sizeof(wifi_ssid));
                return;
            }
            
            // 每收个包就回复个响应
            generate_response(ORACLE, 0x90 + data[2] - 0x70, 0x01, 0x00, 0x00);
            expected_ssid_packet++;
            
            // data[3]到[5], 一共三字节是真正数据内容
            for (int i = 3; i <= 5; ++i) {
                if (data[i] == 0xFF) {
                    done = true;  // 出现终止符，表示已经结束
                    break;
                }

                if (ssid_offset < sizeof(wifi_ssid) - 1) {
                    wifi_ssid[ssid_offset++] = data[i];
                }
            }

            if (done) {
                wifi_ssid[ssid_offset] = '\0';
                ESP_LOGI(TAG, "接收到完整SSID: %s\n", wifi_ssid);
                recv_ssid_ing = false;
                ssid_offset = 0;
                save_wifi_credentials(wifi_ssid, nullptr);
            }

            return;
        } else if (is_test_mode() && data[2] >= 0x81 && data[2] <= 0x8F) {
            uint8_t packet_id = data[2];
            bool done = false;

            if (!recv_pass_ing && packet_id != 0x81) {
                ESP_LOGW(TAG, "PASS接收未初始化却收到非首包: 0x%02X", packet_id);
                return;
            }

            if (packet_id == 0x81) {
                recv_pass_ing = true;
                pass_offset = 0;
                memset(wifi_pass, 0, sizeof(wifi_pass));
                pass_recv_start_time = esp_timer_get_time();
                expected_pass_packet = 0x81;
            }

            if (packet_id != expected_pass_packet) {
                ESP_LOGW(TAG, "PASS包顺序错误: 收到 0x%02X, 预期 0x%02X", packet_id, expected_pass_packet);
                recv_pass_ing = false;
                pass_offset = 0;
                memset(wifi_pass, 0, sizeof(wifi_pass));
                return;
            }

            generate_response(ORACLE, 0xA0 + (packet_id - 0x80), 0x01, 0x00, 0x00);
            expected_pass_packet++;

            for (int i = 3; i <= 5; ++i) {
                if (data[i] == 0xFF) {
                    done = true;
                    break;
                }

                if (pass_offset < sizeof(wifi_pass) - 1) {
                    wifi_pass[pass_offset++] = data[i];
                }
            }

            if (done) {
                wifi_pass[pass_offset] = '\0';
                ESP_LOGI(TAG, "接收到完整PASS: %s\n", wifi_pass);
                recv_pass_ing = false;
                pass_offset = 0;
                save_wifi_credentials(nullptr, wifi_pass);
            }

            return;
        }

        // 其他指令
        uart_frame_t frame;
        switch (data[2]) {
            case 0x01:  // 进入测试模式
                test_mode = true;
                ESP_LOGI("ORACLE", "进入测试模式");
                if (oracle_task_handle == NULL) {
                    xTaskCreate(periodic_oracle_task, "oracle_task", 2048, NULL, 5, &oracle_task_handle);
                }
                generate_response(ORACLE, 0x01, 0x00, 0x00, 0x00);
                break;
            case 0x00:  // 退出测试模式
                test_mode = false;
                ESP_LOGI("ORACLE", "退出测试模式");
                if (oracle_task_handle != NULL) {
                    vTaskDelete(oracle_task_handle);
                    oracle_task_handle = NULL;
                }
                generate_response(ORACLE, 0x00, 0x00, 0x00, 0x00);
                break;
            case 0x02:  // 控制继电器
                if (is_test_mode()) {
                    build_frame(0x01, data[3], data[4], data[5], 0x00, &frame);
                    send_frame(&frame);
                    break;
                }
            case 0x03:  // 控制干接点输出
                if (is_test_mode()) {
                    build_frame(0x05, data[3], data[4], data[5], 0x00, &frame);
                    send_frame(&frame);
                    break;
                }
            case 0x04:  // 调用stm32跑马灯
                if (is_test_mode()) {
                    build_frame(0x07, 0x02, 0x01, 0x00, 0x00, &frame);
                    send_frame(&frame);
                    break;
                }
            case 0x05:  // 全部控制
                if (is_test_mode()) {
                    for (uint8_t i = 1; i <= 25; i++) {
                        build_frame(0x01, data[3], i, data[5], 0x00, &frame);
                        send_frame(&frame);
                    }
                    for (uint8_t i = 1; i <= 8; i++) {
                        build_frame(0x05, data[3], i, data[5], 0x00, &frame);
                        send_frame(&frame);
                    }
                    break;
                }
            case 0x06:  // 控制干接点输入
                if (is_test_mode()) {
                    uart_frame_t frame;
                    build_frame(CMD_DRYCONTACT_INPUT, 0x00, data[4], data[5], 0x00, &frame);
                    handle_response(&frame);
                    break;
                }
            case 0x07:  // 调光控制
                if (is_test_mode()) {
                    sendStm32Cmd(0x03, 0x00, data[3], data[4], data[5]);
                    break;
                }
            case 0x09:  // 切换网络驱动
                if (is_test_mode()) {
                    if (data[5] == 0x01) {
                        ESP_LOGI(TAG, "切换至WiFi");
                        generate_response(ORACLE, 0x09, 0x01, 0x00, 0x00);
                        change_network_type_and_reboot(NET_TYPE_WIFI);
                    } else if (data[5] == 0x02) {
                        ESP_LOGI(TAG, "切换至以太网");
                        generate_response(ORACLE, 0x09, 0x02, 0x00, 0x00);
                        change_network_type_and_reboot(NET_TYPE_ETHERNET);
                    } else if (data[5] == 0x00) {
                        ESP_LOGI(TAG, "遭到查询, 返回网络状态");
                        report_net_state_to_rs485();
                    }
                    break;
                }
            case 0x0A:  // 查固件版本
                if (is_test_mode()) {
                    generate_response(ORACLE, 0x0A, AETHORAC_VERSION_MAJOR, AETHORAC_VERSION_MINOR, AETHORAC_VERSION_PATCH);
                    break;
                }
            case 0x0B:  // 开关stm32的RX/TX打印
                if (is_test_mode()) {
                    if (data[4] == 0x00) {
                        global_STM32_log_enable_flag = data[5];
                    } else if (data[4] == 0x01) {
                        global_RS485_log_enable_flag = data[5];
                    }
                    break;
                }
            // 上面一堆奇怪的break是故意的, 为了fall到这里来
            default:
                if (is_test_mode()) {
                    ESP_LOGW("ORACLE", "未知测试指令: 0x%02X", data[2]);
                } else {
                    ESP_LOGW("ORACLE", "未处于测试模式");
                    generate_response(ORACLE, 0x79, 0x00, 0x00, 0x00);
                }
                break;
        }
    }
}

void generate_response(uint8_t param1, uint8_t param2, uint8_t param3, uint8_t param4, uint8_t param5) {
    std::vector<uint8_t> command = {
        RS485_FRAME_HEADER,
        param1,
        param2,
        param3,
        param4,
        param5,
        0x00,
        RS485_FRAME_FOOTER
    };

    command[6] = calculate_checksum(command);
    sendRS485CMD(command);
}

void report_net_state_to_rs485() {
    // 1. 获取IP地址
    uint32_t ip_raw = get_ip_raw();

    // 2. 获取序列号（字符串）
    const char *serial_str = getSerialNum();

    // 3. 序列号转成uint32_t（8位数字）
    uint32_t serial_num = (uint32_t)strtoul(serial_str, NULL, 10);

    // 4. 拆成字节
    uint8_t seq_bytes[4] = {
        (uint8_t)((serial_num >> 24) & 0xFF),
        (uint8_t)((serial_num >> 16) & 0xFF),
        (uint8_t)((serial_num >> 8) & 0xFF),
        (uint8_t)(serial_num & 0xFF)
    };

    uint8_t ip_bytes[4] = {
        (uint8_t)(ip_raw & 0xFF),
        (uint8_t)((ip_raw >> 8) & 0xFF),
        (uint8_t)((ip_raw >> 16) & 0xFF),
        (uint8_t)((ip_raw >> 24) & 0xFF)
    };

    // 5. 获取当前网络类型
    uint8_t net_type_byte = 0x00;
    switch (network_current_type()) {
        case NET_TYPE_WIFI: net_type_byte = 0x01; break;
        case NET_TYPE_ETHERNET: net_type_byte = 0x02; break;
        default:
            net_type_byte = 0x00;
            ESP_LOGE(TAG, "预期之外的网络类型: %d", network_current_type());
            break;
    }

    // 6. 构造发送包（14字节）
    uint8_t frame[14] = {
        0x7F, 0x79,          // header
        0x11,                // type
        seq_bytes[0], seq_bytes[1], seq_bytes[2], seq_bytes[3],
        ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3],
        net_type_byte,       // 网络类型
        0x00,                // checksum 占位
        0x7E                 // tail
    };

    // 7. 计算校验和
    
    uint8_t checksum = 0;
    for (int i = 0; i < 12; ++i) checksum += frame[i];
    frame[12] = checksum & 0xFF;

    // 8. 发送
    uart_write_bytes(UART_NUM_1, (const char*)frame, sizeof(frame));
    ESP_LOGI("ORACLE", "发送设备状态包 (网络类型: %s)",
             net_type_byte == 0x01 ? "WiFi" :
             net_type_byte == 0x02 ? "Ethernet" : "None");
}

// 测试模式中的一个loop
void periodic_oracle_task(void *arg)
{
    while (1) {
        int64_t now = esp_timer_get_time();

        if (recv_ssid_ing && (now - ssid_recv_start_time > 2 * 1000000)) {
            ESP_LOGW(TAG, "SSID接收超时, 清空状态");
            recv_ssid_ing = false;
            ssid_offset = 0;
            memset(wifi_ssid, 0, sizeof(wifi_ssid));
        }

        if (recv_pass_ing && (now - pass_recv_start_time > 2 * 1000000)) {
            ESP_LOGW(TAG, "PASS接收超时, 清空状态");
            recv_pass_ing = false;
            pass_offset = 0;
            memset(wifi_pass, 0, sizeof(wifi_pass));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
