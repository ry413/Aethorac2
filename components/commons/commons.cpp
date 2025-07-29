#include "commons.h"
#include "esp_log.h"
#include "../my_mqtt/my_mqtt.h"
#include "esp_partition.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include <stdexcept>

#include "action_group.h"
#include "rs485_comm.h"
#include "lamp.h"
#include "curtain.h"
#include "air_conditioner.h"
#include "identity.h"
#include "rs485_command.h"
#include "network.h"

#define TAG "commons"

using json = nlohmann::json;

time_t get_current_timestamp() {
    time_t now = time(nullptr);

    if (now < 1609459200) { // 如果时间戳小于 2021-01-01，表示时间未同步, 鬼知道为什么用这个时间, 随便
        ESP_LOGW(__func__, "时间未同步");
        return 0;           // 返回0表示无效时间戳
    }

    return now;
}

// *************** 上报操作日志 ***************
void add_log_entry(const std::string& devicetype, uint16_t deviceid, const std::string& operation,
                   std::string param, bool should_log) {
    if (!should_log) return;
    
    std::lock_guard<std::mutex> lock(log_mutex); // 确保线程安全

    if (log_array.size() >= LOG_MAX_SIZE) {
        ESP_LOGI(TAG, "一分钟内日志抵达上限, 移除最旧的日志");
        log_array.erase(log_array.begin());
    }
    nlohmann::json entry = {
        {"devicetype", devicetype},
        {"deviceid", std::to_string(deviceid)},
        {"operation", operation},
        {"param", param},
        {"tm", std::to_string(get_current_timestamp())}
    };
    log_array.push_back(entry);
}

std::vector<nlohmann::json> fetch_and_clear_logs() {
    std::lock_guard<std::mutex> lock(log_mutex); // 确保线程安全
    std::vector<nlohmann::json> current_logs = std::move(log_array); // 移动数据
    log_array.clear();
    return current_logs;
}

void report_op_logs(void) {
    if (!(network_is_ready()) || !mqtt_connected) {
        ESP_LOGW(TAG, "无网络, 不上报操作日志");
        return;
    }
    
    auto logs = fetch_and_clear_logs();
    if (!logs.empty()) {
        while (!logs.empty()) {
            size_t batch_size = std::min(logs.size(), LOG_MAX_SIZE / 2);
            std::vector<nlohmann::json> batch(logs.begin(), logs.begin() + batch_size);
            logs.erase(logs.begin(), logs.begin() + batch_size);

            nlohmann::json json;
            json["type"] = "log";
            json["mac"] = getSerialNum();
            json["list"] = batch;

            mqtt_publish_message(json.dump(), 0, 0);
        }
    } else {
        ESP_LOGI(TAG, "日志数组为空, 无需发送");
    }
}

void printCurrentFreeMemory(const std::string& msg_head) {
    ESP_LOGI("Monitor_memory", "%s Internal: %d, DMA: %d", 
        msg_head.c_str(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_DMA));
}

void urgentPublishDebugLog(const std::string& msg) {
    add_log_entry("debug", 0, "", msg);
    report_op_logs();
}

std::vector<uint8_t> pavectorseHexToFixedArray(const std::string& hexString) {
    std::vector<uint8_t> result;
    size_t len = hexString.length();

    if (len % 2 != 0) {
        ESP_LOGE(TAG, "十六进制字符串长度必须是偶数");
        return result;
    }

    for (size_t i = 0; i < len; i += 2) {
        std::string byte_str = hexString.substr(i, 2);
        try {
            uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
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