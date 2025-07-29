#pragma once

#include "idevice.h"
#include "enums.h"
#include "action_group.h"

class SingleRelayDevice : public IDevice {
public:
    SingleRelayDevice(uint16_t did, DeviceType dev_type, const std::string& name, const std::string& carry_state, uint8_t channel, bool initial_state)
        : IDevice(did, dev_type, name, carry_state), channel(channel) {
        updateButtonIndicator(initial_state);
    }
    ~SingleRelayDevice() = default;
    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
    void addAssBtn(PanelButtonPair pair) override { associated_buttons.push_back(pair);}
    void syncAssBtnToDevState() override;
    bool isOn() const override;
protected:
    uint8_t channel;
    void open_self(bool should_log);
    void close_self(bool should_log);
    void updateButtonIndicator(bool state) override;
};