#pragma once

#include <stdint.h>
#include "enums.h"
#include <string>
#include <unordered_map>
#include <memory>
#include "commons.h"
#include "idevice.h"
#include "action_group.h"
#include "iinput.h"
#include "channel_input.h"
#include "panel_input.h"

#define ALIVE_HEARTBEAT_CODE "7FC0FFFF0080BD7E"     // 存活心跳, 所有设备正常工作
#define SLEEP_HEARTBEAT_CODE "7FC0FFFF00003D7E"     // 睡眠心跳, 所有设备仍然工作但是按键面板背光会熄灭

class LordManager {
public:
    static LordManager& instance() {
        static LordManager instance;
        return instance;
    }

    void clearAll();

    void updateChannelState(uint8_t board_id, uint8_t channel, uint8_t is_on);
    void executeInputAction(uint8_t board_id, uint8_t channel, uint8_t is_on);

    // ================ 一些普通信息 ================
    void setCommonConfig(const std::string& datetime) { config_generate_time = datetime; }
    const std::string& getConfigGenerageTime() const { return config_generate_time; }
    
    // ================ 注册各种东西 ================
    void registerPreset(uint16_t did, const std::string& name, const std::string&carry_state, DeviceType type);
    void registerLamp(uint16_t did, const std::string& name, const std::string&carry_state, uint8_t channel);
    void registerCurtain(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t open_ch, uint8_t close_ch, uint64_t runtime);
    void registerIngraredAir(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t airId);
    void registerSingleAir(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t airId, uint8_t wc, uint8_t lc, uint8_t mc, uint8_t hc);
    void registerRs485(uint16_t did, const std::string& name, const std::string& carry_state, const std::string& code);
    void registerRelayOut(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel);
    void registerDryContactOut(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel);
    void registerActionGroup(uint16_t aid, const std::string& name, bool is_mode, std::vector<AtomicAction> actions);
    void registerPanelKeyInput(uint16_t iid, const std::string& name, InputTag tag, uint8_t pid, uint8_t bid, std::vector<std::unique_ptr<ActionGroup>>&& action_groups);
    void registerDryContactInput(uint16_t iid, InputType type, const std::string& name, InputTag tag, uint8_t channel, TriggerType trigger_type, uint64_t duration, std::vector<std::unique_ptr<ActionGroup>>&& action_groups);

    // ================ 获取注册表里的某些东西 ================
    IDevice* getDeviceByDid(uint16_t did) const;
    template <typename T> std::vector<T*> getDevicesByType() const {
        static_assert(std::is_base_of<IDevice, T>::value, "T must derive from IDevice");
        std::vector<T*> result;
        for (const auto& [did, dev] : devices_map) {
            if (auto casted = dynamic_cast<T*>(dev.get())) {
                result.push_back(casted);
            }
        }
        return result;
    }
    std::vector<ActionGroup*> getAllModeActionGroup() const;
    Panel* getPanelByPid(uint8_t pid) const;

    // ================ 心跳包 ================
    const std::vector<uint8_t>& getHeartbeatCode() const { return heartbeat_code; }
    void useAliveHeartBeat() { ESP_LOGI("心跳包", "切换至插卡心跳"); heartbeat_code = pavectorseHexToFixedArray(ALIVE_HEARTBEAT_CODE); }
    void useSleepHeartBeat() { ESP_LOGI("心跳包", "切换至睡眠心跳"); heartbeat_code = pavectorseHexToFixedArray(SLEEP_HEARTBEAT_CODE); }

    // ================ 按键面板 ================
    void handlePanel(uint8_t panel_id, uint8_t target_buttons, uint8_t old_bl_state);
    void updateButtonIndicator(PanelButtonPair assoc, bool state);
    
    // ================ 空调 ================
    void updateAirState(uint8_t states, uint8_t temps);
    void updateRoomTemp(uint8_t air_id, uint8_t room_temp);
    
    // ================ 一些很有用的标志位 ================
    bool getAlive() const { return true; }
    const std::string& getLastModeName() const { return last_mode_name; }
private:
    LordManager() = default;
    std::vector<uint8_t> heartbeat_code = pavectorseHexToFixedArray(SLEEP_HEARTBEAT_CODE);        // 不停发的心跳包, 不停地
    
    std::string config_generate_time;
    // uint8_t alive_channel;
    // uint8_t door_channel;

    std::string last_mode_name;
    std::unordered_map<uint16_t, std::unique_ptr<IDevice>> devices_map;             // did, device
    std::unordered_map<uint16_t, std::unique_ptr<ActionGroup>> action_groups_map;   // aid, action_group
    std::unordered_map<uint16_t, std::unique_ptr<ChannelInput>> channel_inputs;     // iid, channel_input
    std::unordered_map<uint8_t, std::unique_ptr<Panel>> panels_map;                 // pid, panel // 这个其实不使用iid, panel不是派生类
};