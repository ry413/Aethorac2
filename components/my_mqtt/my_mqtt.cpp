#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"

#include "mqtt_secrets.h"
#include "stm32_tx.h"
#include "lord_manager.h"
#include "indicator.h"
#include "commons.h"
#include "network.h"
#include "identity.h"
#include "my_mqtt.h"
#include "json_codec.h"
#include "../json.hpp"


static const char *TAG = "mqtt";
using json = nlohmann::json;

esp_mqtt_client_handle_t client;
static volatile vprintf_like_t orig_vprintf = nullptr;

char down_topic[40];
char up_topic[40];
char log_topic[40];

static std::string g_ota_url;

static esp_err_t ota_dbg_handler(esp_http_client_event_t *e);
static void handle_mqtt_ndjson(const char* data, size_t data_len);

bool mqtt_connected = false;

void ota_task(void *param) {
    ESP_LOGI("heap", "free=%u  min=%u",
         heap_caps_get_free_size(MALLOC_CAP_8BIT),
         heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "Starting OTA task, URL=%s", g_ota_url.c_str());

    esp_http_client_config_t http_config = {};
    http_config.url = g_ota_url.c_str();
    http_config.crt_bundle_attach = esp_crt_bundle_attach; 
    http_config.event_handler = ota_dbg_handler;
    http_config.keep_alive_enable = true;
    http_config.use_global_ca_store = true;
    http_config.skip_cert_common_name_check = true;
    http_config.disable_auto_redirect = false;
    http_config.user_agent = "xzrcu-ota";

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_config;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "试图OTA: %s", g_ota_url.c_str());
    ESP_LOGI("ota", "%s", buffer);
    urgentPublishDebugLog(buffer);
    
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        snprintf(buffer, sizeof(buffer), "OTA完成, 准备重启");
        ESP_LOGI(TAG, "%s", buffer);
        urgentPublishDebugLog(buffer);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        snprintf(buffer, sizeof(buffer), "OTA upgrade failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "%s", buffer);
        urgentPublishDebugLog(buffer);
    }
    vTaskDelete(NULL);
}

void start_ota_upgrade(const std::string &url) {
    g_ota_url = url;
    xTaskCreatePinnedToCore(ota_task, "ota_task", 8192, nullptr, 5, nullptr, 1);
}

// 固定间隔上报所有设备状态
static void report_state_task(void *param) {
    int runtime_counter = 0;
    
    while (true) {
        report_states();

        if (runtime_counter >= 60) {   
            int64_t uptime_us = esp_timer_get_time();
            int64_t uptime_s = uptime_us / 1000000;
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "runtime: %lld", uptime_s);
            urgentPublishDebugLog(log_msg);
            runtime_counter = 0;
        }

        vTaskDelay(60000 / portTICK_PERIOD_MS);
        runtime_counter++;
    }
    vTaskDelete(nullptr);
}

// 固定间隔上报过去一分钟的操作记录
static void report_operation_task(void *param) {
    while (true) {
        report_op_logs();

        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(nullptr);
}

static void report_after_ota(void) {
    static bool is_first = true;
    if (!is_first) {
        return;
    }

    const esp_partition_t *part = esp_ota_get_running_partition();
    const esp_app_desc_t  *app  = esp_app_get_description();

    esp_ota_img_states_t st;
    esp_ota_get_state_partition(part, &st);

    char sha8[9];
    sprintf(sha8, "%02x%02x%02x%02x",
            app->app_elf_sha256[0],
            app->app_elf_sha256[1],
            app->app_elf_sha256[2],
            app->app_elf_sha256[3]);
    sha8[8] = '\0';
    
    char msg[256];
    snprintf(msg, sizeof(msg),
            "OTA_OK | part=%s | ver=%s | sha=%.8s | build=%s %s | "
            "state=%s | heap=%lu/%lu",
            part->label,
            app->version,
            sha8,
            app->date, app->time,
            (st==ESP_OTA_IMG_VALID)?"VALID":
            (st==ESP_OTA_IMG_PENDING_VERIFY)?"PENDING":
            (st==ESP_OTA_IMG_INVALID)?"INVALID":"ABORTED",
            esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    printf("OTA-POST: %s\n", msg);
    ESP_LOGI("OTA-POST", "%s", msg);
    is_first = false;
}

static void register_the_rcu();
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    client = event->client;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        mqtt_connected = true;
        printf("MQTT 已连接\n");
        printCurrentFreeMemory();
        esp_mqtt_client_subscribe(client, down_topic, 0);
        printf("已订阅接收主题: %s\n", down_topic);

        // 注册本机
        register_the_rcu();
        // 启动报告状态任务
        static TaskHandle_t report_st_task_handle;
        if (report_st_task_handle == nullptr) {
            xTaskCreate(report_state_task, "report_state_task", 4096, nullptr, 3, &report_st_task_handle);
        }
        // 启动报告操作任务
        static TaskHandle_t report_op_task_handle;
        if (report_op_task_handle == nullptr) {
            xTaskCreate(report_operation_task, "report_operation_task", 8192, nullptr, 3, &report_op_task_handle);
        }

        if (!orig_vprintf) {
            orig_vprintf = esp_log_set_vprintf(my_log_send_func);
        } else {
            esp_log_set_vprintf(my_log_send_func);
        }
        ESP_LOGI(TAG, "日志已重定向至mqtt");
        xTaskCreate([](void *param) {            
            vTaskDelay(pdMS_TO_TICKS(1000));
            report_after_ota();
            vTaskDelete(nullptr);
        }, "report_after_ota_task", 4096, nullptr, 3, nullptr);
        break;
    }
    case MQTT_EVENT_DISCONNECTED: {
        mqtt_connected = false;
        if (orig_vprintf) {
            esp_log_set_vprintf(orig_vprintf);
            ESP_LOGI(TAG, "日志重定向回本地");
        } else {
            printf("orig_vprintf == null\n");
        }
        printf("MQTT 已断开\n");
        printCurrentFreeMemory();
        break;
    }
    case MQTT_EVENT_DATA: {
        std::string msg_data(event->data, event->data_len);
        handle_mqtt_ndjson(msg_data.c_str(), msg_data.size());
        break;
    }
    default:
        break;
    }
}

void mqtt_app_start() {
    if (client) {
        ESP_LOGW(TAG, "MQTT client 非预期存在, 自动清理旧连接");
        mqtt_app_stop();
    }
    
    if (std::string(getSerialNum()).empty()) {
        ESP_LOGW(TAG, "此设备无序列号, 停止连接mqtt");
        return;
    }
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {.address = {.uri = MQTT_BROKER,}},
        .credentials = {
            .username = MQTT_USER,
            .authentication = {.password = MQTT_PASS},
        },
        .session = {.protocol_ver = MQTT_PROTOCOL_V_5},
        .task = {.stack_size = 8192},
        .buffer = {.size = 20480},
    };

    snprintf(down_topic, sizeof(down_topic), "/XZRCU/DOWN/%s", getSerialNum());
    snprintf(up_topic, sizeof(up_topic), "/XZRCU/UP/%s", getSerialNum());
    snprintf(log_topic, sizeof(log_topic), "/XZRCU/LOG/%s", getSerialNum());

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
void mqtt_app_stop(void) {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
        ESP_LOGI(TAG, "MQTT 已停止");
    }
}

void mqtt_publish_message(const std::string& message, int qos, int retain) {
    if (!client || !network_is_ready() || !mqtt_connected) {
        return;
    }
    char up_topic[40];
    snprintf(up_topic, sizeof(up_topic), "/XZRCU/UP/%s", getSerialNum());
    int msg_id = esp_mqtt_client_publish(client, up_topic, message.c_str(), message.length(), qos, retain);
    printf("Message sent, msg_id=%d, topic=%s, message=%s\n", msg_id, up_topic, message.c_str());
}

// 注册时携带所有信息
static void register_the_rcu() {
    std::string json_str = generateRegisterInfo().dump();
    mqtt_publish_message(json_str.c_str(), 0, 0);
}

void report_states() {
    std::string json_str = generateReportStates().dump();
    mqtt_publish_message(json_str.c_str(), 0, 0);
}

static void handle_mqtt_ndjson(const char* data, size_t data_len) {
    std::string_view raw{data, data_len};
    const auto lines = splitByLineView(raw);

    if (lines.empty()) {
        ESP_LOGW(TAG, "接收到空行，忽略");
        return;
    }

    json obj;   
    try {
        obj = json::parse(lines[0]);
    } catch (const std::exception& e) {
        ESP_LOGW(TAG, "JSON 解析失败: %s", e.what());
        return;
    }

    if (obj.empty()) {
        ESP_LOGW(TAG, "解析后的 JSON 是空的");
        return;
    }

    auto it = obj.begin();
    std::string_view key = it.key();
    const json& value = it.value();

    std::string type = value.get<std::string>();

    try {
        const std::string& data_type = obj["type"];
        if (key == "type") {
            if (type == "urge") {
                report_states();
                return;
            } else if (type == "ctl") {
                if (auto msg = json::parse(lines[1]); msg.is_object()) {
                    std::string dev_type = msg.value("devicetype", "");
                    uint16_t dev_did = static_cast<uint16_t>(std::stoi(msg.value("deviceid", "-1")));
                    std::string operation = msg.value("operation", "");
                    std::string parameter;
                    if (msg.contains("param")) {
                        if (msg["param"].is_string()) {
                            parameter = msg["param"];
                        } else if (msg["param"].is_number()) {
                            parameter = std::to_string(msg["param"].get<int>());
                        }
                    }

                    if (dev_type == "mode") {
                        for (auto* mode : LordManager::instance().getAllModeActionGroup()) {
                            if (mode->getName() == operation) {
                                ESP_LOGI_CYAN(TAG, "后台调用[%s]", mode->getName().c_str());
                                mode->executeAllAtomicAction();
                            }
                        }
                    } else {
                        if (auto* dev = LordManager::instance().getDeviceByDid(dev_did)) {
                            dev->execute(operation, parameter);
                            // 后台单控完设备后要同步那个设备可能存在的按键指示灯
                            IndicatorHolder::getInstance().callAllAndClear();
                        }
                    }
                }
                return;
            } else if (type == "ota") {
                auto msg = json::parse(lines[1]);
                if (msg.is_object()) {
                    if (msg.contains("ver") && msg["ver"].is_string() &&
                        msg.contains("url") && msg["url"].is_string()) {
                        const std::string ver = msg["ver"].get<std::string>();
                        const std::string ota_url = msg["url"].get<std::string>();
                        if (ver != AETHORAC_VERSION) {
                            ESP_LOGI(TAG, "OTA: %s -> %s", AETHORAC_VERSION, ver.c_str());
                            start_ota_upgrade(ota_url);
                        } else {
                            ESP_LOGI(TAG, "Same version, skip OTA.");
                            urgentPublishDebugLog("OTA版本相同, 跳过升级");
                        }
                    }
                }
                return;
            } else if (type == "oracle") {
                auto msg = json::parse(lines[1]);
                if (msg.is_object()) {
                    if (msg.contains("operation") && msg["operation"].is_string()) {
                        std::string operation = msg["operation"].get<std::string>();
                        // 查stm32版本号
                        if (operation == "stm32_version") {
                            sendStm32Cmd(0xFF, 00, 00, 00, 00);
                        }
                        // 指定继电器进入测试模式
                        else if (operation == "stm32_test") {
                            if (msg.contains("target") && msg["target"].is_string()
                            && msg.contains("state") && msg["state"].is_string()) {
                                std::string target = msg["target"].get<std::string>();
                                std::string state = msg["state"].get<std::string>();

                                sendStm32Cmd(0x07, std::stoi(target), std::stoi(state), 00, 00);
                            }
                        }
                        // 重定向控制台到mqtt
                        else if (operation == "air_log") {
                            if (msg.contains("state") && msg["state"].is_string()) {
                                std::string state = msg["state"].get<std::string>();
                                int state_int = std::stoi(state);

                                if (state_int == 1) {
                                    if (!orig_vprintf) {
                                        orig_vprintf = esp_log_set_vprintf(my_log_send_func);
                                    } else {
                                        esp_log_set_vprintf(my_log_send_func);
                                    }
                                    ESP_LOGI(TAG, "日志已重定向至mqtt: %s", log_topic);
                                } else if (orig_vprintf) {
                                    esp_log_set_vprintf(orig_vprintf);
                                    ESP_LOGI(TAG, "日志已重定向至初始值");
                                }
                            }
                        }
                        else if (operation == "ver") {
                            urgentPublishDebugLog(AETHORAC_VERSION);
                        }
                    }
                }
                return;
            } else if (type == "GET_FILE") {
                ESP_LOGI(TAG, "通过MQTT收到 GET_FILE 命令");
                int file_fd = open(LOGIC_CONFIG_FILE_PATH, O_RDONLY);
                if (file_fd < 0) {
                    ESP_LOGE(TAG, "读取文件失败");
                    std::string error_msg = "ERROR: File not found\n";
                    mqtt_publish_message(error_msg, 1, 0);
                } else {
                    char file_buf[1024];
                    ssize_t read_bytes;
                    std::string file_data;
                    // 分段读取文件内容
                    while ((read_bytes = read(file_fd, file_buf, sizeof(file_buf))) > 0) {
                        file_data.append(file_buf, read_bytes);
                    }
                    close(file_fd);
                    // 结束标识
                    mqtt_publish_message(file_data, 1, 0);
                }
                return;
            } else if (type == "room_info") {
                auto msg = json::parse(lines[1]);
                if (msg.is_object()) {
                    ESP_LOGI(TAG, "收到房间信息数据");
                    if (msg.contains("hotel_name") && msg["hotel_name"].is_string() &&
                        msg.contains("room_name") && msg["room_name"].is_string()) {
                        
                        std::string hotel_name = msg["hotel_name"].get<std::string>();
                        std::string room_name = msg["room_name"].get<std::string>();

                        ESP_LOGI(TAG, "hotel_name: %s, room_name: %s", hotel_name.c_str(), room_name.c_str());

                        nvs_handle_t handle;
                        esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "打开 NVS 句柄失败: %s", esp_err_to_name(err));
                            return;
                        }
                        err = nvs_set_str(handle, "hotel_name", hotel_name.c_str());
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "写入 hotel_name 失败: %s", esp_err_to_name(err));
                            return;
                        }
                        err = nvs_set_str(handle, "room_name", room_name.c_str());
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "写入 room_name 失败: %s", esp_err_to_name(err));
                            return;
                        }
                        err = nvs_commit(handle);
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "提交数据失败: %s", esp_err_to_name(err));
                            return;
                        }
                        nvs_close(handle);
                        register_the_rcu();
                    }
                }
                return;
            } else if (type == "wifi_cfg") {
                auto msg = json::parse(lines[1]);
                if (msg.is_object()) {
                    ESP_LOGI(TAG, "通过网络收到WiFi配置");
                    if (msg.contains("ssid") && msg["ssid"].is_string() &&
                        msg.contains("pass") && msg["pass"].is_string()) {
                            save_wifi_credentials(msg["ssid"].get<std::string>().c_str(),
                                                msg["pass"].get<std::string>().c_str());
                    }
                }
                return;
            } else if (type == "change_net_type") {
                auto msg = json::parse(lines[1]);
                if (msg.is_object()) {
                    ESP_LOGI(TAG, "通过网络收到更改网络驱动请求");
                    if (msg.contains("target") && msg["target"].is_string()) {
                        auto val = msg["target"].get<std::string>();
                        if (val == "1") {
                            change_network_type_and_reboot(NET_TYPE_WIFI);
                        } else if (val == "2") {
                            change_network_type_and_reboot(NET_TYPE_ETHERNET);
                        } else {
                            ESP_LOGE(TAG, "错误参数: %s", val.c_str());
                        }
                    }
                }
                return;
            } else if (type == "Laminor2") {
                ESP_LOGI(TAG, "通过MQTT收到 JSON 配置数据");
                char buffer[128];
                size_t total = 0, used = 0;
                esp_err_t ret = esp_littlefs_info("littlefs", &total, &used);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "LittleFS 总容量: %d 字节, 已用: %d 字节", total, used);
                } else {
                    snprintf(buffer, sizeof(buffer), "获取 LittleFS 信息失败: %s", esp_err_to_name(ret));
                    ESP_LOGE(TAG, "%s", buffer);
                    urgentPublishDebugLog(buffer);
                }

                int file_fd = open(LOGIC_CONFIG_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (file_fd < 0) {
                    snprintf(buffer, sizeof(buffer), "打开文件失败: %s", strerror(errno));
                    ESP_LOGE(TAG, "%s", buffer);
                    urgentPublishDebugLog(buffer);
                } else {
                    ssize_t bytes_written = write(file_fd, data, data_len);
                    if (bytes_written < 0) {
                        snprintf(buffer, sizeof(buffer), "写入文件失败: %s %d", strerror(errno), errno);
                        ESP_LOGE(TAG, "%s", buffer);
                        urgentPublishDebugLog(buffer);
                    } else {
                        ESP_LOGI(TAG, "成功保存到文件, 5秒后应用配置");
            
                        xTaskCreate([](void *param) {
                            printCurrentFreeMemory();
                            vTaskDelay(3000 / portTICK_PERIOD_MS);
                            printCurrentFreeMemory();
                            parseLocalLogicConfig();
                            vTaskDelay(2000 / portTICK_PERIOD_MS);   // 等物理查询结果
                            register_the_rcu();
                            vTaskDelete(nullptr);
                        }, "parsejson", 8192, nullptr, 3, nullptr);
                    }
                    close(file_fd);
                }
                return;
            } else if (type == "restart") {
                esp_restart();
            } else {
                ESP_LOGE(TAG, "未知的操作类型: %s", type.c_str());
                return;
            }
        }
    } catch(const std::exception& e) {
        ESP_LOGE(TAG, "%s: %s\n内容: %s", __func__, e.what(), data);
    }
}

static volatile bool in_hook = false;
int my_log_send_func(const char *fmt, va_list args) {
    if (in_hook) return 0;
    in_hook = true;

    static char logbuf[512];
    size_t len = vsnprintf(logbuf, sizeof(logbuf), fmt, args);
    if (len >= sizeof(logbuf)) 
        len = sizeof(logbuf) - 1;

    if (client) {
        esp_mqtt_client_publish(client, log_topic,
                                logbuf, len, 0, 0);
    }

    in_hook = false;
    return len;
}

static esp_err_t ota_dbg_handler(esp_http_client_event_t *e) {
    switch (e->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI("OTA", "Connected to host");
        break;

    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI("OTA", "Request header sent");
        break;

    case HTTP_EVENT_ON_HEADER:
        // 打印所有响应头, 看 Content-Length
        ESP_LOGI("OTA-HDR", "%s: %s", e->header_key, e->header_value);
        break;

    case HTTP_EVENT_ON_FINISH: {
        // 头部全部收完, 拿到状态码 & content-length
        int status = esp_http_client_get_status_code(e->client);
        int64_t len = esp_http_client_get_content_length(e->client);
        ESP_LOGI("OTA", "HTTP %d, Content-Length = %lld", status, len);
        break;
    }
    case HTTP_EVENT_REDIRECT: {
        int status = esp_http_client_get_status_code(e->client);
        char redir_url[256] = {0};
        esp_err_t err = esp_http_client_get_url(e->client, redir_url, sizeof(redir_url));
        if (err == ESP_OK) {
            ESP_LOGW("OTA-HDR", "### REDIRECT %d → %s", status, redir_url);
        } else {
            ESP_LOGW("OTA-HDR", "### REDIRECT %d → <url too long>", status);
        }
    }
    default:
        break;
    }
    return ESP_OK;
}
