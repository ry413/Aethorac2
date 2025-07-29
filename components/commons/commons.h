#ifndef commons_H
#define commons_H

extern "C" {
#include "stdio.h"
}

#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <memory>
#include "../json.hpp"
#include <ctime>
#include <esp_sntp.h>
#include <esp_log.h>

void printCurrentFreeMemory(const std::string& head = "当前内存");  // 打印内存
std::vector<uint8_t> pavectorseHexToFixedArray(const std::string& hexString);// 将指令码字符串解析成array
time_t get_current_timestamp();                             // 获取当前时间

// *************** 上报操作日志 ***************
static std::vector<nlohmann::json> log_array;
static std::mutex log_mutex;
const size_t LOG_MAX_SIZE = 50;
void add_log_entry(const std::string& devicetype, uint16_t deviceid,
                   const std::string& operation, std::string param, bool should_log = true);
std::vector<nlohmann::json> fetch_and_clear_logs();
void report_op_logs(void);
void urgentPublishDebugLog(const std::string& msg);

#define ESP_LOGI_CYAN(tag, fmt, ...) \
    ESP_LOGI(tag, "%s" fmt "%s", "\033[1;36m", ##__VA_ARGS__, "\033[0m")
#endif // commons_H