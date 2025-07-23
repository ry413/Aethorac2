#include "lamp.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "commons.h"
#include "lord_manager.h"
#include "stm32_tx.h"
// #include "manager_base.h"
// #include "panel.h"

#define TAG "LAMP"

void Lamp::open_lamp(bool should_log) {
    // add_log_entry("light", uid, "打开", "", should_log);
    //           固定                固定   channel  开    固定
    sendStm32Cmd(CMD_RELAY_CONTROL, 0x00, channel, 0x01, 0x00);
    
    current_state = State::ON;
    updateButtonIndicator(true);
    change_state(true);
    // sync_link_devices("打开");
    // close_repel_devices();
}

void Lamp::close_lamp(bool should_log) {
    // add_log_entry("light", uid, "关闭", "", should_log);
    //           固定                固定   channel  关    固定
    sendStm32Cmd(CMD_RELAY_CONTROL, 0x00, channel, 0x00, 0x00);
    current_state = State::OFF;
    updateButtonIndicator(false);
    change_state(false);
    // sync_link_devices("关闭");
    // 不能关闭排斥设备, 显然
}

void Lamp::execute(std::string operation, std::string parameter, int action_group_id, bool should_log) {
    ESP_LOGI(TAG, "灯[%s]收到操作[%s]", name.c_str(), operation.c_str());
    if (operation == "打开") {
        open_lamp(should_log);
    } else if (operation == "关闭") {
        close_lamp(should_log);
    } else if (operation == "反转") {
        if (current_state == State::ON) {
            close_lamp(should_log);
        } else {
            open_lamp(should_log);
        }
    }
    // else if (operation == "调光") {
    //     ESP_LOGI(__func__, "调光至 %d0%%", parameter);
    //     if (parameter != 0) {
    //         updateButtonIndicator(true);
    //     } else {
    //         updateButtonIndicator(false);
    //     }
    // }
}

void Lamp::updateButtonIndicator(bool state) {
    // 同时开关此灯所有关联的按钮的指示灯
    for (const auto assoc : associated_buttons) {
        LordManager::instance().updateButtonIndicator(assoc, state);
    }
}
