#pragma once

#include "idevice.h"
#include <memory>

class BoardOutput;

enum class OtherDeviceType {
    OUTPUT_CONTROL,
    HEARTBEAT_STATE,
    DELAYER,
    ACTION_GROUP_MANAGER,
    STATE_SETTER,
    LOGICIAN,
    TIMESYNC
};

// 噢, 我知道这个类简直跟Lamp一模一样

class PresetDevice : public IDevice {
public:
    PresetDevice(uint8_t did, const std::string& name, const std::string&carry_state, DeviceType type)
        : IDevice(did, type, name, carry_state) {}

    // OtherDeviceType type;
    // std::shared_ptr<BoardOutput> output;
    void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
    bool isOn() const { return current_state == State::ON; }
    void updateButtonIndicator(bool state);
    
private:
    enum class State { OFF, ON };
    State current_state = State::OFF;

    // 只用于case OtherDeviceType::OUTPUT_CONTROL的封装
    void open_device(bool should_log);
    void close_device(bool should_log);
};