#include <iostream>
#include <cstdint>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "air_conditioner.h"
#include "lord_manager.h"
#include "rs485_comm.h"
#include "commons.h"
#include "stm32_tx.h"

#define TAG "AIR_CON"

static ACMode bitsToMode(uint8_t mode_bits);
static ACFanSpeed bitsToFanSpeed(uint8_t fan_speed_bits);
static uint8_t modeToBits(ACMode mode);
static uint8_t fanSpeedToBits(ACFanSpeed fan_speed);
static std::string modeBitsToName(uint8_t mode_bits);
static std::string fanSpeedBitsToName(uint8_t fan_speed_bits);

// 用户操作温控器来触发
void AirConBase::update_state(uint8_t state, uint8_t temps) {
    uint8_t power = (state >> 7) & 0x01;                    // 空调开关
    uint8_t mode_bits = (state >> 5) & 0x03;                // 空调模式
    uint8_t fan_speed_bits = (state >> 3) & 0x03;           // 空调风速

    uint8_t target_temp_val = ((temps >> 4) & 0x0F) + 16;   // 目标温度
    uint8_t room_temp_val = ((temps >> 0) & 0x0F) + 16;     // 室温

    room_temp.store(room_temp_val);

    // 拔卡状态并且拔卡时不允许操作空调
    if (!LordManager::instance().getAlive() && !AirConGlobalConfig::getInstance().remove_card_air_usable) { 
        return;
    }

    // 上报空调操作
    if (power == 0x00 && is_running.load() == true) {
        add_log_entry("air", did, "关闭", "");        // 因为是用户操控温控器面板, 所以传true让它记录日志
    } else if (power == 0x01 && is_running.load() == false) {
        add_log_entry("air", did, "打开", "");
    }

    if (mode.load() != bitsToMode(mode_bits)) {
        add_log_entry("air", did, modeBitsToName(mode_bits), "");
    }

    if (fan_speed.load() != bitsToFanSpeed(fan_speed_bits)) {
        add_log_entry("air", did, fanSpeedBitsToName(fan_speed_bits), "");
    }

    if (target_temp_val != target_temp.load()) {
        add_log_entry("air", did, "调整至" + std::to_string(target_temp_val) + "度", "");
    }

    // 更新模式
    mode.store(bitsToMode(mode_bits));

    // 更新风速
    fan_speed.store(bitsToFanSpeed(fan_speed_bits));

    // 温度
    target_temp.store(target_temp_val);
    
}

// 另一种操控空调的方式, 比如语音控制, 后台控制
void SinglePipeFCU::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    add_log_entry("air", did, operation, parameter, should_log);
    ESP_LOGI_CYAN(TAG, "空调[%s] 收到操作[%s] param[%s]", name.c_str(), operation.c_str(), parameter.c_str());

    // 直接处理关闭
    if (operation == "关" || operation == "关闭") {
        power_off();
        sync_states();
        return;
    }

    // 否则固定进入on状态, 并把可能存在的"shutdownAfter"定时器关掉
    is_running.store(true);
    xTimerStop(shutdown_after_timer, 0);

    // 这种方式打开空调时, 固定使用全局默认配置的目标温度和风速
    if (operation == "开" || operation == "打开") {
        target_temp.store(AirConGlobalConfig::getInstance().default_target_temp);
        fan_speed.store(AirConGlobalConfig::getInstance().default_fan_speed);
        mode.store(AirConGlobalConfig::getInstance().default_mode);
    } else if (operation == "制冷") {
        mode.store(ACMode::COOLING);
    } else if (operation == "制热") {
        mode.store(ACMode::HEATING);
    } else if (operation == "通风") {
        mode.store(ACMode::FAN);
    } else if (operation == "高风") {
        fan_speed.store(ACFanSpeed::HIGH);
    } else if (operation == "中风") {
        fan_speed.store(ACFanSpeed::MEDIUM);
    } else if (operation == "低风") {
        fan_speed.store(ACFanSpeed::LOW);
    } else if (operation == "自动") {
        fan_speed.store(ACFanSpeed::AUTO);
    } else if (operation == "风量加大") {
        if (fan_speed.load() == ACFanSpeed::LOW) {
            fan_speed.store(ACFanSpeed::MEDIUM);
        } else if (fan_speed.load() == ACFanSpeed::MEDIUM) {
            fan_speed.store(ACFanSpeed::HIGH);
        }
    } else if (operation == "风量减小") {
        if (fan_speed.load() == ACFanSpeed::HIGH) {
            fan_speed.store(ACFanSpeed::MEDIUM);
        } else if (fan_speed.load() == ACFanSpeed::MEDIUM) {
            fan_speed.store(ACFanSpeed::LOW);
        }
    } else if (operation == "温度升高") {
        uint8_t current_temp = target_temp.load();
        if (current_temp < 31) {
            target_temp.store(current_temp + 1);
        }
    } else if (operation == "温度降低") {
        uint8_t current_temp = target_temp.load();
        if (current_temp > 16) {
            target_temp.store(current_temp - 1);
        }
    } else if (operation == "调节温度") {
        int temp = std::stoi(parameter);
        
        temp = std::max(16, std::min(temp, 31));

        ESP_LOGI(TAG, "调节温度至%d\n", temp);
        target_temp.store(static_cast<uint8_t>(temp));
    }

    adjust_relay_states();
    sync_states();
}

// 处理温控器持续上报来的状态
void SinglePipeFCU::update_state(uint8_t state, uint8_t temps) {
    AirConBase::update_state(state, temps);
    
    if (!LordManager::instance().getAlive() && !AirConGlobalConfig::getInstance().remove_card_air_usable) { 
        return;
    }

    if (((state >> 7) & 0x01) == 0x00) {
        power_off();
    } else {
        is_running.store(true);
        xTimerStop(shutdown_after_timer, 0);
        adjust_relay_states();
    }
    sync_states();
}

// 同步状态给温控器
void SinglePipeFCU::sync_states() {
    uint8_t state = 0;
    uint8_t mode_bits = modeToBits(mode.load());
    uint8_t fan_speed_bits = fanSpeedToBits(fan_speed.load());

    state |= (is_running.load() & 0x01) << 7;
    state |= (mode_bits & 0x03) << 5;
    state |= (fan_speed_bits & 0x03) << 3;
    state |= (ac_id & 0x03) << 0;

    uint8_t temps = 0;
    temps |= ((target_temp.load() - 16) & 0x0F) << 4;
    temps |= ((room_temp.load() - 16) & 0x0F) << 0;

    generate_response(AIR_CON, AIR_CON_CONTROL, 0x00, state, temps);
}

void SinglePipeFCU::staticShutdownAfterTimerCallback(TimerHandle_t xTimer) {
    void* pv = pvTimerGetTimerID(xTimer);
    if (SinglePipeFCU* pThis = static_cast<SinglePipeFCU*>(pv)) {
        pThis->shutdownAfterTimerCallback(xTimer);
    }
}

void SinglePipeFCU::shutdownAfterTimerCallback(TimerHandle_t xTimer) {
    controlRelay(mid_channel, false);
    controlRelay(high_channel, false);
    controlRelay(low_channel, false);
}

void SinglePipeFCU::power_off() {
    AirConBase::power_off();
    
    // 固定关闭水阀
    controlRelay(water1_channel, false);
    // 关闭除了目标之外的风机
    auto& air_configs = AirConGlobalConfig::getInstance();
    switch (air_configs.shutdown_after_fan_speed) {
        case ACFanSpeed::LOW:
            controlRelay(low_channel, true);
            controlRelay(mid_channel, false);
            controlRelay(high_channel, false);
            break;
        case ACFanSpeed::MEDIUM:
            controlRelay(low_channel, false);
            controlRelay(mid_channel, true);
            controlRelay(high_channel, false);
            break;
        case ACFanSpeed::HIGH:
            controlRelay(low_channel, false);
            controlRelay(mid_channel, false);
            controlRelay(high_channel, true);
            break;
        default:
            ESP_LOGE(TAG, "shutdown_after_fan_speed错误: %d", (int)air_configs.shutdown_after_fan_speed);
            break;
    }

    xTimerStart(shutdown_after_timer, 0);

}

// 根据当前温度差值, 开关水阀
void SinglePipeFCU::adjust_relay_states() {
    auto& airConManager = AirConGlobalConfig::getInstance();
    uint8_t target_temp_value = target_temp.load();

    switch (mode.load()) {
        case ACMode::COOLING:
            // 正在制冷时, 直到室温低于目标温度 - 阈值才停止
            if (is_work.load()) {
                if (room_temp.load() <= target_temp_value - airConManager.stop_threshold) {
                    ESP_LOGI(TAG, "室温[%d] <= (目标温度[%d] - 阈值[%d]), 停止制冷\n", room_temp.load(), target_temp_value, airConManager.stop_threshold);
                    stop_on_reached_target();
                    is_work.store(false);
                } else {
                    ESP_LOGI(TAG, "室温[%d] > (目标温度[%d] - 阈值[%d]), 保持制冷\n", room_temp.load(), target_temp_value, airConManager.stop_threshold);
                    adjust_fan_relay();
                }
            }
            // 当不在制冷时, 直到室温高于目标温度 + 阈值才开始工作
            else {
                if (room_temp.load() >= target_temp_value + airConManager.rework_threshold) {
                    ESP_LOGI(TAG, "室温[%d] >= (目标温度[%d] - 阈值[%d]), 开始制冷\n", room_temp.load(), target_temp_value, airConManager.stop_threshold);
                    controlRelay(water1_channel, true);
                    adjust_fan_relay();
                    is_work.store(true);
                } else {
                    ESP_LOGI(TAG, "室温[%d] < (目标温度[%d] + 阈值[%d]), 拒绝制冷\n", room_temp.load(), target_temp_value, airConManager.stop_threshold);
                }
            }
            break;
        case ACMode::HEATING:
            // 正在制热时, 直到室温高于目标温度 + 阈值才停止
            if (is_work.load()) {
                if (room_temp.load() >= target_temp_value + airConManager.stop_threshold) {
                    ESP_LOGI(TAG, "室温[%d] >= (目标温度[%d] + 阈值[%d]), 停止制热\n", room_temp.load(), target_temp_value, airConManager.stop_threshold);
                    stop_on_reached_target();
                    is_work.store(false);
                } else {
                    ESP_LOGI(TAG, "室温[%d] < (目标温度[%d] + 阈值[%d]), 保持制热\n", room_temp.load(), target_temp_value, airConManager.stop_threshold);
                    adjust_fan_relay();
                }
            }
            // 当不在制热时, 直到室温低于目标温度 - 阈值才开始工作
            else {
                if (room_temp.load() <= target_temp_value - airConManager.stop_threshold) {
                    ESP_LOGI(TAG, "室温[%d] <= (目标温度[%d] - 阈值[%d]), 开始制热\n", room_temp.load(), target_temp_value, airConManager.stop_threshold);
                    controlRelay(water1_channel, true);
                    adjust_fan_relay();
                    is_work.store(true);
                } else {
                    ESP_LOGI(TAG, "室温[%d] > (目标温度[%d] + 阈值[%d]), 拒绝制热\n", room_temp.load(), target_temp_value, airConManager.stop_threshold);
                }
            }
            break;
        // 通风模式则关闭水阀, 只根据风速调整风速继电器
        case ACMode::FAN:
            controlRelay(water1_channel, false);
            adjust_fan_relay();
            break;
    }
}

// 根据当前温度差值, 调节风机
void SinglePipeFCU::adjust_fan_relay() {
    auto& airConManager = AirConGlobalConfig::getInstance();

    // 如果是自动风
    if (fan_speed.load() == ACFanSpeed::AUTO) {
        // 同时又是通风模式, 则使用全局配置指定的风速
        if (mode.load() == ACMode::FAN) {
            open_fan_relay(airConManager.auto_fun_wind_speed);
        }
        // 否则就是制冷/制热, 那就根据温差调节风速
        else {
            int temp_diff = abs(target_temp.load() - room_temp.load());

            if (temp_diff <= airConManager.low_diff) {
                open_fan_relay(ACFanSpeed::LOW);
            } else if (temp_diff >= airConManager.high_diff) {
                open_fan_relay(ACFanSpeed::HIGH);
            } else {
                open_fan_relay(ACFanSpeed::MEDIUM);
            }
        }
    }
    // 如果不是自动风, 那就直接设置对应的风速
    else {
        open_fan_relay(fan_speed.load());
    }
}

// 入参不应该是AUTO. 理应同时只能有一个风机打开
void SinglePipeFCU::open_fan_relay(ACFanSpeed speed) {
    switch (speed) {
        case ACFanSpeed::LOW:
            controlRelay(low_channel, true);
            controlRelay(mid_channel, false);
            controlRelay(high_channel, false);
            ESP_LOGI(TAG, "风速设置为低风");
            break;
        case ACFanSpeed::MEDIUM:
            controlRelay(low_channel, false);
            controlRelay(mid_channel, true);
            controlRelay(high_channel, false);
            ESP_LOGI(TAG, "风速设置为中风");
            break;
        case ACFanSpeed::HIGH:
            controlRelay(low_channel, false);
            controlRelay(mid_channel, false);
            controlRelay(high_channel, true);
            ESP_LOGI(TAG, "风速设置为高风");
            break;
        case ACFanSpeed::AUTO:
            ESP_LOGE(TAG, "不应该传入AUTO");
            break;
        default:
            ESP_LOGE(TAG, "未知的风速值: %d", static_cast<int>(speed));
            break;
    }
}

// 达到目标温度后的停止动作
void SinglePipeFCU::stop_on_reached_target() {
    auto stop_action = AirConGlobalConfig::getInstance().stop_action;

    switch (stop_action) {
        case ACStopAction::CLOSE_ALL:
            controlRelay(low_channel, false);
            controlRelay(mid_channel, false);
            controlRelay(high_channel, false);
            controlRelay(water1_channel, false);
            break;
        case ACStopAction::CLOSE_FAN:
            controlRelay(low_channel, false);
            controlRelay(mid_channel, false);
            controlRelay(high_channel, false);
            break;
        case ACStopAction::CLOSE_VALVE:
            controlRelay(water1_channel, false);
            break;
        case ACStopAction::CLOSE_NONE:
        default:
            break;
    }
}

void AirConBase::power_off() {
    is_work.store(false);
    is_running.store(false);
    ESP_LOGI(TAG, "空调%d已关闭", ac_id);
}

void InfraredAC::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    add_log_entry("air", did, operation, parameter, should_log);
    ESP_LOGI_CYAN(TAG, "空调[%s] 收到操作[%s]", name.c_str(), operation.c_str());

    if (operation == "关" || operation == "关闭") {
        power_off();
        sync_states();
        return;
    }

    is_running.store(true);
    if (operation == "开" || operation == "打开") {
        target_temp.store(AirConGlobalConfig::getInstance().default_target_temp);
        fan_speed.store(AirConGlobalConfig::getInstance().default_fan_speed);
        mode.store(AirConGlobalConfig::getInstance().default_mode);
    } else if (operation == "制冷") {
        mode.store(ACMode::COOLING);
    } else if (operation == "制热") {
        mode.store(ACMode::HEATING);
    } else if (operation == "通风") {
        mode.store(ACMode::FAN);
    } else if (operation == "高风") {
        fan_speed.store(ACFanSpeed::HIGH);
    } else if (operation == "中风") {
        fan_speed.store(ACFanSpeed::MEDIUM);
    } else if (operation == "低风") {
        fan_speed.store(ACFanSpeed::LOW);
    } else if (operation == "自动") {
        fan_speed.store(ACFanSpeed::AUTO);
    } else if (operation == "风量加大") {
        if (fan_speed.load() == ACFanSpeed::LOW) {
            fan_speed.store(ACFanSpeed::MEDIUM);
        } else if (fan_speed.load() == ACFanSpeed::MEDIUM) {
            fan_speed.store(ACFanSpeed::HIGH);
        }
    } else if (operation == "风量减小") {
        if (fan_speed.load() == ACFanSpeed::HIGH) {
            fan_speed.store(ACFanSpeed::MEDIUM);
        } else if (fan_speed.load() == ACFanSpeed::MEDIUM) {
            fan_speed.store(ACFanSpeed::LOW);
        }
    } else if (operation == "温度升高") {
        uint8_t current_temp = target_temp.load();
        if (current_temp < 31) {
            target_temp.store(current_temp + 1);
        }
    } else if (operation == "温度降低") {
        uint8_t current_temp = target_temp.load();
        if (current_temp > 16) {
            target_temp.store(current_temp - 1);
        }
    } else if (operation == "调节温度") {
        int temp = std::stoi(parameter);
        
        temp = std::max(16, std::min(temp, 31));
        
        ESP_LOGI(TAG, "调节温度至%d\n", temp);
        target_temp.store(static_cast<uint8_t>(temp));
    }

    sync_states();
}

void InfraredAC::update_state(uint8_t state, uint8_t temps) {
    AirConBase::update_state(state, temps);
    
    if (!LordManager::instance().getAlive() && !AirConGlobalConfig::getInstance().remove_card_air_usable) { 
        return;
    }
    
    if (((state >> 7) & 0x01) == 0x00) {
        power_off();
    } else {
        is_running.store(true);
    }
    sync_states();
}

void InfraredAC::sync_states() {
    uint8_t state = 0;
    uint8_t mode_bits = modeToBits(mode.load());
    uint8_t fan_speed_bits = fanSpeedToBits(fan_speed.load());

    state |= (is_running.load() & 0x01) << 7;
    state |= (mode_bits & 0x03) << 5;
    state |= (fan_speed_bits & 0x03) << 3;
    state |= (ac_id & 0x03) << 0;

    uint8_t temps = 0;
    temps |= ((target_temp.load() - 16) & 0x0F) << 4;
    temps |= ((room_temp.load() - 16) & 0x0F) << 0;

    generate_response(AIR_CON, AIR_CON_CONTROL, 0x00, state, temps);
    // generate_response(AIR_CON, 0x08, 0x00, state, temps);
}

void InfraredAC::set_code_base(std::string code_base) {
    this->code_base = code_base;
    uint8_t code1;
    uint8_t code2;
    if (code_base == "gree") {
        code1 = 0x03;
        code2 = 0x3E;
    }
    generate_response(INFRARED_CONTEROLLER, 0x12, 0x00, code1, code2);
}

std::string InfraredAC::get_code_base() const { return code_base; }

static ACMode bitsToMode(uint8_t mode_bits) {
    switch (mode_bits) {
        case 0x00: return ACMode::COOLING;
        case 0x01: return ACMode::HEATING;
        case 0x03: return ACMode::FAN;
        default: 
            ESP_LOGE(TAG, "未知模式位: 0x%2x", mode_bits);
            return ACMode::COOLING;
    }
}

static ACFanSpeed bitsToFanSpeed(uint8_t fan_speed_bits) {
    switch (fan_speed_bits) {
        case 0x00: return ACFanSpeed::LOW;
        case 0x01: return ACFanSpeed::MEDIUM;
        case 0x02: return ACFanSpeed::HIGH;
        case 0x03: return ACFanSpeed::AUTO;
        default: 
            ESP_LOGE(TAG, "未知风速位: 0x%2x", fan_speed_bits);
            return ACFanSpeed::LOW;
    }
}

static uint8_t modeToBits(ACMode mode) {
    switch (mode) {
        case ACMode::COOLING: return 0x00;
        case ACMode::HEATING: return 0x01;
        case ACMode::FAN:     return 0x03;
        default:
            ESP_LOGE(TAG, "未知模式: %d", static_cast<int>(mode));
            return 0x00;
    }
}

static uint8_t fanSpeedToBits(ACFanSpeed fan_speed) {
    switch (fan_speed) {
        case ACFanSpeed::LOW:    return 0x00;
        case ACFanSpeed::MEDIUM: return 0x01;
        case ACFanSpeed::HIGH:   return 0x02;
        case ACFanSpeed::AUTO:   return 0x03;
        default:
            ESP_LOGE(TAG, "未知风速: %d", static_cast<int>(fan_speed));
            return 0x00;
    }
}

static std::string modeBitsToName(uint8_t mode_bits) {
    switch (mode_bits) {
        case 0x00: return "制冷";
        case 0x01: return "制热";
        case 0x03: return "通风";
        default:
            ESP_LOGE(TAG, "未知模式位: 0x%2x", mode_bits);
            return "未知模式";
    }
}

static std::string fanSpeedBitsToName(uint8_t fan_speed_bits) {
    switch (fan_speed_bits) {
        case 0x00: return "低风";
        case 0x01: return "中风";
        case 0x02: return "高风";
        case 0x03: return "自动";
        default:
            ESP_LOGE(TAG, "未知风速位: 0x%2x", fan_speed_bits);
            return "未知风速";
    }
}