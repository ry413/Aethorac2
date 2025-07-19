#include "panel_input.h"
// #include "freertos/FreeRTOS.h"
// #include "esp_log.h"
// // #include "../rs485/rs485.h"
// // #include "../my_mqtt/my_mqtt.h"
// // #include "../lamp/lamp.h"
// // #include "../other_device/other_device.h"
// // #include "rs485_comm.h"
// // #include "lamp.h"
// // #include "indicator.h"
// // #include "other_device.h"
// // #include "manager_base.h"
// // #include "panel.h"

// #define TAG "PANEL"
// PanelButton *last_press_btn;

// void PanelButton::execute() {
//     // 在拔卡状态时且此按键不允许在拔卡时使用, 则返回
//     if (!get_alive() && !remove_card_usable) {
//         return;
//     }

//     // 只要按下按钮, 就试图唤醒
//     // 也就是说如果这个按钮的动作组里又有进入睡眠, 则会醒一下再睡去
//     if (is_sleep() && get_alive()) {
//         wakeup_heartbeat();
//         // 睡眠会将所有指示灯熄灭, 但有些设备实际上并没有被"关"
//         // 所以在醒来后, 要再点亮那些设备的指示灯

//         auto lamps = DeviceManager::getInstance().getDevicesOfType<Lamp>();
//         for (auto& lamp : lamps) {
//             if (lamp->isOn()) {
//                 lamp->updateButtonIndicator(true);
//             }
//         }

//         // 我觉得不会有窗帘, 实际上哪怕灯应该也不会有. 最好没有

//         // 目前我只看见"勿扰"与"清理"这两个按键有这b事
//         // auto others = DeviceManager::getInstance().getDevicesOfType<PresetDevice>();
//         // for (auto& other : others) {
//         //     if (other->type == OtherDeviceType::OUTPUT_CONTROL) {
//         //         if (other->isOn()) {
//         //             other->updateButtonIndicator(true);
//         //         }
//         //     }
//         // }
//     }

//     // 判断上一次execute是否与现在是同一个按钮, 不是的话就重置current_index
//     if (this != last_press_btn) {
//         current_index = 0;
//     }


//     if (current_index < action_groups.size()) {
//         action_groups[current_index]->executeAllAtomicAction(mode_name);
//         // 执行指示灯策略
//         execute_polit_actions(current_index);
        
//         current_index = (current_index + 1) % action_groups.size();
//     }

//     last_press_btn = this;
// }

// void PanelButton::execute_polit_actions(uint8_t index) {
//     auto panel = host_panel.lock();
//     if (!panel) {
//         ESP_LOGE(TAG, "host_panel 不存在");
//     }

//     // 处理本按钮的指示灯行为
//     if (index < action_groups.size()) {
//         ButtonPolitAction action = action_groups[index]->pressed_polit_actions;
//         switch (action) {
//             case ButtonPolitAction::LIGHT_ON:
//                 panel->set_button_bl_state(id, true);
//                 panel->publish_bl_state();
//                 break;
//             case ButtonPolitAction::LIGHT_OFF:
//                 panel->set_button_bl_state(id, false);
//                 panel->publish_bl_state();
//                 break;
//             case ButtonPolitAction::LIGHT_SHORT:
//                 panel->set_button_bl_state(id, true);
//                 panel->publish_bl_state();
//                 // 1秒后熄灭
//                 schedule_light_off(1000);
//                 break;
//             case ButtonPolitAction::IGNORE:
//                 // 不做任何操作
//                 break;
//         }
//     }
//     // 处理其他按钮的指示灯行为
//     if (index < action_groups.size()) {
//         ButtonOtherPolitAction action = action_groups[index]->pressed_other_polit_actions;
//         switch (action) {
//             case ButtonOtherPolitAction::LIGHT_OFF:
//                 panel->turn_off_other_buttons(id);
//                 panel->publish_bl_state();
//                 break;
//             case ButtonOtherPolitAction::IGNORE:
//                 // 不做任何操作
//                 break;
//         }
//     }
// }

// void PanelButton::schedule_light_off(uint32_t delay_ms) {
//     if (light_off_timer == nullptr) {
//         // 创建新的定时器
//         light_off_timer = xTimerCreate(
//             "LightOffTimer",                        // 定时器名称
//             pdMS_TO_TICKS(delay_ms),                // 定时周期
//             pdFALSE,                                // 不自动重载
//             (void*)this,                            // 定时器 ID，传递当前对象指针
//             light_off_timer_callback                // 回调函数
//         );
//         if (light_off_timer == nullptr) {
//             ESP_LOGE(TAG, "Failed to create light_off_timer for button %d", id);
//             return;
//         }
//     } else {
//         // 如果定时器已存在，先停止定时器
//         xTimerStop(light_off_timer, 0);
//         // 更新定时器周期
//         xTimerChangePeriod(light_off_timer, pdMS_TO_TICKS(delay_ms), 0);
//     }
//     // 重置并启动定时器
//     xTimerReset(light_off_timer, 0);
// }

// void PanelButton::light_off_timer_callback(TimerHandle_t xTimer) {
//     // 获取 PanelButton 对象指针
//     PanelButton* button = static_cast<PanelButton*>(pvTimerGetTimerID(xTimer));
//     if (button != nullptr) {
//         auto panel = button->host_panel.lock();
//         if (panel) {
//             // 熄灭指示灯
//             panel->set_button_bl_state(button->id, false);
//             panel->publish_bl_state();
//         }
//         // 定时器是一次性的，无需删除或重置
//     }
// }

// void Panel::toggle_button_bl_state(int index) {
//     uint8_t state = get_button_bl_states();
//     state ^= (1 << index);
//     set_button_bl_states(state);
// }

// void Panel::set_button_bl_state(uint8_t button_id, bool state) {
//     uint8_t bl_states = get_button_bl_states();
//     if (state) {
//         bl_states |= (1 << button_id);
//     } else {
//         bl_states &= ~(1 << button_id);
//     }
//     set_button_bl_states(bl_states);
// }

// void Panel::turn_off_other_buttons(uint8_t exclude_button_id) {
//     uint8_t bl_states = get_button_bl_states();
//     bl_states &= (1 << exclude_button_id);  // 保留指定按钮的状态，其余清零
//     set_button_bl_states(bl_states);
// }

// void Panel::turn_off_all_buttons() {
//     set_button_bl_states(0);
// }

// void Panel::trun_on_precise_buttons() {
//     auto lamps = DeviceManager::getInstance().getDevicesOfType<Lamp>();
//     for (auto& lamp : lamps) {
//         if (lamp->isOn()) {
//             lamp->updateButtonIndicator(true);
//         }
//     }

//     // 没有窗帘

//     // auto others = DeviceManager::getInstance().getDevicesOfType<PresetDevice>();
//     // for (auto& other : others) {
//     //     if (other->type == OtherDeviceType::OUTPUT_CONTROL) {
//     //         if (other->isOn()) {
//     //             other->updateButtonIndicator(true);
//     //         }
//     //     }
//     // }

//     IndicatorHolder::getInstance().callAllAndClear();
// }

// void Panel::publish_bl_state(void) {               // 第五位(0xFF)传什么都没事, 面板不在乎
//     generate_response(SWITCH_WRITE, 0x00, id, 0xFF, get_button_bl_states());
// }

// void Panel::register_publish_bl_state() {
//     // 将当前实例的 publish_bl_state 添加到 IndicatorHolder
//     IndicatorHolder::getInstance().addFunction([this]() { publish_bl_state(); }, this);
// }

void PanelButtonInput::execute() {}