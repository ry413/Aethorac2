#pragma once

#include "idevice.h"
#include "enums.h"
#include "action_group.h"

enum class BGMMode { TF, BL };

class BGM : public IDevice {
public:
    BGM(uint16_t did, DeviceType dev_type, const std::string& name, const std::string& carry_state)
        : IDevice(did, dev_type, name, carry_state) {}
    ~BGM() = default;
    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
    void addAssBtn(PanelButtonPair pair) override { associated_buttons.push_back(pair);}
    void syncAssBtnToDevState() override;
    bool isOn() const override { ESP_LOGE("BGM", "背景音乐不应该调用isOn"); return false; };
    void changeMode(BGMMode mode);
protected:
    BGMMode curr_mode = BGMMode::TF;
    void updateButtonIndicator(bool state) override;
};