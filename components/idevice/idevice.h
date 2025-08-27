#pragma once

#include <string>
#include <vector>
#include "enums.h"
#include "action_group.h"
#include "esp_log.h"

struct PanelButtonPair {
    uint8_t panel_id;
    uint8_t button_id;
};

// 所有设备的基类
class IDevice {
public:
    IDevice(uint16_t did, DeviceType type, const std::string& name, const std::string& carry_state)
        : did(did), type(type), name(name), carry_state(carry_state) {}

    virtual ~IDevice() = default;
    virtual void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) = 0;
    virtual void addAssBtn(PanelButtonPair) = 0;// 添加关联按钮(按键指示灯)至本设备
    virtual void syncAssBtnToDevState() { ESP_LOGW("IDevice", "基类方法不该被调用到"); } // 将本设备可能拥有的关联按键的指示灯, 调整至本设备的onoff状态
    virtual bool isOn() const = 0;
    bool isOperated(void) { return operated_flag; };
    uint16_t getDid()  const { return did;  }
    DeviceType getType() const { return type; }
    const std::string& getName() const { return name; }
    void addLinkDidsAndRepelDids(const std::vector<uint16_t> lkds, const std::vector<uint16_t> rpds) { link_dids = lkds; repel_dids = rpds;}

protected:
    uint16_t did;
    DeviceType type;
    std::string  name;
    std::string carry_state;                            // 此设备会携带的房间状态
    void change_state(bool state);                      // 更改携带的房间状态
    virtual void updateButtonIndicator(bool state) = 0;
    std::vector<PanelButtonPair> associated_buttons;    // 关联按钮

    std::vector<uint16_t> link_dids;                  // 此设备动作时会同时操作联动设备
    std::vector<uint16_t> repel_dids;                 // 开启此设备会关闭排斥设备(关闭当然不会)
    void sync_link_devices(std::string operation, bool should_log = true);// 同步操作联动设备
    void close_repel_devices(void);                     // 关闭排斥设备

private:
    bool operated_flag = false;      // 用于联动设备的操作链, 表示此设备是否已经被动过了, 防止无限递归
};
