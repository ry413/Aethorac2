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
#include "voice_command.h"
#include "esp_timer.h"
#include "room_state.h"

static std::array<uint8_t, 8> alive_heartbeat_code = {0x7F, 0xC0, 0xFF, 0xFF, 0x00, 0x80, 0xBD, 0x7E};
static std::array<uint8_t, 8> sleep_heartbeat_code = {0x7F, 0xC0, 0xFF, 0xFF, 0x00, 0x00, 0x3D, 0x7E};

class LordManager {
public:
    static LordManager& instance() {
        static LordManager instance;
        return instance;
    }

    // ================ 一些普通信息 ================
    void setCommonConfig(const std::string& datetime) { config_generate_time = datetime; }
    const std::string& getConfigGenerageTime() const { return config_generate_time; }
    
    // ================ 注册各种东西 ================
    void registerPreset(uint16_t did, const std::string& name, const std::string&carry_state, DeviceType type);
    void registerLamp(uint16_t did, const std::string& name, const std::string&carry_state, uint8_t channel, const std::vector<uint16_t> link_dids, const std::vector<uint16_t> repel_dids);
    void registerCurtain(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t open_ch, uint8_t close_ch, uint64_t runtime);
    void registerIngraredAir(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t airId);
    void registerSingleAir(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t airId, uint8_t wc, uint8_t lc, uint8_t mc, uint8_t hc);
    void registerRs485(uint16_t did, const std::string& name, const std::string& carry_state, const std::string& code);
    void registerRelayOut(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel, const std::vector<uint16_t> link_dids, const std::vector<uint16_t> repel_dids);
    void registerDryContactOut(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel, const std::vector<uint16_t> link_dids, const std::vector<uint16_t> repel_dids);
    void registerDoorbell(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel);
    void registerActionGroup(uint16_t aid, const std::string& name, bool is_mode, std::vector<AtomicAction> actions);

    void registerPanelKeyInput(uint16_t iid, const std::string& name, std::set<InputTag> tags, uint8_t pid, uint8_t bid, std::vector<std::unique_ptr<ActionGroup>>&& action_groups);
    void registerDryContactInput(uint16_t iid, const std::string& name, std::set<InputTag> tags, uint8_t channel, TriggerType trigger_type, uint64_t duration, std::vector<std::unique_ptr<ActionGroup>>&& action_groups);
    void registerVoiceInput(uint16_t iid, const std::string& name, std::set<InputTag> tags, const std::string& code, std::vector<std::unique_ptr<ActionGroup>>&& action_groups);

    // ================ 获取注册表里的某些东西 ================
    IDevice* getDeviceByDid(uint16_t did);
    template <typename T> std::vector<T*> getDevicesByType() {
        static_assert(std::is_base_of<IDevice, T>::value, "T must derive from IDevice");
        std::vector<T*> result;
        for (const auto& [did, dev] : devices_map) {
            if (auto casted = dynamic_cast<T*>(dev.get())) {
                result.push_back(casted);
            }
        }
        return result;
    }
    ActionGroup* getActionGroupByAid(uint16_t aid);
    std::vector<ActionGroup*> getAllModeActionGroup();
    std::vector<ChannelInput*> getAllChannelInputByChannelNum(uint8_t channel_num);// 返回所有指定channel的实例
    ChannelInput* getAliveChannel();
    Panel* getPanelByPid(uint8_t pid);

    // ================ 心跳包 ================
    std::array<uint8_t, 8> getHeartbeatCode() const { return heartbeat_code; }
    void useAliveHeartBeat() { ESP_LOGI("HEARTBEAT", "切换至插卡心跳"); heartbeat_code = alive_heartbeat_code; }
    void useSleepHeartBeat() { ESP_LOGI("HEARTBEAT", "切换至睡眠心跳"); heartbeat_code = sleep_heartbeat_code; }

    // ================ 按键面板 ================
    void handlePanel(uint8_t panel_id, uint8_t target_buttons, uint8_t old_bl_state);
    void handleDimming(uint8_t panel_id, uint8_t target_buttons, uint8_t brightness);
    void wishIndicatorAllPanel(bool state);   // 希望操作所有面板的指示灯
    
    // ================ 语音指令 ================
    void handleVoiceCmd(uint8_t* code_data);
    
    // ================ 空调 ================
    void updateAirState(uint8_t states, uint8_t temps);
    void updateRoomTemp(uint8_t air_id, uint8_t room_temp);
    
    // ================ 获得物理上的继电器与干接点输入状态 ================
    void syncAllRelayPhysicsOnoff();
    void updateRelayPhysicsState(uint8_t channel, uint8_t is_on);
    bool readRelayPhysicsState(uint8_t channel);
    void syncAllDrycontactInputPhysicsOnoff();
    void updateDrycontactInputPhysicsState(uint8_t channel, uint8_t is_on);
    bool readDrycontactInputPhysicsState(uint8_t channel);
    
    // ================ 门与红外 ================
    void onDoorOpened();
    void onDoorClosed();
    uint64_t last_presence_time = 0;    // 最近一次确认“有人”活动的时刻
    uint64_t last_door_open_time = 0;   // 最近一次门被打开的时刻
    uint64_t last_door_close_time = 0;  // 最近一次门被关上的时刻
    bool door_open = false;

    bool useDayNight = false;           // 是否启用昼夜模式, 影响红外时长是否翻倍
    int dayTimePoint = 7;               // 昼间开始时间, 小时
    int nightTimePoint = 19;            // 夜间开始时间, 小时

    bool ignoreInfrared = false;        // 现在是否忽略红外输入
    
    // ================ 其实是杂项 ================
    inline bool getAlive() const { return the_rcu_is_alive; }
    void setAlive(bool state);
    inline bool isSleep() const { return heartbeat_code == sleep_heartbeat_code; }
    const std::string& getLastModeName() const { return last_mode_name; }

    void setAnyKeyActionGroup(uint16_t aid) { any_key_execute_action_group_id = aid; }
    void clearAnyKeyActionGroup() { any_key_execute_action_group_id = -1; }
    bool execute_any_key_action_group();

    uint64_t last_action_group_time = esp_timer_get_time() / 1000ULL;

    void clearAll();

private:
    LordManager() {
        relay_physics_map_mutex = xSemaphoreCreateMutex();
        assert(relay_physics_map_mutex);
        drycontactInput_physics_map_mutex = xSemaphoreCreateMutex();
        assert(drycontactInput_physics_map_mutex);
    }

    bool the_rcu_is_alive = false;  // 非常高的地位, 作为插拔卡的标志位
    std::array<uint8_t, 8> heartbeat_code = sleep_heartbeat_code;        // 不停发的心跳包, 不停地
    int any_key_execute_action_group_id = -1;   // 任意键执行的动作组id
    std::string config_generate_time;
    std::string last_mode_name;
    std::unordered_map<uint16_t, std::unique_ptr<IDevice>> devices_map;             // did, device
    std::unordered_map<uint16_t, std::unique_ptr<ActionGroup>> action_groups_map;   // aid, action_group
    std::unordered_map<uint16_t, std::unique_ptr<ChannelInput>> channel_inputs_map; // iid, channel_input
    std::unordered_map<uint8_t, std::unique_ptr<Panel>> panels_map;                 // pid, panel       // 不使用iid, panel是包装类, 里边的PanelButtonInput才是与ChannelInput同辈分的类
    std::unordered_map<uint8_t, std::unique_ptr<VoiceCommand>> voice_cmds_map;      // iid, voice_cmd

    SemaphoreHandle_t relay_physics_map_mutex;
    std::unordered_map<uint8_t, bool> relay_physics_map;                            // channel, state   // 继电器物理通断状态
    SemaphoreHandle_t drycontactInput_physics_map_mutex;
    std::unordered_map<uint8_t, bool> drycontactInput_physics_map;                  // channel, state   // 干接点输入物理通断状态
};