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

    void executeAllAtomicAction();
    void clearTaskHandle();
    void suicide();
    
    std::vector<AtomicAction> actions;
private:
    uint16_t aid;
    std::string name;
    bool mode;
    TaskHandle_t task_handle = nullptr;
};
