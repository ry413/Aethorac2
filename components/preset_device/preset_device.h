#pragma once

#include "idevice.h"

class PresetDevice : public IDevice {
public:
    PresetDevice(uint8_t did, const std::string& name, const std::string&carry_state, DeviceType type)
        : IDevice(did, type, name, carry_state) {}

private:
    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
    void addAssBtn(PanelButtonPair) override { ESP_LOGW("PresetDevice", "预设设备不应该添加关联按钮"); }
    void syncAssBtnToDevState() override { ESP_LOGW("PresetDevice", "预设设备不应该有关联按钮"); }
    bool isOn() const override { ESP_LOGW("PresetDevice", "预设设备不应该调用isOn"); return false; }
    void updateButtonIndicator(bool state) override { ESP_LOGW("PresetDevice", "预设设备不应该更新指示灯"); }
};