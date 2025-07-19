#include "rs485_comm.h"
#include "rs485_command.h"
#include "esp_log.h"

void RS485Command::execute(std::string operation, std::string parameter, int action_group_id, bool should_log) {
    ESP_LOGI("RS485Command::execute", "发送485指令[%s] %s\n", name.c_str(), operation.c_str());
    if (operation == "发送") {
        sendRS485CMD(code);
    }
}