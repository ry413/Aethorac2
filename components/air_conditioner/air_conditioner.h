#pragma once

#include <map>
#include <memory>
#include <atomic>
#include "idevice.h"

enum class ACFanSpeed : uint8_t {
    LOW,
    MEDIUM,
    HIGH,
    AUTO,
};

enum class ACMode: uint8_t {
    COOLING,
    HEATING,
    FAN,
};

enum class ACType: uint8_t {
    SINGLE_PIPE_FCU,
    INFRARED,
    DOUBLE_PIPE_FCU,
};

enum class ACStopAction: uint8_t {
    CLOSE_ALL,
    CLOSE_VALVE,
    CLOSE_FAN,
    CLOSE_NONE
};


// 空调管理者
// 不掌管空调, 只是操控
class AirConManager {
public:
    static AirConManager& getInstance() {
        static AirConManager instance;
        return instance;
    }

    AirConManager(const AirConManager&) = delete;
    AirConManager& operator=(const AirConManager&) = delete;

    // 空调的全局设置
    uint8_t default_target_temp;        // 默认目标温度
    ACMode default_mode;                // 默认模式
    ACFanSpeed default_fan_speed;       // 默认风速
    uint8_t stop_threshold;             // 超出目标温度后停止工作的阈值
    uint8_t rework_threshold;           // 回温后重新开始工作的阈值
    ACStopAction stop_action;           // 达到目标温度停止工作应该怎么停
    bool remove_card_air_usable;        // 拔卡时是否允许操作空调
    uint8_t low_diff;                   // 风速: 自动时, 进入低风所需小于等于的温差
    uint8_t high_diff;                  // 温差大于等于以进入高风
    ACFanSpeed auto_fun_wind_speed;     // (风速: AUTO, 模式: 通风) 这个状态时, 应该开什么风速
private:
    AirConManager() = default;
};

class AirConBase : public IDevice {
public:
    AirConBase(uint16_t did, const std::string& name, const std::string&carry_state, uint8_t ac_id, DeviceType dev_type, ACType ac_type)
        : IDevice(did, dev_type, name, carry_state), ac_id(ac_id), ac_type(ac_type) {
            auto& airConManager = AirConManager::getInstance();
            target_temp.store(airConManager.default_target_temp);
            fan_speed.store(airConManager.default_fan_speed);
            mode.store(airConManager.default_mode);
        }
    
    virtual void update_state(uint8_t state, uint8_t temps);
    virtual void execute_backstage(std::string operation, std::string parameter) = 0;

    uint8_t getAcId() const { return ac_id; }
    ACMode get_mode() const { return mode.load(); }
    ACFanSpeed get_fan_speed() const { return fan_speed.load(); }
    uint8_t get_target_temp() const { return target_temp.load(); }
    uint8_t get_room_temp() const { return room_temp.load(); }
    bool get_state() const { return is_running.load(); }
    void update_room_temp(uint8_t temp) { room_temp.store(temp); }

protected:
    uint8_t ac_id;
    ACType  ac_type;
    std::atomic<ACMode> mode{ACMode::COOLING};          // 当前模式
    std::atomic<ACFanSpeed> fan_speed{ACFanSpeed::LOW}; // 当前风速
    std::atomic<uint8_t> target_temp{26};               // 设置的目标温度
    std::atomic<uint8_t> room_temp{26};                 // 把室温也维护起来, 反正会持续更新
    std::atomic<bool> is_running{false};                // 空调是否开启
    std::atomic<bool> is_work{false};                   // 空调是否在制冷/制热

    virtual void power_off();
};

// 单管空调
class SinglePipeFCU : public AirConBase {
public:
    SinglePipeFCU(uint16_t did, const std::string& name, const std::string&carry_state,
                  uint8_t ac_id, uint8_t water1_ch, uint8_t low_ch, uint8_t mid_ch, uint8_t high_ch)
        : AirConBase(did, name, carry_state, ac_id, DeviceType::SINGLE_AIR, ACType::SINGLE_PIPE_FCU),
          water1_channel(water1_ch), low_channel(low_ch), mid_channel(mid_ch), high_channel(high_ch) {}

    void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
    void execute_backstage(std::string operation, std::string parameter) override;
    void update_state(uint8_t state, uint8_t temps) override;
    void sync_states();

protected:
    uint8_t water1_channel;
    uint8_t low_channel;
    uint8_t mid_channel;
    uint8_t high_channel;

private:
    void power_off() override;
    void adjust_relay_states();
    void adjust_fan_relay();
    void open_fan_relay(ACFanSpeed speed);
    void stop_on_reached_target();
};

// 双管空调(暂不实现)
class DoublePipeFCU : public SinglePipeFCU {
public:
    uint8_t water2_channel;
};

// 红外空调
class InfraredAC : public AirConBase {
public:
    InfraredAC(uint16_t did, const std::string& name, const std::string&carry_state, uint8_t ac_id)
        : AirConBase(did, name, carry_state, ac_id, DeviceType::INFRARED_AIR, ACType::INFRARED) {}
    void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
    void execute_backstage(std::string operation, std::string parameter) override;
    void update_state(uint8_t state, uint8_t temps) override;
    void sync_states();
    void set_code_base(std::string code_base);
    std::string get_code_base() const;

private:
    std::string code_base;
};