#pragma once

#include <map>
#include <memory>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "idevice.h"
#include "enums.h"

enum class CurtainState { CLOSED, OPEN, OPENING, CLOSING, STOPPED };

class Curtain : public IDevice {
public:
    Curtain(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t open_ch, uint8_t close_ch, uint64_t runtime)
        : IDevice(did, DeviceType::CURTAIN, name, carry_state), open_channel(open_ch), close_channel(close_ch), runtime(runtime) {}

    ~Curtain() {
        if (actionTaskHandle != nullptr) {
            vTaskDelete(actionTaskHandle);
            actionTaskHandle = nullptr;
        }
    }

    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
    void addAssBtn(PanelButtonPair) override { ESP_LOGW("Curtain", "窗帘不该使用addAssBtn"); }
    void syncAssBtnToDevState() override { ESP_LOGW("Curtain", "窗帘不该使用syncAssBtnToDevState"); }
    bool isOn() const override;

    void addOpenAssBtn(PanelButtonPair pair) { open_buttons.push_back(pair); }
    void addCloseAssBtn(PanelButtonPair pair) { close_buttons.push_back(pair); }
    CurtainState getState() const { return state; }

private:
    uint8_t open_channel;
    uint8_t close_channel;
    uint64_t runtime;

    // 情况1.两个按键分别开与关
    CurtainState state = CurtainState::CLOSED;
    std::vector<PanelButtonPair> open_buttons;
    std::vector<PanelButtonPair> close_buttons;
    
    // 情况2.一个按键处理开/关, 两种情况只应该存在一种(暂时不实现)
    std::vector<PanelButtonPair> reverse_buttons;
    enum class LastAction { NONE, OPENING, CLOSING };
    LastAction last_action = LastAction::NONE;

    std::vector<PanelButtonPair> action_buttons;               // 当前动作的按钮（开、关或反转）

    TaskHandle_t actionTaskHandle = nullptr;  // 任务句柄

    // 处理"开"操作
    void handleOpenAction();

    // 处理"关"操作
    void handleCloseAction();

    // // 处理"反转"操作
    // void handleReverseAction();

    // 打开操作对应的继电器
    void startAction(uint8_t channel, CurtainState newState, std::vector<PanelButtonPair> action_buttons);

    // 停止此时的动作的继电器
    void stopCurrentAction();

    // 成功打开/关闭窗帘, 关闭对应继电器, 并熄灭来源按钮的指示灯
    void completeAction();

    void updateButtonIndicator(bool state) override { ESP_LOGW("Curtain", "窗帘不应该调用这个函数"); }
    void updateButtonIndicator(std::vector<PanelButtonPair> buttons, bool state);
};