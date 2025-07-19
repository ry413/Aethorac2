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

class BoardOutput;

class Lamp : public IDevice {
public:
    Lamp(uint8_t did, const std::string& name, const std::string&carry_state, uint8_t channel)
        : IDevice(did, DeviceType::LAMP, name, carry_state) {
            this->channel = channel;
        }

    // LampType type;
    
    std::shared_ptr<BoardOutput> output;
    void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
    bool isOn() const { return current_state == State::ON; }
    void updateButtonIndicator(bool state);

private:
    uint8_t channel;
    enum class State { OFF, ON };
    State current_state = State::OFF;

    void open_lamp(bool should_log);
    void close_lamp(bool should_log);
};