#include "rs485_comm.h"
#include "rs485_command.h"
#include "esp_log.h"

void RS485Command::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    ESP_LOGI_CYAN("RS485Command", "发送485指令[%s]\n", name.c_str());
    if (operation == "发送") {
        sendRS485CMD(code);
    }
}