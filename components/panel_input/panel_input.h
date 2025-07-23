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
    PanelButtonInput(uint16_t iid, const std::string& name, uint8_t pid, int8_t bid, InputTag tag, std::vector<std::unique_ptr<ActionGroup>>&& action_groups)
        : InputBase(iid, InputType::PANEL_BTN, name, tag, std::move(action_groups)), pid(pid), bid(bid) {}

    void execute() override;
    uint8_t getBid() const { return bid; }

private:
    uint8_t pid;
    uint8_t bid;
};

class Panel {
public:
    Panel(uint8_t pid)
        : pid(pid) {}
    
    bool addButton(uint16_t iid, const std::string& name, uint8_t bid, InputTag tag, std::vector<std::unique_ptr<ActionGroup>>&& action_groups) {
        auto btn = std::make_unique<PanelButtonInput>(iid, name, pid, bid, tag, std::move(action_groups));
        auto [it, ok] = buttons_map.try_emplace(bid, std::move(btn));
        return ok;
    }

    uint8_t getPid() const { return pid; }
    // 处理485发来的开关上报码
    void switchReport(uint8_t target_buttons, uint8_t old_bl_state);
    // 更新某按钮的指示灯(不是即时的)
    void updateButtonIndicator(uint8_t bid, bool state);
    
private:
    uint8_t pid;
    std::unordered_map<uint8_t, std::unique_ptr<PanelButtonInput>> buttons_map;


    // ================ 驱动层 ================
    uint8_t button_bl_states = 0x00;        // 所有按钮的背光状态, 1亮0灭
    uint8_t button_operation_flags = 0x00;  // 按钮们的"正在操作"标记
    // 设置所有整个面板所有按钮的指示灯
    void set_button_bl_states(uint8_t state) { button_bl_states = state; }
    uint8_t get_button_bl_states(void) const { return button_bl_states; }

    void set_button_operation_flags(uint8_t flags) { button_operation_flags = flags; }
    uint8_t get_button_operation_flags(void) const { return button_operation_flags; }

    
    // 翻转某一位的指示灯
    // void toggle_button_bl_state(int index);

    // 设置指定按钮的指示灯状态
    void set_button_bl_state(uint8_t button_id, bool state);

    // 设置除指定按钮外的所有按钮的指示灯为熄灭
    // void turn_off_other_buttons(uint8_t exclude_button_id);

    // **直接打开(即publish)**所有开着的设备的关联按钮的指示灯
    // void static trun_on_precise_buttons(void);

    // 设置所有指示灯为灭
    // void turn_off_all_buttons(void);

    // 终端函数, 发送指令更新面板状态
    void publish_bl_state(void);

    // 注册此面板到某个结构里, 表示此面板想要更新指示灯
    void register_publish_bl_state();
};