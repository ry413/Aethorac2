#include <esp_log.h>
#include "curtain.h"
#include "lord_manager.h"
#include "stm32_tx.h"
#include "indicator.h"

#define TAG "CURTAIN"

void Curtain::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    ESP_LOGI_CYAN(TAG, "窗帘[%s]收到操作[%s]", name.c_str(), operation.c_str());
    if (operation == "开") {
        add_log_entry("cur", did, operation, parameter, should_log);
        handleOpenAction();
    } else if (operation == "关") {
        add_log_entry("cur", did, operation, parameter, should_log);
        handleCloseAction();
    } else if (operation == "反转") {
        // 反转先不管
        // handleReverseAction();
    }
    // // 后台管理对窗帘的操作特殊处理
    // else if (operation == "开") {
    //     switch (state) {
    //         case State::CLOSED:
    //         case State::CLOSING:
    //         case State::STOPPED:
    //             handleOpenAction();
    //             break;
    //         default:
    //             break;
    //     }
    // } else if (operation == "关") {
    //     switch (state) {
    //         case State::OPEN:
    //         case State::OPENING:
    //         case State::STOPPED:
    //             handleCloseAction();
    //             break;
    //         default:
    //             break;
    //     }
    // } else if (operation == "停止") {
    //     switch (state) {
    //         case State::CLOSING:
    //         case State::OPENING:
    //             stopCurrentAction();
    //             break;
    //         default:
    //             break;
    //     }
    // }
}

bool Curtain::isOn(void) const {
    switch (state) {
        case State::CLOSED:
        case State::CLOSING:
            return false;
        default:
            return true;
    }
}

void Curtain::handleOpenAction() {
    action_buttons = open_buttons;

    if (state == State::OPEN) {
        ESP_LOGI(TAG, "[%s]已经彻底打开, 不做任何操作", name.c_str());
        // 熄灭指示灯
        updateButtonIndicator(action_buttons, false);
        return;
    } else if (state == State::CLOSED || state == State::STOPPED) {
        ESP_LOGI(TAG, "开始打开[%s]...", name.c_str());
        startAction(open_channel, State::OPENING, action_buttons);
        last_action = LastAction::OPENING;
    } else if (state == State::OPENING) {
        ESP_LOGI(TAG, "停止打开[%s]", name.c_str());
        stopCurrentAction();
    } else if (state == State::CLOSING) {
        ESP_LOGI(TAG, "停止关闭, 开始打开[%s]", name.c_str());
        // 熄灭“窗帘关”按钮的指示灯
        updateButtonIndicator(close_buttons, false);
        stopCurrentAction();
        startAction(open_channel, State::OPENING, action_buttons);
        last_action = LastAction::OPENING;
    }
}

void Curtain::handleCloseAction() {
    action_buttons = close_buttons;

    if (state == State::CLOSED) {
        ESP_LOGI(TAG, "[%s]已经彻底关闭, 不做任何操作", name.c_str());
        // 熄灭指示灯
        updateButtonIndicator(action_buttons, false);
        return;
    } else if (state == State::OPEN || state == State::STOPPED) {
        ESP_LOGI(TAG, "开始关闭[%s]...", name.c_str());
        startAction(close_channel, State::CLOSING, action_buttons);
        last_action = LastAction::CLOSING;
    } else if (state == State::CLOSING) {
        ESP_LOGI(TAG, "停止关闭[%s]", name.c_str());
        stopCurrentAction();
    } else if (state == State::OPENING) {
        ESP_LOGI(TAG, "停止打开, 开始关闭[%s]", name.c_str());
        // 熄灭“窗帘开”按钮的指示灯
        updateButtonIndicator(open_buttons, false);
        stopCurrentAction();
        startAction(close_channel, State::CLOSING, action_buttons);
        last_action = LastAction::CLOSING;
    }
}

// void Curtain::handleReverseAction() {
//     action_buttons = reverse_buttons;
//     if (state == State::CLOSED) {
//         // 当前是关闭状态，开始打开
//         ESP_LOGI(TAG, "开始打开[%s]...", name.c_str());
//         startAction(output_open, State::OPENING, action_buttons);
//         last_action = LastAction::OPENING;
//     } else if (state == State::OPENING) {
//         // 正在打开，停止打开
//         ESP_LOGI(TAG, "停止打开[%s]", name.c_str());
//         stopCurrentAction();
//         // 保持 last_action 为 OPENING
//     } else if (state == State::STOPPED) {
//         // 已停止，根据上一次动作方向决定下一步
//         if (last_action == LastAction::OPENING) {
//             // 上一次是打开，反转为关闭
//             ESP_LOGI(TAG, "开始关闭[%s]...", name.c_str());
//             startAction(output_close, State::CLOSING, action_buttons);
//             last_action = LastAction::CLOSING;
//         } else {
//             // 上一次是关闭或未定义，开始打开
//             ESP_LOGI(TAG, "开始打开[%s]...", name.c_str());
//             startAction(output_open, State::OPENING, action_buttons);
//             last_action = LastAction::OPENING;
//         }
//     } else if (state == State::CLOSING) {
//         // 正在关闭，停止关闭
//         ESP_LOGI(TAG, "停止关闭[%s]", name.c_str());
//         stopCurrentAction();
//         // 保持 last_action 为 CLOSING
//     } else if (state == State::OPEN) {
//         // 当前是打开状态，开始关闭
//         ESP_LOGI(TAG, "开始关闭[%s]...", name.c_str());
//         startAction(output_close, State::CLOSING, action_buttons);
//         last_action = LastAction::CLOSING;
//     }
// }

void Curtain::startAction(uint8_t channel, State newState, const std::vector<PanelButtonPair> action_buttons) {
    controlRelay(channel, 0x01);
    state = newState;

    if (actionTaskHandle != nullptr) {
        vTaskDelete(actionTaskHandle);
        actionTaskHandle = nullptr;
    }

    this->action_buttons = action_buttons;

    updateButtonIndicator(action_buttons, true);

    // 创建新的任务来处理窗帘动作
    xTaskCreate([](void* param) {
        Curtain* self = static_cast<Curtain*>(param);
        // 延时运行时间
        vTaskDelay(self->runtime * 1000 / portTICK_PERIOD_MS);
        // 调用完成动作
        self->completeAction();
        // 任务结束，自动删除
        vTaskDelete(nullptr);
    }, "CurtainActionTask", 4096, this, 5, &actionTaskHandle);
}

void Curtain::stopCurrentAction() { 
    if (state == State::OPENING) {
        controlRelay(open_channel, false);
    } else if (state == State::CLOSING) {
        controlRelay(close_channel, false);
    }
    state = State::STOPPED;

    // 删除正在运行的任务
    if (actionTaskHandle != nullptr) {
        vTaskDelete(actionTaskHandle);
        actionTaskHandle = nullptr;
    }

    // 熄灭指示灯
    updateButtonIndicator(action_buttons, false);
}

void Curtain::completeAction() {
    if (state == State::OPENING) {
        ESP_LOGI(TAG, "[%s]已打开", name.c_str());
        controlRelay(open_channel, false);
        state = State::OPEN;
        // 熄灭指示灯
        updateButtonIndicator(action_buttons, false);
        // 重置 last_action
        last_action = LastAction::NONE;
    } else if (state == State::CLOSING) {
        ESP_LOGI(TAG, "[%s]已关闭", name.c_str());
        controlRelay(close_channel, false);
        state = State::CLOSED;
        // 熄灭指示灯
        updateButtonIndicator(action_buttons, false);
        // 重置 last_action
        last_action = LastAction::NONE;
    }

    // 清理任务句柄
    actionTaskHandle = nullptr;
}

// 窗帘异步运行完成后直接更新按键指示灯
void Curtain::updateButtonIndicator(std::vector<PanelButtonPair> buttons, bool state) {
    for (const auto [pid, bid] : buttons) {
        if (Panel* panel = LordManager::instance().getPanelByPid(pid)) {
            panel->wishIndicatorByButton(bid, state);
        }
    }

    IndicatorHolder::getInstance().callAllAndClear();
}