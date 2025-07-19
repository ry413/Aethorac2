#pragma once

#include <map>
#include <memory>
#include <atomic>
#include "idevice.h"

enum class ACFanSpeed : int {
    LOW,
    MEDIUM,
    HIGH,
    AUTO,
};

enum class ACMode: int {
    COOLING,
    HEATING,
    FAN,
};

enum class ACType: int {
    SINGLE_PIPE_FCU,
    INFRARED,
    DOUBLE_PIPE_FCU,
};

enum class ACStopAction: int {
    CLOSE_ALL,
    CLOSE_VALVE,
    CLOSE_FAN,
    CLOSE_NONE
};

class BoardOutput;

class AirConBase : public IDevice {
public:
    int ac_id;
    ACType  ac_type;

    AirConBase();

    virtual void update_state(int state, int temps);
    virtual void execute_backstage(std::string operation, std::string parameter) = 0;

    ACMode get_mode();
    ACFanSpeed get_fan_speed();
    int get_target_temp();
    int get_room_temp();
    bool get_state();
    void update_room_temp(int temp);

protected:
    std::atomic<ACMode> mode{ACMode::COOLING};          // 当前模式
    std::atomic<ACFanSpeed> fan_speed{ACFanSpeed::LOW}; // 当前风速
    std::atomic<int> target_temp{26};               // 设置的目标温度
    std::atomic<int> room_temp{26};                 // 把室温也维护起来, 反正会持续更新
    std::atomic<bool> is_running{false};                // 空调是否开启
    std::atomic<bool> is_work{false};                   // 空调是否在制冷/制热

    virtual void power_off();
};

// 单管空调
class SinglePipeFCU : public AirConBase {
public:
    std::shared_ptr<BoardOutput> low_output;
    std::shared_ptr<BoardOutput> mid_output;
    std::shared_ptr<BoardOutput> high_output;
    std::shared_ptr<BoardOutput> water1_output;

    void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
    void execute_backstage(std::string operation, std::string parameter) override;
    void update_state(int state, int temps) override;
    void sync_states();

private:
    void power_off() override;
    void adjust_relay_states();
    void adjust_fan_relay();
    void open_fan_relay(ACFanSpeed speed);
    void stop_on_reached_target();
};

// 双管空调
class DoublePipeFCU : public SinglePipeFCU {
public:
    std::shared_ptr<BoardOutput> water2_channel;
};

// 红外空调
class InfraredAC : public AirConBase {
public:
    void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
    void execute_backstage(std::string operation, std::string parameter) override;
    void update_state(int state, int temps) override;
    void sync_states();
    void set_code_base(std::string code_base);
    std::string get_code_base() const;

private:
    std::string code_base;
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
    int default_target_temp;        // 默认目标温度
    ACMode default_mode;                // 默认模式
    ACFanSpeed default_fan_speed;       // 默认风速
    int stop_threshold;             // 超出目标温度后停止工作的阈值
    int rework_threshold;           // 回温后重新开始工作的阈值
    ACStopAction stop_action;           // 达到目标温度停止工作应该怎么停
    bool remove_card_air_usable;        // 拔卡时是否允许操作空调
    int low_diff;                   // 风速: 自动时, 进入低风所需小于等于的温差
    int high_diff;                  // 温差大于等于以进入高风
    ACFanSpeed auto_fun_wind_speed;     // (风速: AUTO, 模式: 通风) 这个状态时, 应该开什么风速

    // 更新指定空调的状态, 空调id在state中
    void update_states(int state, int temps);

    // 红外温感上报室温
    void update_room_temp(int ac_id, int temp);

private:
    AirConManager() = default;
};