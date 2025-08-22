#pragma once

#include <string>
#include <memory>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// extern int64_t last_action_group_time;

class IDevice;

// 最原子级的一条操作, 某种意义上
struct AtomicAction {
    IDevice* target_device;                     // 本操作的目标设备
    std::string operation;                      // 操作名, 直接交由某个设备处理
    std::string parameter;                      // 有什么是字符串不能表示的呢
};
    
// 动作组基类
class ActionGroup {
public:
    ActionGroup(uint16_t aid, const std::string& name, bool is_mode, std::vector<AtomicAction> actions)
        : actions(actions), aid(aid), name(name), mode(is_mode) {}
    
    uint16_t getAid() const { return aid; }
    const std::string& getName() const { return name; }
    bool is_mode() const { return mode; }

    // 取消/可中断延时接口
    static constexpr uint32_t CANCEL_BIT = 0x1;   // 任务通知位：取消
    bool delay_ms(uint32_t ms);                   // 可被取消的睡眠，true=睡满，false=被取消
    void request_cancel();                        // 请求取消（唤醒正在睡眠的任务）
    bool cancelled() const { return cancel_flag; }

    void executeAllAtomicAction();
    void clearTaskHandle();
    void suicide();
    
    std::vector<AtomicAction> actions;
private:
    uint16_t aid;
    std::string name;
    bool mode;
    volatile bool cancel_flag = false;            // 取消标志
    TaskHandle_t task_handle = nullptr;
};
