#pragma once

#include <string>
#include <vector>

#define RS485_UART_PORT   1
#define RS485_TX_PIN      17
#define RS485_RX_PIN      5
#define RS485_DE_GPIO_NUM 33
#define RS485_BAUD_RATE   9600
#define RS485_BUFFER_SIZE 1024 * 2

#define RS485_FRAME_HEADER 0x7F
#define RS485_FRAME_FOOTER 0x7E
#define RS485_CMD_MAX_LEN   8
#define RS485_QUEUE_LEN     50

#define SWITCH_REPORT       0x00        // 按钮输入
#define SWITCH_WRITE        0x01        // 控制按钮

#define AIR_CON             0x16        // 设备类型: 空调
#define AIR_CON_INQUIRE_XZ  0xA2        // 逼迫温控器上报状态(XZ的)
#define AIR_CON_CONTROL     0xA1        // 空调控制
#define AIR_CON_INQUERE     0xA0        // 空调查询(逼迫温控器上报状态)
#define AIR_CON_REPORT      0x08        // 空调响应
#define INFRARED_CONTEROLLER 0x77       // 红外控制器, 就是空调遥控

#define ORACLE              0x79        // esp32测试相关
#define ALL_TIME_SYNC       0x78        // 广播时间
#define VOICE_CONTROL       0x80        // 语音控制

struct rs485_bus_cmd {
    size_t len;
    uint8_t data[RS485_CMD_MAX_LEN];
};

extern bool global_RS485_log_enable_flag;

void uart_init_rs485();
uint8_t calculate_checksum(const std::vector<uint8_t>& data);
void sendRS485CMD(const std::vector<uint8_t>& data);
void handle_rs485_data(uint8_t* data, int length);
void generate_response(uint8_t param1, uint8_t param2, uint8_t param3, uint8_t param4, uint8_t param5);

// 是否是测试模式
bool is_test_mode();
// 报告网络状态到485里, 只有我那个脚本可以处理
void report_net_state_to_rs485();