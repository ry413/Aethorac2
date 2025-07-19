#include "commons.h"
#include "esp_log.h"
#include "../my_mqtt/my_mqtt.h"
#include "esp_partition.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include <stdexcept>

#include "action_group.h"
#include "manager_base.h"
// #include "board_config.h"
#include "board_output.h"
#include "rs485_comm.h"
#include "stm32_comm_types.h"
#include "stm32_tx.h"
#include "lamp.h"
#include "curtain.h"
#include "air_conditioner.h"
// #include "other_device.h"
#include "rs485_command.h"
#include "voice_command.h"
#include "network.h"

// #include "../board_config/board_config.h"
// #include "../lamp/lamp.h"
// #include "../air_conditioner/air_conditioner.h"
// #include "../curtain/curtain.h"
// #include "../other_device/other_device.h"
// #include "../ethernet/ethernet.h"
// #include "panel.h"

#define TAG "commons"

using json = nlohmann::json;

// bool isValidSerial(const char *serial) {
//     if (!serial) return false;

//     size_t len = strlen(serial);

//     if (len != 8) return false;

//     for (size_t i = 0; i < 8; ++i) {
//         if (!isdigit((unsigned char)serial[i])) {
//             return false;
//         }
//     }
//     return true;
// }

// const char *getSerialNum() {
//     static char serial_buf[SERIAL_LEN + 1] = {0};
//     static bool is_initialized = false;

//     if (!is_initialized) {
//         const esp_partition_t *part = esp_partition_find_first(
//             ESP_PARTITION_TYPE_DATA,        // data
//             (esp_partition_subtype_t)0x79,  // subtype=0x79
//             SERIAL_PART_LABEL               // 名字=serial_num
//         );
//         if (!part) {
//             ESP_LOGE(TAG, "Failed to find 'serial_num' partition!");
//             urgentPublishDebugLog("找不到serial_num分区");
//             return "";
//         }

//         esp_err_t err = esp_partition_read(part, 0,  // offset=0
//                                            serial_buf, SERIAL_LEN);
//         if (err != ESP_OK) {
//             ESP_LOGE(TAG, "读取serial_num分区失败, err=0x%x", err);
//             urgentPublishDebugLog("读取serial_num分区失败");
//             return "";
//         }

//         serial_buf[SERIAL_LEN] = '\0';

//         if (!isValidSerial(serial_buf)) {
//             ESP_LOGW(TAG, "无效或缺失的serial number: '%s'", serial_buf);
//             urgentPublishDebugLog("无效或缺失的serial number");
//             serial_buf[0] = '\0';
//         }

//         is_initialized = true;
//     }

//     return serial_buf;
// }

time_t get_current_timestamp() {
    time_t now = time(nullptr);

    if (now < 1609459200) { // 如果时间戳小于 2021-01-01，表示时间未同步, 鬼知道为什么用这个时间, 随便
        ESP_LOGW(__func__, "时间未同步");
        return 0; // 返回0表示无效时间戳
    }

    return now;
}

// *************** 上报操作日志 ***************
// // 我很疑惑为什么不写默认参数
void add_log_entry(const std::string& devicetype, uint16_t deviceid, const std::string& operation,
                   std::string param, bool should_log) {
//     if (!should_log) return;
    
//     std::lock_guard<std::mutex> lock(log_mutex); // 确保线程安全

//     if (log_array.size() >= LOG_MAX_SIZE) {
//         ESP_LOGI(TAG, "一分钟内日志抵达上限, 移除最旧的日志");
//         log_array.erase(log_array.begin());
//     }
//     nlohmann::json entry = {
//         {"devicetype", devicetype},
//         {"deviceid", std::to_string(deviceid)},
//         {"operation", operation},
//         {"param", param},
//         {"tm", std::to_string(get_current_timestamp())}
//     };
//     log_array.push_back(entry);
}

// std::vector<nlohmann::json> fetch_and_clear_logs() {
//     std::lock_guard<std::mutex> lock(log_mutex); // 确保线程安全
//     std::vector<nlohmann::json> current_logs = std::move(log_array); // 移动数据
//     log_array.clear();
//     return current_logs;
// }

void report_op_logs(void) {
//     if (!(network_is_ready()) || !mqtt_connected) {
//         ESP_LOGW(TAG, "无网络, 不上报操作日志");
//         return;
//     }
    
//     auto logs = fetch_and_clear_logs();
//     if (!logs.empty()) {
//         while (!logs.empty()) {
//             size_t batch_size = std::min(logs.size(), LOG_MAX_SIZE / 2);
//             std::vector<nlohmann::json> batch(logs.begin(), logs.begin() + batch_size);
//             logs.erase(logs.begin(), logs.begin() + batch_size);

//             nlohmann::json json;
//             json["type"] = "log";
//             json["mac"] = getSerialNum();
//             json["list"] = batch;

//             mqtt_publish_message(json.dump(), 0, 0);
//         }
//     } else {
//         ESP_LOGI(TAG, "日志数组为空, 无需发送");
//     }
}

void printCurrentFreeMemory(const std::string& msg_head) {
    ESP_LOGI("Monitor_memory", "%s INTERNAL: %d, DMA: %d", 
        msg_head.c_str(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_DMA));
}

void urgentPublishDebugLog(const std::string& msg) {
    add_log_entry("debug", 0, "", msg, true);
    report_op_logs();
}

std::vector<int> pavectorseHexToFixedArray(const std::string& hexString) {
    std::vector<int> result;
    size_t len = hexString.length();

    if (len % 2 != 0) {
        ESP_LOGE(TAG, "十六进制字符串长度必须是偶数");
        return result;
    }

    for (size_t i = 0; i < len; i += 2) {
        std::string byte_str = hexString.substr(i, 2);
        try {
            int byte = static_cast<int>(std::stoi(byte_str, nullptr, 16));
            result.push_back(byte);
        } catch (const std::invalid_argument& e) {
            ESP_LOGE(TAG, "非法十六进制字符: '%s'", byte_str.c_str());
            return {};
        } catch (const std::out_of_range& e) {
            ESP_LOGE(TAG, "十六进制数值溢出: '%s'", byte_str.c_str());
            return {};
        }
    }

    return result;
}