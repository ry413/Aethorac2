#pragma once

#include <map>
#include <memory>
#include <set>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "action_group.h"
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


// 空调的全局配置, 就只是储存一些数据
class AirConGlobalConfig {
public:
    static AirConGlobalConfig& getInstance() {
        static AirConGlobalConfig instance;
        return instance;
    }

    AirConGlobalConfig(const AirConGlobalConfig&) = delete;
    AirConGlobalConfig& operator=(const AirConGlobalConfig&) = delete;

    uint8_t default_target_temp = 26;                           // 默认目标温度
    ACMode default_mode = ACMode::COOLING;                      // 默认模式
    ACFanSpeed default_fan_speed = ACFanSpeed::MEDIUM;          // 默认风速
    uint8_t stop_threshold = 1;                                 // 超出目标温度后停止工作的阈值
    uint8_t rework_threshold = 1;                               // 回温后重新开始工作的阈值
    ACStopAction stop_action = ACStopAction::CLOSE_ALL;         // 达到目标温度停止工作应该怎么停
    bool remove_card_air_usable = false;                        // 拔卡时是否允许操作空调
    uint8_t low_diff = 2;                                       // [风速: 自动]时, 进入低风所需小于等于的温差
    uint8_t high_diff = 4;                                      // [风速: 自动]时, 进入高风所需大于等于的温度
    ACFanSpeed auto_fun_wind_speed = ACFanSpeed::MEDIUM;        // (风速: AUTO, 模式: 通风) 这个状态时, 应该开什么风速
    uint16_t shutdown_after_duration = 30;                      // 盘管空调关机后还要吹的时长
    ACFanSpeed shutdown_after_fan_speed = ACFanSpeed::MEDIUM;   // 盘管空调关机后还要吹的风速

    esp_err_t load();
    esp_err_t save();

    std::set<uint8_t> air_ids;                                  // 所有存在的空调ID

private:
    AirConGlobalConfig() = default;

    static constexpr const char* NS  = "aircon";
    static constexpr const char* KEY = "cfg";

    // 紧凑的存盘结构（整块 blob）
    struct Blob {
        uint8_t default_target_temp;
        uint8_t default_mode;              // enum 存为 uint8_t
        uint8_t default_fan_speed;
        uint8_t stop_threshold;
        uint8_t rework_threshold;
        uint8_t stop_action;
        uint8_t remove_card_air_usable;    // bool -> uint8_t
        uint8_t low_diff;
        uint8_t high_diff;
        uint8_t auto_fun_wind_speed;
        uint16_t shutdown_after_duration;
        uint8_t shutdown_after_fan_speed;
    };

    Blob toBlob() const;
    void fromBlob(const Blob& b);
};

class AirConBase : public IDevice {
public:
    AirConBase(uint16_t did, const std::string& name, const std::string&carry_state, uint8_t ac_id, DeviceType dev_type, ACType ac_type)
        : IDevice(did, dev_type, name, carry_state), ac_id(ac_id), ac_type(ac_type) {
            auto& airConManager = AirConGlobalConfig::getInstance();
            target_temp.store(airConManager.default_target_temp);
            fan_speed.store(airConManager.default_fan_speed);
            mode.store(airConManager.default_mode);
        }
    void addAssBtn(PanelButtonPair) override { ESP_LOGW("AirConBase", "空调不应该添加关联按钮"); }
    void syncAssBtnToDevState() override { ESP_LOGW("AirConBase", "空调不应该有关联按钮"); }
    bool isOn() const override { return is_running.load(); }
    void updateButtonIndicator(bool state) override { ESP_LOGW("AirConBase", "空调不应该调用这个函数"); }

    virtual void update_state(uint8_t state, uint8_t temps);
    virtual void sync_states() = 0;

    uint8_t getAcId() const { return ac_id; }
    ACMode get_mode() const { return mode.load(); }
    ACFanSpeed get_fan_speed() const { return fan_speed.load(); }
    uint8_t get_target_temp() const { return target_temp.load(); }
    uint8_t get_room_temp() const { return room_temp.load(); }
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
          water1_channel(water1_ch), low_channel(low_ch), mid_channel(mid_ch), high_channel(high_ch) {
            if (!shutdown_after_timer) {
                shutdown_after_timer = xTimerCreate(
                    "shutdownAfterTimer",
                    pdMS_TO_TICKS(AirConGlobalConfig::getInstance().shutdown_after_duration * 1000),
                    pdFALSE,
                    this,
                    staticShutdownAfterTimerCallback
                );
            }
        }
    ~SinglePipeFCU() {
        if (shutdown_after_timer != nullptr) {
            xTimerStop(shutdown_after_timer, 0);
            xTimerDelete(shutdown_after_timer, 0);
            shutdown_after_timer = nullptr;
        }
    }

    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
    void update_state(uint8_t state, uint8_t temps) override;
    void sync_states() override;

protected:
    uint8_t water1_channel;
    uint8_t low_channel;
    uint8_t mid_channel;
    uint8_t high_channel;

private:
    TimerHandle_t shutdown_after_timer = nullptr;
    static void staticShutdownAfterTimerCallback(TimerHandle_t xTimer);
    void shutdownAfterTimerCallback(TimerHandle_t xTimer);

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
    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
    void update_state(uint8_t state, uint8_t temps) override;
    void sync_states() override;

    void set_code_base(std::string code_base);
    std::string get_code_base() const;

private:
    std::string code_base;
};