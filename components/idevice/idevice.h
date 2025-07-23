#pragma once

#include <string>
#include <vector>
#include "enums.h"

struct PanelButtonPair {
    uint8_t panel_id;
    uint8_t button_id;
};

// 所有设备的基类
class IDevice {
public:
    IDevice(uint16_t did, DeviceType type, const std::string& name, const std::string& carry_state)
        : did(did), type(type), name(name), carry_state(carry_state) {}
    std::vector<PanelButtonPair> associated_buttons;   // 关联按钮
    std::vector<int> repel_device_uids;                 // 使能此设备会关闭排斥设备

    virtual ~IDevice() = default;
    virtual void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) = 0;

    // virtual void addAssBtn(PanelButtonPair);

    uint16_t getDid()  const { return did;  }
    DeviceType getType() const { return type; }
    const std::string& getName() const { return name; }

protected:
    uint16_t did;
    DeviceType type;
    std::string  name;
    std::string carry_state;                            // 此设备会携带的房间状态
    void change_state(bool state);                      // 更改携带的房间状态

    // void sync_link_devices(std::string operation);      // 同步操作联动设备
    // void close_repel_devices(void);                     // 关闭排斥设备

private:
    // bool operated_flag = false;      // 用于联动设备的操作链, 表示此设备是否已经被动过了, 防止递归
};