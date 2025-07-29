#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "action_group.h"
#include "idevice.h"
#include "indicator.h"
#include "esp_timer.h"
#include "commons.h"
#include "lord_manager.h"

#define TAG "ACTION_GROUP"

static void executeAllAtomicActionTask(void* pvParameter) {
    ActionGroup* self = static_cast<ActionGroup*>(pvParameter);

    for (const auto& atomic_action : self->actions) {
        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            ESP_LOGW(TAG, "任务收到中断通知，中止执行");
            break;
        }
        if (atomic_action.target_device) {
            atomic_action.target_device->execute(atomic_action.operation, atomic_action.parameter, self, !self->is_mode());
        } else {
            ESP_LOGE(TAG, "动作组(%d)找不到目标设备", self->getAid());
        }
    }

    // 完成动作组, 发布所有已注册的面板按键指示灯更新函数
    IndicatorHolder::getInstance().callAllAndClear();

    self->clearTaskHandle();
    vTaskDelete(nullptr);
}

void ActionGroup::executeAllAtomicAction() {
    static auto& lord = LordManager::instance();

    lord.last_action_group_time = esp_timer_get_time() / 1000ULL;
    // 检查是否已有任务在运行
    if (task_handle != nullptr) {
        ESP_LOGW(TAG, "动作已在执行中，跳过新任务创建");
        return;
    }

    // 判断执行完后是否要进入某种模式
    if (is_mode()) {
        ESP_LOGI(TAG, "进入模式[%s]", name.c_str());
        add_log_entry("mode", 0, "进入" + name, "");
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
        ESP_LOGI(TAG, "请求中断任务执行");
        xTaskNotifyGive(task_handle);
    }
}