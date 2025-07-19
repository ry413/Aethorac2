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

#define FILE_PATH "/littlefs/rcu_config.ndjson"
#define LFS_PARTITION_LABEL "fake_lfs"

// 获取字符串格式的序列号
const char *getSerialNum();
// 读取本地配置文件
std::string read_json_to_string(const std::string& filepath);
// 解析配置文件
void parseNdJson(const std::string& json_str);
// 反正也是解析用的
// std::vector<std::string> splitByLine(const std::string& content);
// 打印内存
void printCurrentFreeMemory(const std::string& head = "");
// 将指令码字符串解析成array
std::vector<int> pavectorseHexToFixedArray(const std::string& hexString);

// *************** 时间 ***************
// void init_sntp();
time_t get_current_timestamp();

// *************** 上报操作日志 ***************
static std::vector<nlohmann::json> log_array;
static std::mutex log_mutex;
const size_t LOG_MAX_SIZE = 50;
void add_log_entry(const std::string& devicetype, uint16_t deviceid,
                   const std::string& operation, std::string param, bool should_log);
std::vector<nlohmann::json> fetch_and_clear_logs();
void report_op_logs(void);
void urgentPublishDebugLog(const std::string& msg);

#endif // commons_H