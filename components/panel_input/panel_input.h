#pragma once

#include <unordered_map>
#include <memory>
#include <vector>
#include <unordered_map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "action_group.h"
#include "iinput.h"

class PanelButtonInput : public InputBase {
public:
    PanelButtonInput(uint16_t iid, const std::string& name, uint8_t pid, int8_t bid, std::set<InputTag> tags, std::vector<std::unique_ptr<ActionGroup>>&& action_groups)
        : InputBase(iid, InputType::PANEL_BTN, name, tags, std::move(action_groups)), pid(pid), bid(bid) {}

    void execute() override;
    uint8_t getBid() const { return bid; }

private:
    uint8_t pid;
    uint8_t bid;
};

extern PanelButtonInput* last_press_btn;

class Panel {
public:
    Panel(uint8_t pid)
        : pid(pid) {}

    ~Panel() {
        if (light_off_timer != nullptr) {
            xTimerStop(light_off_timer, 0);
            xTimerDelete(light_off_timer, 0);
            light_off_timer = nullptr;
        }
    }
    
    bool addButton(uint16_t iid, const std::string& name, uint8_t bid, std::set<InputTag> tags, std::vector<std::unique_ptr<ActionGroup>>&& action_groups) {
        auto btn = std::make_unique<PanelButtonInput>(iid, name, pid, bid, tags, std::move(action_groups));
        auto [it, ok] = buttons_map.try_emplace(bid, std::move(btn));
        return ok;
    }

    uint8_t getPid() const { return pid; }

    // 修改此面板指定按键的指示灯状态并注册更新函数, 之后必须在某处使用Indicator来call
    void wishIndicatorByButton(uint8_t bid, uint8_t state);
    // 修改此面板所有按键的指示灯状态并注册更新函数, 之后必须在某处使用Indicator来call
    void wishIndicatorByPanel(uint8_t state);
    void shortLightIndicator(uint8_t bid);

    // 处理485发来的开关上报码, 真正的处理函数
    void switchReport(uint8_t target_buttons, uint8_t old_bl_state);
    void dimmingReport(uint8_t target_buttons, uint8_t brightness);

    // 短亮按键指示灯用的临时储存点
    std::vector<uint8_t> short_light_bids;
private:
    uint8_t pid;
    std::unordered_map<uint8_t, std::unique_ptr<PanelButtonInput>> buttons_map;

    // 用于"亮1秒"(后熄灭指示灯)的定时器
    void schedule_light_off(uint32_t delay_ms);
    TimerHandle_t light_off_timer = nullptr;
    static void light_off_timer_callback(TimerHandle_t xTimer);

    // ================ 驱动层 ================
    uint8_t button_bl_states = 0x00;        // 所有按钮的背光状态, 1亮0灭
    uint8_t button_operation_flags = 0x00;  // 按钮们的"正在操作"标记

    // 设置此面板所有按钮的指示灯, 之后必须在某处进行publish_bl_state才算真的修改了物理指示灯
    void set_button_bl_states(uint8_t state) { button_bl_states = state; }
    // 设置此面板指定按钮的指示灯, 之后必须在某处进行publish_bl_state才算真的修改了物理指示灯
    void set_button_bl_state(uint8_t button_id, bool state);

    // 注册此面板到某个结构里, 表示此面板想要更新指示灯
    void register_publish_bl_state();

    // 终端函数, 发送指令更新面板状态
    void publish_bl_state(void);

};