#pragma once

#include "relay_out.h"
#include "enums.h"

enum class LampType {
    SWITCH_LIGHT,
    DIMMABLE_LIGHT
};

class Lamp : public SingleRelayDevice {
public:
    Lamp(uint16_t did, const std::string& name, const std::string&carry_state, uint8_t channel, bool initial_state)
        : SingleRelayDevice(did, DeviceType::LAMP, name, carry_state, channel, initial_state) {}

    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
};