#pragma once

#include <stdint.h>
#include "enums.h"
#include <string>
#include <unordered_map>
#include <memory>
#include "idevice.h"
#include "action_group.h"
#include "iinput.h"
#include "channel_input.h"
#include "panel_input.h"


class LordManager {
public:
    static LordManager& instance() {
        static LordManager instance;
        return instance;
    }

    void updateChannelState(uint8_t board_id, uint8_t channel, uint8_t is_on);
    void executeInputAction(uint8_t board_id, uint8_t channel, uint8_t is_on);

    void registerPreset(uint8_t did, const std::string& name, const std::string&carry_state, DeviceType type);
    void registerLamp(uint8_t did, const std::string& name, const std::string&carry_state, uint8_t channel);
    void registerCurtain(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t open_ch, uint8_t close_ch, uint32_t runtime);
    void registerIngraredAir(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t airId);
    void registerSingleAir(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t airId, uint8_t wc, uint8_t lc, uint8_t mc, uint8_t hc);
    void registerRs485(uint8_t did, const std::string& name, const std::string& carry_state, const std::string& code);
    void registerRelayOut(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t channel);
    void registerDryContactOut(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t channel);
    IDevice* getDeviceByDid(uint8_t did);

    void registerActionGroup(uint8_t aid, const std::string& name, bool is_mode, std::vector<AtomicAction> actions);

    void registerPanelKeyInput(uint8_t iid, const std::string& name, InputTag tag, uint8_t pid, uint8_t bid, std::vector<std::unique_ptr<ActionGroup>>&& action_groups);
    void registerDryContactInput(uint8_t iid, InputType type, const std::string& name, InputTag tag, uint8_t channel, TriggerType trigger_type, uint64_t duration, std::vector<std::unique_ptr<ActionGroup>>&& action_groups);
private:
    LordManager() = default;
    std::string config_version;
    uint8_t alive_channel;
    uint8_t door_channel;

    // std::string last_mode_name;
    std::unordered_map<uint8_t, std::unique_ptr<IDevice>> devices_map;              // did, device
    std::unordered_map<uint8_t, std::unique_ptr<ActionGroup>> action_groups_map;    // aid, action_group
    // std::unordered_map<uint8_t, std::unique_ptr<InputBase>> inputs;
    std::unordered_map<uint8_t, std::unique_ptr<ChannelInput>> channel_inputs;      // iid, channel_input
    std::unordered_map<uint8_t, std::unique_ptr<Panel>> panels_map;                 // pid, panel // 这个其实不使用iid, panel不是派生类
};