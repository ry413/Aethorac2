#include "lamp.h"
#include "commons.h"

#define TAG "LAMP"

void Lamp::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    ESP_LOGI_CYAN(TAG, "灯[%s]收到操作[%s]", name.c_str(), operation.c_str());
    if (operation == "调光") {

    } else {
        SingleRelayDevice::execute(operation, parameter, self_action_group, should_log);
    }
}