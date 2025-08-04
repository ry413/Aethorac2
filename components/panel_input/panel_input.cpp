#include <esp_log.h>
#include "panel_input.h"
#include "rs485_comm.h"
#include "indicator.h"
#include "lord_manager.h"
#include "drycontact_out.h"

#define TAG "PANEL_INPUT"
PanelButtonInput *last_press_btn;

void PanelButtonInput::execute() {
    ESP_LOGI(TAG, "pid(%d) bid(%d)开始执行动作组(%d)", pid, bid, current_index);
    static auto& lord = LordManager::instance();

    // 任意键执行得放在getAlive检测前面, 因为红外超时导致错误拔卡, 需要可以用面板返回插卡状态
    if (lord.execute_any_key_action_group()) {
        printf("已调用任意键执行动作组, 返回\n");

        // 只要按下按钮, 就试图唤醒
        // 也就是说如果这个按钮的动作组里又有进入睡眠, 则会醒一下再睡去
        if (lord.isSleep() && lord.getAlive()) {
            lord.useAliveHeartBeat();
            // 睡眠会将所有指示灯熄灭, 但有些设备实际上并没有被"关"
            // 所以在醒来后, 要再点亮那些设备的指示灯
            // 应该只会有干接点输出, 即清理和勿扰两个东西
            for (auto* dry : lord.getDevicesByType<DryContactOut>()) {
                dry->syncAssBtnToDevState();
            }
        }
        return;
    }

    // 在拔卡状态时且此按键不允许在拔卡时使用, 则返回
    if (!lord.getAlive() && tag == InputTag::NONE) {
        return;
    }

    // 无任意键执行时, 也要一次这个
    if (lord.isSleep() && lord.getAlive()) {
        lord.useAliveHeartBeat();
        for (auto* dry : lord.getDevicesByType<DryContactOut>()) {
            dry->syncAssBtnToDevState();
        }
    }

    // 判断上一次execute是否与现在是同一个按钮, 不是的话就重置current_index
    if (this != last_press_btn) {
        current_index = 0;
    }

    if (current_index < action_groups.size()) {
        action_groups[current_index]->executeAllAtomicAction();        
        current_index = (current_index + 1) % action_groups.size();
    }

    last_press_btn = this;
}

void Panel::set_button_bl_state(uint8_t button_id, bool state) {
    uint8_t bl_states = button_bl_states;
    if (state) {
        bl_states |= (1 << button_id);
    } else {
        bl_states &= ~(1 << button_id);
    }
    set_button_bl_states(bl_states);
}

void Panel::publish_bl_state(void) {               // 第五位(0xFF)传什么都没事, 面板不在乎
    generate_response(SWITCH_WRITE, 0x00, pid, 0xFF, button_bl_states);
}

void Panel::register_publish_bl_state() {
    // 将当前实例的 publish_bl_state 添加到 IndicatorHolder
    IndicatorHolder::getInstance().addFunction([this]() { publish_bl_state(); }, this);
}

void Panel::wishIndicatorByButton(uint8_t bid, uint8_t state) {
    set_button_bl_state(bid, state);
    register_publish_bl_state();
}

void Panel::wishIndicatorByPanel(uint8_t state) {
    set_button_bl_states(state);
    register_publish_bl_state();
}

void Panel::shortLightIndicator(uint8_t bid) {
    set_button_bl_state(bid, true);
    short_light_bids.push_back(bid);
    publish_bl_state();
    schedule_light_off(1000);
}

void Panel::schedule_light_off(uint32_t delay_ms) {
    if (light_off_timer == nullptr) {
        // 创建新的定时器
        light_off_timer = xTimerCreate(
            "LightOffTimer",                        // 定时器名称
            pdMS_TO_TICKS(delay_ms),                // 定时周期
            pdFALSE,                                // 不自动重载
            (void*)this,                            // 定时器 ID，传递当前对象指针
            light_off_timer_callback                // 回调函数
        );
        if (light_off_timer == nullptr) {
            ESP_LOGE(TAG, "Failed to create light_off_timer for pid(%d)", pid);
            return;
        }
    } else {
        // 如果定时器已存在，先停止定时器
        xTimerStop(light_off_timer, 0);
        // 更新定时器周期
        xTimerChangePeriod(light_off_timer, pdMS_TO_TICKS(delay_ms), 0);
    }
    // 重置并启动定时器
    xTimerReset(light_off_timer, 0);
}

void Panel::light_off_timer_callback(TimerHandle_t xTimer) {
    if (Panel* self = static_cast<Panel*>(pvTimerGetTimerID(xTimer))) {
        for (uint8_t bid : self->short_light_bids) {
            self->set_button_bl_state(bid, false);
        }
        self->publish_bl_state();
        self->short_light_bids.clear();
    }
}

void Panel::switchReport(uint8_t target_buttons, uint8_t old_bl_state) {
    // 更新指示灯们的状态
    set_button_bl_states(old_bl_state);

    // 如果是0xFF, 说明哪个按钮都没按下, 所以重置所有按钮的标记, 这通常是released时会收到的
    if (target_buttons == 0xFF) {
        button_operation_flags = 0x00;
        return;
    }

    // 获取当前的按钮操作标记
    uint8_t operation_flags = button_operation_flags;

    // 遍历每个按钮, 处理按下与释放
    for (int i = 0; i < 8; ++i) {
        uint8_t mask = 1 << i;
        bool is_pressed = !(target_buttons & mask);     // 当前是否按下
        bool is_operating = operation_flags & mask;     // 是否标记为"正在操作"

        if (is_pressed && !is_operating) {
            // 按钮按下, 且未被标记为"正在操作", 则触发按钮逻辑
            auto it = buttons_map.find(i);
            if (it != buttons_map.end() && it->second) {
                it->second->execute();
            }
            operation_flags |= mask;                    // 设置"正在操作"标记
        } else if (!is_pressed && is_operating) {
            // 按钮释放，且之前被标记为"正在操作"
            operation_flags &= ~mask;    // 清除标记
        }
        // 如果按钮状态未变化, 或已经标记为"正在操作", 则不进行任何操作     // [这一行似乎很没存在感, 但是很重要]
    }

    // 更新按钮操作标记
    button_operation_flags = operation_flags;

    // 不在这操作指示灯, 在动作组执行完成后用Indicator一并更新
}