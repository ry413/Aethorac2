#include "lamp.h"
#include "board_output.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "commons.h"
#include "manager_base.h"
// #include "panel.h"

#define TAG "LAMP"

void Lamp::open_lamp(bool should_log) {
    // add_log_entry("light", uid, "打开", "", should_log);
    // output->connect();
    // current_state = State::ON;
    // updateButtonIndicator(true);

    // change_state(true);
    // sync_link_devices("打开");
    // close_repel_devices();
}

void Lamp::close_lamp(bool should_log) {
    // add_log_entry("light", uid, "关闭", "", should_log);
    // output->disconnect();
    // current_state = State::OFF;
    // updateButtonIndicator(false);

    // change_state(false);
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
    // for (const auto& assoc : associated_buttons) {
    //     // 根据面板 ID 获取面板
    //     auto panel = PanelManager::getInstance().getItem(assoc.panel_id);
    //     if (panel) {
    //         // 根据按钮 ID 获取按钮
    //         auto it = panel->buttons.find(assoc.button_id);
    //         if (it != panel->buttons.end()) {
    //             auto& button = it->second;
    //             // 设置按钮的背光状态
    //             panel->set_button_bl_state(button->id, state);
    //             // 不直接发布状态, 而是等待整个动作组执行完一并更新
    //             panel->register_publish_bl_state();
    //         } else {
    //             ESP_LOGW("Indicator", "Button ID %d not found in panel %d", assoc.button_id, assoc.panel_id);
    //         }
    //     } else {
    //         ESP_LOGW("Indicator", "Panel ID %d not found", assoc.panel_id);
    //     }
    // }
}
