// #ifndef CURTAIN_H
// #define CURTAIN_H

// #include <map>
// #include <memory>
// #include <vector>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "board_output.h"
// #include "idevice.h"
// #include "panel.h"

// class Curtain : public IDevice {
// public:
//     std::shared_ptr<BoardOutput> output_open;
//     std::shared_ptr<BoardOutput> output_close;
//     uint16_t run_duration;

//     void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
//     bool getState(void);

//     ~Curtain() {
//         if (actionTaskHandle != nullptr) {
//             vTaskDelete(actionTaskHandle);
//             actionTaskHandle = nullptr;
//         }
//     }

//     std::vector<std::weak_ptr<PanelButton>> reverse_buttons;// 关联的按钮, "反转"与"开/关"只会同时存在一种组合

//     std::vector<std::weak_ptr<PanelButton>> open_buttons;
//     std::vector<std::weak_ptr<PanelButton>> close_buttons;

// private:
//     enum class State { CLOSED, OPEN, OPENING, CLOSING, STOPPED };   // 通常使用的"开/关"控制用的状态
//     State state = State::CLOSED;

//     enum class LastAction { NONE, OPENING, CLOSING };   // 用于, 用一个按键控制窗帘的时候, 即"反转"操作
//     LastAction last_action = LastAction::NONE;

//     std::vector<std::weak_ptr<PanelButton>> action_buttons;               // 当前动作的按钮（开、关或反转）

//     TaskHandle_t actionTaskHandle = nullptr;  // 任务句柄

//     // 处理"开"操作
//     void handleOpenAction();

//     // 处理"关"操作
//     void handleCloseAction();

//     // 处理"反转"操作
//     void handleReverseAction();

//     // 打开操作对应的继电器
//     void startAction(std::shared_ptr<BoardOutput> channel, State newState, const std::vector<std::weak_ptr<PanelButton>>& action_button);

//     // 停止此时的动作的继电器
//     void stopCurrentAction();

//     // 成功打开/关闭窗帘, 关闭对应继电器, 并熄灭来源按钮的指示灯
//     void completeAction();

//     void updateButtonIndicator(const std::vector<std::weak_ptr<PanelButton>>& button, bool state);
// };
// #endif // CURTAIN_H