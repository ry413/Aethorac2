#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "action_group.h"
#include "manager_base.h"
#include "idevice.h"
#include "room_state.h"
#include "indicator.h"
#include "commons.h"
#include "esp_timer.h"

#define TAG "ACTION_GROUP"

static void executeAllAtomicActionTask(void* pvParameter) {
    ActionGroup* self = static_cast<ActionGroup*>(pvParameter);

    for (const auto& atomic_action : self->atomic_actions) {
        // auto target_ptr = atomic_action.target_device.lock();
        // if (target_ptr) {
        //     target_ptr->execute(atomic_action.operation,
        //         atomic_action.parameter,
        //         self->uid,
        //         LordManager::getInstance().getCurrMode().empty());  // 如果此时进入了某种模式, 则不让设备记录日志
        // } else {
        //     ESP_LOGE(TAG, "target不存在");
        // }
    }

    IndicatorHolder::getInstance().callAllAndClear();


    // 如果执行完这个动作组需要上报...那就上报
    if (self->require_report) {
        // report_states();
        self->require_report = false;
    }

    // 任务结束后清空句柄并自我删除
    self->clearTaskHandle();
    vTaskDelete(nullptr);
}

void ActionGroup::executeAllAtomicAction(std::string mode_name) {
    // last_action_group_time = esp_timer_get_time() / 1000ULL;
    // printf("此动作执行于%llu\n", last_action_group_time);
    // 检查是否已有任务在运行
    if (task_handle != nullptr) {
        ESP_LOGW(TAG, "动作已在执行中，跳过新任务创建");
        return;
    }

    std::string current_mode = LordManager::getInstance().getCurrMode();
    // 判断执行完后是否要进入某种模式
    if (!mode_name.empty()) {
        ESP_LOGI(TAG, "进入[%s]", mode_name.c_str());
        // 判断是否从无模式进入某种模式，或者从一种模式切换到另一种模式
        if (current_mode.empty() || current_mode != mode_name) {
            LordManager::getInstance().setCurrMode(mode_name);
            require_report = true;
            add_log_entry("mode", 0, "进入" + mode_name, "", true);
        }
    }
    // 如果未传入模式
    else {
        // 判断刚刚是不是模式, 如果是的话, 说明现在要退出某种模式
        // if (!current_mode.empty()) {
        //     // add_log_entry("mode", 0, "退出" + LordManager::getInstance().getCurrMode(), "", true);
        //     LordManager::getInstance().setCurrMode("");
        //     require_report = true;
        // }
    }

    // 创建新任务
    BaseType_t ret = xTaskCreate(
        executeAllAtomicActionTask,
        "ExecuteAtomicActions",
        4096,
        this,
        5,
        &task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建动作执行任务失败");
        task_handle = nullptr;
        return;
    }
}

void ActionGroup::clearTaskHandle() {
    task_handle = nullptr;
}

void ActionGroup::suicide() {
    // 在销毁此动作组前更新指示灯
    IndicatorHolder::getInstance().callAllAndClear();
    if (task_handle != nullptr) {
        TaskHandle_t temp_task_handle = task_handle;  // 保存任务句柄
        task_handle = nullptr;
        vTaskDelete(temp_task_handle);
    }
}