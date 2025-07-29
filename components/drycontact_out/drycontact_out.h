#pragma once

#include "idevice.h"
#include "enums.h"

class DryContactOut : public IDevice {
public:
    DryContactOut(uint16_t did, const std::string& name, const std::string&carry_state, uint8_t channel)
        : IDevice(did, DeviceType::DRY_CONTACT, name, carry_state), channel(channel) {}

    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
    void addAssBtn(PanelButtonPair pair) override { associated_buttons.push_back(pair); }
    bool isOn() const override { return current_state == State::ON; }
protected:
    uint8_t channel;
    void open_self(bool should_log);
    void close_self(bool should_log);
    enum class State { OFF, ON };
    State current_state = State::OFF;
    void updateButtonIndicator(bool state);
};