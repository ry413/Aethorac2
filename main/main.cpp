#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_littlefs.h"
#include "stdio.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_core_dump.h"
#include "esp_netif.h"

// #include <string>
// #include "rs485_comm.h"
#include "stm32_rx.h"
#include "network.h"
// #include "action_group.h"
// #include "esp_timer.h"
// #include <stm32_comm_types.h>
// #include <stm32_tx.h>
// #include <manager_base.h>

#include "commons.h"
#include "identity.h"
#include "json_codec.h"

#define TAG "main.cpp"

void init_littlefs() {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
        .grow_on_mount = true,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS 挂载失败: %s", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info("littlefs", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS 已挂载，总容量: %d 字节，已用: %d 字节", total, used);
    } else {
        ESP_LOGE(TAG, "无法获取 LittleFS 分区信息: %s", esp_err_to_name(ret));
    }
}

void init_nvs() {
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 初始化失败 (%s)，尝试擦除后重新初始化", esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS 初始化成功");
}

// static void monitor_task(void *pvParameter) {
// 	while(1) {
//         // 我知道这明显重复了
//         print_free_memory();

//         char buffer[128];
//         snprintf(buffer, sizeof(buffer), "I: %d, D: %d",
//                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_DMA));

//         add_log_entry("monitor", 0, "memory", buffer, true);
// 		vTaskDelay(1800000 / portTICK_PERIOD_MS);
// 	}
// }

extern "C" void app_main(void) {
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "当前版本: %s, 运行于分区: %s", AETHORAC_VERSION, running_partition->label);
    ESP_LOGI(TAG, "本设备序列号: %s", getSerialNum());
    init_littlefs();
    init_nvs();
    uart_init_stm32();
    // uart_init_rs485();
    esp_netif_init();
    
    xTaskCreate([] (void *param) {
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);  // 稍微等一下, 不然第二次parseNdJson时会崩溃
    //     parseNdJson(read_json_to_string(FILE_PATH));
        parseLocalLogicConfig();
        restore_last_network();

        vTaskDelete(nullptr);
    }, "parse_json_task", 8192, nullptr, 3, nullptr);
    // xTaskCreate(monitor_task, "monitor_task", 4096, nullptr, 3, nullptr);

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA upgrade confirmed, rollback cancelled");
    } else {
        ESP_LOGE(TAG, "Failed to mark app as valid: %s", esp_err_to_name(err));
    }
}
