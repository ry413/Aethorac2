// #include "other_device.h"
// #include "board_output.h"

// #include "esp_log.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "commons.h"
// #include "panel.h"
// #include "manager_base.h"
// #include "rs485_comm.h"
// #include "indicator.h"
// #include "room_state.h"
// #include <charconv>

// #define TAG "OTHER_DEVICE"

// void OtherDevice::open_device(bool should_log) {
//     add_log_entry("other", uid, "打开", "", should_log);
//     output->connect();
//     current_state = State::ON;
//     updateButtonIndicator(true);

//     change_state(true);
//     sync_link_devices("打开");
//     close_repel_devices();
// }

// void OtherDevice::close_device(bool should_log) {
//     add_log_entry("other", uid, "关闭", "", should_log);
//     output->disconnect();
//     current_state = State::OFF;
//     updateButtonIndicator(false);

//     change_state(false);
//     sync_link_devices("关闭");
//     // close_repel_devices();   // 关闭本设备, 不需要关闭排斥设备
// }

// void OtherDevice::execute(std::string operation, std::string parameter, int action_group_id, bool should_log) {
//     ESP_LOGI(TAG, "[%s]收到操作[%s], par:[%s]", name.c_str(), operation.c_str(), parameter.c_str());
//     switch (type) {
//         case OtherDeviceType::OUTPUT_CONTROL:
//             if (operation == "打开") {
//                 open_device(should_log);
//             } else if (operation == "关闭") {
//                 close_device(should_log);
//             } else if (operation == "反转") {
//                 if (current_state == State::ON) {
//                     close_device(should_log);
//                 } else {
//                     open_device(should_log);
//                 }
//             }
//             break;

//         case OtherDeviceType::HEARTBEAT_STATE:
//             // 如果收到睡眠操作, 改变心跳包
//             if (operation == "睡眠") {
//                 // 关闭所有指示灯
//                 auto all_panel = PanelManager::getInstance().getAllItems();
//                 for (const auto& panel : all_panel) {
//                     panel.second->turn_off_all_buttons();
//                     panel.second->publish_bl_state();
//                 }
//                 sleep_heartbeat();
//             } else if (operation == "死亡") {
//                 // 拔卡后也关闭指示灯, 但要再打开仍然开着的设备**关联**的指示灯(即保留)
//                 auto all_panel = PanelManager::getInstance().getAllItems();
//                 for (const auto& panel : all_panel) {
//                     // 设置所有灯为关闭并注册, 在下方Panel::trun_on_precise_buttons处打开正确的灯发布
//                     panel.second->turn_off_all_buttons();
//                     panel.second->register_publish_bl_state();
//                 }
//                 Panel::trun_on_precise_buttons();
//                 vTaskDelay(300 / portTICK_PERIOD_MS);
//                 // 发送睡眠心跳
//                 sleep_heartbeat();
//                 // 进入拔卡状态
//                 set_alive(false);

//             } else if (operation == "存活") {
//                 // 进入插卡状态
//                 set_alive(true);
//             }
//             break;
//         case OtherDeviceType::DELAYER:
//             if (operation == "延时") {
//                 int delay = 0;
//                 auto result = std::from_chars(parameter.data(), parameter.data() + parameter.size(), delay);
//                 if (result.ec != std::errc() || delay < 0) {
//                     ESP_LOGE(TAG, "延时参数非法: %s", parameter.c_str());
//                     break;
//                 }

//                 ESP_LOGI(TAG, "延时%d秒\n", delay);
//                 // 在进入延时前先发布已注册的指示灯
//                 IndicatorHolder::getInstance().callAllAndClear();
//                 vTaskDelay(delay * 1000 / portTICK_PERIOD_MS);
//             }
//             break;
//         case OtherDeviceType::ACTION_GROUP_MANAGER:
//             if (operation == "销毁") {
//                 int id = 0;
//                 auto result = std::from_chars(parameter.data(), parameter.data() + parameter.size(), id);
//                 if (result.ec != std::errc() || id < 0) {
//                     ESP_LOGE(TAG, "非法的动作组ID: %s", parameter.c_str());
//                     break;
//                 }

//                 auto actionGroup = ActionGroupManager::getInstance().getItem(id);
//                 if (!actionGroup) {
//                     ESP_LOGE(TAG, "动作组ID %d 不存在, 无法销毁", id);
//                     break;
//                 }

//                 actionGroup->suicide();
//             }
//             break;
//         case OtherDeviceType::STATE_SETTER:
//             if (operation == "添加状态") {
//                 add_state(parameter);
//             } else if (operation == "清除状态") {
//                 remove_state(parameter);
//             } else if (operation == "反转状态") {
//                 toggle_state(parameter);
//             }
//             break;
//         case OtherDeviceType::LOGICIAN:
//             if (operation == "如果存在状态") {
//                 if (exist_state(parameter)) {
//                     ESP_LOGI(TAG, "试图销毁动作组[%d]", action_group_id);
//                     // 销毁自身动作组任务
//                     if (action_group_id != -1) {
//                         auto actionGroup = ActionGroupManager::getInstance().getItem(action_group_id);
//                         actionGroup->suicide();
//                     }
//                 }
//             } else if (operation == "如果不存在状态") {
//                 if (!exist_state(parameter)) {
//                     ESP_LOGI(TAG, "试图销毁动作组[%d]", action_group_id);
//                     if (action_group_id != -1) {
//                         auto actionGroup = ActionGroupManager::getInstance().getItem(action_group_id);
//                         actionGroup->suicide();
//                     }
//                 }
//             }
//             break;
//         case OtherDeviceType::TIMESYNC:
//             if (operation == "广播时间") {
//                 time_t now = get_current_timestamp();

//                 if (now != 0) {
//                     uint8_t p2, p3, p4, p5;
//                     p2 = (now >> 24) & 0xFF;
//                     p3 = (now >> 16) & 0xFF;
//                     p4 = (now >> 8)  & 0xFF;
//                     p5 = now & 0xFF;
                    
//                     generate_response(ALL_TIME_SYNC, p2, p3, p4, p5);
//                 }
//             }
//     }
// }

// void OtherDevice::updateButtonIndicator(bool state) {
//     // 同时开关此灯所有关联的按钮的指示灯
//     for (const auto& assoc : associated_buttons) {
//         // 根据面板 ID 获取面板
//         auto panel = PanelManager::getInstance().getItem(assoc.panel_id);
//         if (panel) {
//             // 根据按钮 ID 获取按钮
//             auto it = panel->buttons.find(assoc.button_id);
//             if (it != panel->buttons.end()) {
//                 auto& button = it->second;
//                 // 设置按钮的背光状态
//                 panel->set_button_bl_state(button->id, state);
//                 panel->register_publish_bl_state();
//             } else {
//                 ESP_LOGW("Indicator", "Button ID %d not found in panel %d", assoc.button_id, assoc.panel_id);
//             }
//         } else {
//             ESP_LOGW("Indicator", "Panel ID %d not found", assoc.panel_id);
//         }
//     }
// }
