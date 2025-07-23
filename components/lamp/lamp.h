#pragma once

#include <map>
#include <memory>
#include <atomic>
#include "idevice.h"
#include "enums.h"

enum class LampType {
    SWITCH_LIGHT,
    DIMMABLE_LIGHT
};

class Lamp : public IDevice {
public:
    Lamp(uint16_t did, const std::string& name, const std::string&carry_state, uint8_t channel)
        : IDevice(did, DeviceType::LAMP, name, carry_state), channel(channel) {}

    // LampType type;

    void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
    void addAssBtn(PanelButtonPair pair) { associated_buttons.push_back(pair);}
    bool isOn() const { return current_state == State::ON; }
    
private:
    uint8_t channel;
    enum class State { OFF, ON };
    State current_state = State::OFF;
    
    void open_lamp(bool should_log);
    void close_lamp(bool should_log);
    void updateButtonIndicator(bool state);
};