#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <esp_log.h>
#include "channel_input.h"
#include "commons.h"
#include "lord_manager.h"
#include "drycontact_out.h"
#include "esp_timer.h"
#include <lamp.h>

#define TAG "CHANNEL_INPUT"

void ChannelInput::execute() {
    ESP_LOGI_CYAN(TAG, "channel(%u)[%s]开始执行动作组(%u/%u)", channel, name.c_str(), current_index + 1, action_groups.size());
    static auto& lord = LordManager::instance();

    if (lord.isSleep()) {
        lord.useAliveHeartBeat();
        for (auto* dry : lord.getDevicesByType<Lamp>()) {
            dry->syncAssBtnToDevState();
        }
        for (auto* dry : lord.getDevicesByType<SingleRelayDevice>()) {
            dry->syncAssBtnToDevState();
        }
        for (auto* dry : lord.getDevicesByType<DryContactOut>()) {
            dry->syncAssBtnToDevState();
        }
    }
    
    // 干接点同样可以忽略任意键执行
    // 如果有"无视任意键"的tag就跳过
    if (!tags.contains(InputTag::IGNORE_ANY_KEY_EXECUTE) && lord.execute_any_key_action_group()) {
        printf("已调用任意键执行动作组, 返回\n");
    } else if (current_index < action_groups.size()) {
        action_groups[current_index]->executeAllAtomicAction();

        current_index = (current_index + 1) % action_groups.size();
    }
}

void ChannelInput::uncertain_timer_callback(TimerHandle_t xTimer) {
    static auto &lord = LordManager::instance();
    // 这里检查一下, 如果此时还是SENSOR_UNCERTAIN，才能执行相关操作
    if (currentState == SENSOR_UNCERTAIN) {
        currentState = SENSOR_IDLE;
        ESP_LOGI(TAG, "定时器超时, 它确认无人, 试图执行超时逻辑\n");

        // 检查现在是不是想拔卡(红外操作插拔卡)
        if (this->tags.contains(InputTag::IS_ALIVE_CHANNEL)) {
            ESP_LOGI(TAG, "试图执行的是拔卡, 进行判断");
            bool door_event_after_last_presence =
                (lord.last_door_open_time  > lord.last_presence_time) &&   // 门确实在“最后有人”之后打开过
                (lord.last_door_close_time > lord.last_door_open_time);    // 而且已经关上，形成一次完整离开动作
            ESP_LOGI(TAG, "last_door_open_time: %llu, last_door_close_time: %llu, last_presence_time: %llu",
                        lord.last_door_open_time, lord.last_door_close_time, lord.last_presence_time);

            bool valid_to_cut_power = lord.last_door_close_time > lord.last_presence_time;  // 比door_event_after_last_presence更宽松的判断条件

            // 1
            if (lord.door_open) {           // 门仍开着，当然不能拔
                ESP_LOGI(TAG, "门还开着，不拔卡, 重置红外倒计时");  // 人出去后, 却还长时间开着门的情况
                // xTimerStop(uncertain_timer, 0);
                // xTimerStart(uncertain_timer, 0);
                arm_uncertain_timer();
                return;
            }
            // 2
            if (!valid_to_cut_power) {
                ESP_LOGI(TAG, "门未发生有效开关，不拔卡");
                return;
            }
            
            // 3
            uint64_t now_ms = esp_timer_get_time() / 1000ULL;
            uint64_t idle_ms = now_ms - lord.last_action_group_time;
            if (idle_ms < 10000) {
                ESP_LOGI(TAG, "最近执行过动作, 不拔卡");
                // xTimerStop(uncertain_timer, 0);
                // xTimerStart(uncertain_timer, 0);
                arm_uncertain_timer();
                return;
            }

            ESP_LOGI(TAG, "判断通过, 可以拔卡");
        }
        
        // 寻找同输入通道, 输入类型是[红外超时]的动作组
        for (ChannelInput* input : lord.getAllChannelInputByChannelNum(this->channel)) {
            if (input->trigger_type == TriggerType::INFRARED_TIMEOUT) {
                input->execute();
            }
        }
    }
}

void ChannelInput::static_uncertain_timer_callback(TimerHandle_t xTimer) {
    // 取出当初传进 pvTimerID 的 this 指针
    void* pv = pvTimerGetTimerID(xTimer);
    ChannelInput* pThis = static_cast<ChannelInput*>(pv);

    // 调用真正的非静态成员函数
    if (pThis) {
        pThis->uncertain_timer_callback(xTimer);
    }
}

void ChannelInput::execute_infrared(uint8_t state) {
    static auto& lord = LordManager::instance();
    switch (currentState) {
        case SENSOR_IDLE:
            if (state == 0x01) {
                currentState = SENSOR_ACTIVE;
                lord.last_presence_time = esp_timer_get_time() / 1000;

                if (lord.execute_any_key_action_group()) {
                    printf("已调用任意键执行动作组, 返回\n");
                } else if (current_index < action_groups.size()) {
                    action_groups[current_index]->executeAllAtomicAction();

                    current_index = (current_index + 1) % action_groups.size();
                }
            } else if (state == 0x00) {
                ESP_LOGI(TAG, "IDLE时收到无人判断, 重置定时器");    // 估计属于纯调试状况, 实际不会发生
                currentState = SENSOR_UNCERTAIN;
                // xTimerStop(uncertain_timer, 0);
                // xTimerStart(uncertain_timer, 0);
                arm_uncertain_timer();
            }
            break;

        case SENSOR_ACTIVE:
            if (state == 0x00) {
                currentState = SENSOR_UNCERTAIN;
                ESP_LOGI(TAG, "检测到不确定状态, 启动定时器\n");
                // 先停止一下，防止之前残留的情况
                // xTimerStop(uncertain_timer, 0);

                // 启动一次性定时器
                // xTimerStart(uncertain_timer, 0);

                arm_uncertain_timer();
            }
            break;

        case SENSOR_UNCERTAIN:
            if (state == 0x01) {
                ESP_LOGI(TAG, "误报，重新确认有人\n");
                currentState = SENSOR_ACTIVE;
                lord.last_presence_time = esp_timer_get_time() / 1000;
                xTimerStop(uncertain_timer, 0);
            }
            // 如果还是0x00, 就继续等待超时
            break;
    }
}

void ChannelInput::init_infrared_timer() {
    assert(trigger_type == TriggerType::INFRARED);
    if (!uncertain_timer) {
        uncertain_timer = xTimerCreate(
            "uncertainTimer",
            1, // 初始值无所谓, 之后会被重设
            pdFALSE,
            this,
            static_uncertain_timer_callback
        );
    }
}


static inline bool is_daytime_now(time_t now, int dayStart, int nightStart) {
    struct tm lt; localtime_r(&now, &lt);
    int h = lt.tm_hour;
    if (dayStart < nightStart) return h >= dayStart && h < nightStart;
    return h >= dayStart || h < nightStart; // 跨零点
}

// 夜间把 duration(秒) x2 -> Tick
TickType_t ChannelInput::calc_uncertain_ticks() const {
    static auto& lord = LordManager::instance();
    time_t now = get_current_timestamp();
    if (now == 0)  {
        ESP_LOGI(TAG, "无法获取时间, 红外计时一倍");
        return pdMS_TO_TICKS(duration * 1000u);
    }
    bool day = is_daytime_now(now, lord.dayTimeStart, lord.nightTimeStart);
    ESP_LOGI(TAG, "现在是[%s]", day ? "白天" : "夜晚");
    uint32_t ms = duration * 1000u * (day ? 1u : 2u);
    return pdMS_TO_TICKS(ms);
}

// 统一"按当前昼/夜重装一次性定时器"的入口
void ChannelInput::arm_uncertain_timer() {
    // xTimerChangePeriod不论已启动与否，都会把到期时间重置为"从现在起新周期"
    xTimerChangePeriod(uncertain_timer, calc_uncertain_ticks(), 0);
}