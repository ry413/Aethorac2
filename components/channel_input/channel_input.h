#pragma once

#include <memory>
#include <string>
#include <esp_log.h>
#include "enums.h"
#include "action_group.h"
#include "iinput.h"
#include "freertos/timers.h"

class ChannelInput : public InputBase {
public:
    ChannelInput(uint16_t iid, InputType type, const std::string& name, InputTag tag, uint8_t channel, TriggerType trigger_type, uint64_t duration, std::vector<std::unique_ptr<ActionGroup>>&& action_groups)
        : InputBase(iid, type, name, tag, std::move(action_groups)) {}

    ~ChannelInput() {
        if (uncertain_timer != nullptr) {
            BaseType_t res = xTimerDelete(uncertain_timer, pdMS_TO_TICKS(3000));
            if (res != pdPASS) {
                ESP_LOGW("ChannelInput", "Failed to delete timer!");
            }
            uncertain_timer = nullptr;

        }
    }
    uint8_t channel;
    TriggerType trigger_type;

    uint64_t duration;  // 红外用的
    void execute() override;

private:
    // 红外用的
    enum SensorState { SENSOR_IDLE, SENSOR_ACTIVE, SENSOR_UNCERTAIN };
    SensorState currentState = SENSOR_IDLE;
    TimerHandle_t uncertain_timer = nullptr;
    static void static_uncertain_timer_callback(TimerHandle_t xTimer);
    void uncertain_timer_callback(TimerHandle_t xTimer);

};