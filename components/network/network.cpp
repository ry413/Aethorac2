#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_com.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "lwip/etharp.h"
#include "driver/gpio.h"
#include <esp_sntp.h>
#include "esp_log.h"
#include "./network.h"
#include "../my_mqtt/my_mqtt.h"
#include "../commons/commons.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "rs485_comm.h"

#include "esp_ota_ops.h"
#include "esp_https_ota.h"

#define TAG "NETWORK"
#define WIFI_CONNECTED_BIT BIT0


#define DEFAULT_WIFI_SSID "xz526"
#define DEFAULT_WIFI_PASS "xz526888"

// ======== 全局状态（单次上电有效） ========
static uint32_t             ip_raw               = 0;
static EventGroupHandle_t   wifi_event_group     = nullptr;
static esp_netif_t          *netif               = nullptr;
static esp_eth_mac_t        *mac                 = nullptr;
static esp_eth_phy_t        *phy                 = nullptr;
static esp_eth_handle_t     eth_handle           = nullptr;
static esp_eth_netif_glue_handle_t glue          = nullptr;

static bool        network_ready      = false;
static bool        sntp_initialized   = false;
static net_type_t  current_net_type   = NET_TYPE_NONE;

static esp_timer_handle_t reconnect_timer = nullptr;
static bool reconnect_timer_created = false;   // 防止重复 create

// ========== NVS 工具 ==========
void store_network_type(net_type_t type) {
    nvs_handle_t handle;
    if (nvs_open("network", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "net_type", static_cast<uint8_t>(type));
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "记录网络类型: %d", type);
    } else {
        ESP_LOGW(TAG, "无法打开 NVS 写入网络类型");
    }
}

static bool load_wifi_credentials(char *ssid_out, char *pass_out) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_cfg", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "无法打开 NVS, 使用默认 Wi-Fi");
        strncpy(ssid_out, DEFAULT_WIFI_SSID, 32);
        strncpy(pass_out, DEFAULT_WIFI_PASS, 64);
        return true;
    }

    size_t ssid_len = 32, pass_len = 64;
    err = nvs_get_str(handle, "ssid", ssid_out, &ssid_len);
    err |= nvs_get_str(handle, "pass", pass_out, &pass_len);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS 无有效 Wi-Fi 配置, 使用默认 Wi-Fi");
        strncpy(ssid_out, DEFAULT_WIFI_SSID, 32);
        strncpy(pass_out, DEFAULT_WIFI_PASS, 64);
    }
    return true;
}

bool save_wifi_credentials(const char* ssid, const char* pass) {
    nvs_handle_t handle;
    esp_err_t res1 = ESP_OK;
    esp_err_t res2 = ESP_OK;

    if (nvs_open("wifi_cfg", NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "无法打开NVS写入WiFi配置");
        return false;
    }

    if (ssid) {
        res1 = nvs_set_str(handle, "ssid", ssid);
        if (res1 != ESP_OK) {
            ESP_LOGE(TAG, "写入SSID失败: %d", res1);
        }
    }

    if (pass) {
        res2 = nvs_set_str(handle, "pass", pass);
        if (res2 != ESP_OK) {
            ESP_LOGE(TAG, "写入密码失败: %d", res2);
        }
    }

    nvs_commit(handle);
    nvs_close(handle);

    if ((ssid && res1 != ESP_OK) || (pass && res2 != ESP_OK)) {
        return false;
    }

    ESP_LOGI(TAG, "WiFi配置保存成功: SSID=\"%s\" Password=%s",
             ssid ? ssid : "(未更新)", pass ? pass : "(未更新)");
    return true;
}

// ========== SNTP ==========
static void init_sntp_once(void)
{
    if (!sntp_initialized) {
        setenv("TZ", "CST-8", 1);
        tzset();
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
        sntp_initialized = true;
        ESP_LOGI(TAG, "SNTP initialized");
    }
}

// ========== Wi-Fi 重连定时器 ==========
static void reconnect_cb(void *arg) {
    ESP_LOGI(TAG, "定时器触发, 尝试 Wi-Fi 重连");
    esp_wifi_connect();
}

static void init_reconnect_timer(void) {
    if (reconnect_timer_created) return;

    esp_timer_create_args_t timer_args = {
        .callback        = &reconnect_cb,
        .arg             = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "wifi_reconnect"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &reconnect_timer));
    reconnect_timer_created = true;
}

// ========== Wi-Fi 事件 ==========
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        network_ready = false;
        set_ip_raw(0);
        mqtt_app_stop();

        if (!esp_timer_is_active(reconnect_timer)) {
            ESP_LOGW(TAG, "Wi-Fi 掉线, 5 秒后重连");
            esp_timer_start_once(reconnect_timer, 5000000);  // 5 s
        }
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        set_ip_raw(event->ip_info.ip.addr);
        ESP_LOGI(TAG, "Wi-Fi got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        network_ready = true;
        init_sntp_once();
        mqtt_app_start();
        report_net_state_to_rs485();

        if (esp_timer_is_active(reconnect_timer)) {
            esp_timer_stop(reconnect_timer);
        }
    }
}

// ========== Ethernet 事件 ==========
static bool link_up = false;
static void eth_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data) {
    uint8_t mac_addr[6];
    esp_eth_handle_t handle = *(esp_eth_handle_t *)data;

    switch (id) {
        case ETHERNET_EVENT_CONNECTED:
            link_up = true;
            esp_eth_ioctl(handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet Link Up: %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2],
                     mac_addr[3], mac_addr[4], mac_addr[5]);
            break;

        case ETHERNET_EVENT_DISCONNECTED:
        case ETHERNET_EVENT_STOP:
            link_up = false;
            network_ready = false;
            set_ip_raw(0);
            mqtt_app_stop();
            break;

        default:
            break;
    }
}

TaskHandle_t net_guard_task_handle = nullptr;
void net_guard_task(void *arg);
static void got_ip_event_handler(void *arg, esp_event_base_t base,
                                 int32_t id, void *data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    set_ip_raw(event->ip_info.ip.addr);
    ESP_LOGI(TAG, "Ethernet got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    network_ready = true;
    init_sntp_once();
    mqtt_app_start();
    report_net_state_to_rs485();
    if (net_guard_task_handle == nullptr) {
        xTaskCreatePinnedToCore(net_guard_task, "net_guard_task", 4096, nullptr, 4, &net_guard_task_handle, 0);
    }
}

// ========== Wi-Fi 启动 ==========
void start_wifi_network(void) {
    current_net_type = NET_TYPE_WIFI;

    /* 1. 事件循环 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 2. netif & Wi-Fi 驱动 */
    wifi_event_group = xEventGroupCreate();
    netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, nullptr));

    /* 3. 读取 SSID / Pass */
    char ssid[33] = {};
    char pass[65] = {};
    load_wifi_credentials(ssid, pass);

    wifi_config_t wifi_cfg = {};
    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.ssid),     ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.password), pass, sizeof(wifi_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 4. 重连定时器 */
    init_reconnect_timer();

    /* 5. 等待首连（10 s 超时）*/
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                                           true, true, pdMS_TO_TICKS(10000));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "Wi-Fi 连接超时");
        generate_response(ORACLE, 0x09, 0x01, 0xEE, 0x00);  // 报告超时
    } else {
        ESP_LOGI(TAG, "Wi-Fi 连接成功");
    }
}

#define APP_RETRY_LIMIT          3              // MQTT 连续失败 N 次 → 尝试重启 ETH
#define APP_RETRY_WINDOW_MS      (7*60*1000)    // 上述计数的滑动窗口
#define ETH_RESTART_COOLDOWN_MS  (60*1000)      // stop/start 之间至少间隔 60 s
#define MCU_REBOOT_AFTER_N_ETH   3              // ETH 重启 N 次仍失败 → esp_restart()
static volatile uint8_t  eth_restart_times   = 0;
static uint64_t          last_eth_restart_ms = 0;
void net_guard_task(void *arg) {
    uint8_t  mqtt_fail  = 0;
    uint64_t first_fail = 0;

    for (;;) {
        if (!mqtt_connected) {

            /* -------- 应用层：先让 MQTT 自动重连 -------- */
            if (++mqtt_fail <= APP_RETRY_LIMIT) {
                if (!first_fail) first_fail = esp_timer_get_time()/1000;
                ESP_LOGW(TAG, "[%u/%u] 等待 MQTT 自动重连…",
                         mqtt_fail, APP_RETRY_LIMIT);
                goto delay;
            }

            /* 如果在滑动窗口外，重新计数 */
            if (esp_timer_get_time()/1000 - first_fail > APP_RETRY_WINDOW_MS) {
                mqtt_fail  = 1;
                first_fail = esp_timer_get_time()/1000;
                ESP_LOGW(TAG, "重新开始 MQTT 失败计数");
                goto delay;
            }

            /* -------- 驱动层：重启 ETH -------- */
            uint64_t now = esp_timer_get_time()/1000;
            if (now - last_eth_restart_ms < ETH_RESTART_COOLDOWN_MS) {
                ESP_LOGW(TAG, "ETH 冷却中… %llu ms",
                         ETH_RESTART_COOLDOWN_MS - (now - last_eth_restart_ms));
                goto delay;
            }

            ESP_LOGE(TAG, "MQTT 连续失败，重启以太网 (第 %u 次)",
                     ++eth_restart_times);

            esp_eth_stop(eth_handle);
            vTaskDelay(pdMS_TO_TICKS(300));      // PHY 彻底停
            esp_eth_start(eth_handle);

            last_eth_restart_ms = esp_timer_get_time()/1000;
            mqtt_fail = 0;                       // 归零等待新一轮

            /* -------- MCU 级：多次 ETH 仍失败 → 重启芯片 -------- */
            if (eth_restart_times >= MCU_REBOOT_AFTER_N_ETH) {
                ESP_LOGE(TAG, "ETH 重启 %u 次无果，触发 esp_restart()",
                         eth_restart_times);
                esp_restart();
            }
        }
        else {
            /* 一旦连上，一切计数清零 */
            mqtt_fail          = 0;
            first_fail         = 0;
            eth_restart_times  = 0;
        }

delay:
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

// ========== Ethernet 启动 ==========
// 只应该运行一次
void start_ethernet_network(void) {
    current_net_type = NET_TYPE_ETHERNET;

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 1. MAC / PHY / Driver */
    eth_esp32_emac_config_t mac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    mac_cfg.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    mac_cfg.clock_config.rmii.clock_gpio = EMAC_CLK_IN_GPIO;
    mac_cfg.smi_mdc_gpio_num = GPIO_NUM_23;
    mac_cfg.smi_mdio_gpio_num = GPIO_NUM_18;

    eth_mac_config_t eth_mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    
    eth_mac_cfg.sw_reset_timeout_ms = 1000;
    mac = esp_eth_mac_new_esp32(&mac_cfg, &eth_mac_cfg);
    if (!mac) { ESP_LOGE(TAG, "MAC 初始化失败"); return; }

    ESP_LOGD(TAG, "Initializing Ethernet PHY (LAN8720A) for WT32-ETH01...");
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;
    phy = esp_eth_phy_new_lan87xx(&phy_config);

    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_16, 1));
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    eth_handle = nullptr;
    // ESP_ERROR_CHECK(esp_eth_driver_install(&eth_cfg, &eth_handle));
    esp_eth_driver_install(&eth_cfg, &eth_handle);

    /* 2. netif + glue */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    netif = esp_netif_new(&netif_cfg);
    glue  = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(netif, glue));

    /* 3. 事件回调 */
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                               &eth_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                               &got_ip_event_handler, nullptr));
                                               
    /* 4. 启动 */
    // ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    esp_eth_start(eth_handle);
    // xTaskCreatePinnedToCore(net_guard_task, "net_guard_task", 2048, nullptr, 4, nullptr, 0);
    ESP_LOGI(TAG, "Ethernet started");
}

// ========== 上电恢复 ==========
void restore_last_network(void)
{
    nvs_handle_t handle;
    uint8_t saved_type = 0xFF;

    esp_err_t err = nvs_open("network", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, "net_type", &saved_type);
        nvs_close(handle);
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "未找到网络配置, 默认 Wi-Fi");
        start_wifi_network();
        return;
    }

    ESP_LOGI(TAG, "恢复上次网络类型: %d", saved_type);
    switch (static_cast<net_type_t>(saved_type)) {
        case NET_TYPE_WIFI:      start_wifi_network();      break;
        case NET_TYPE_ETHERNET:  start_ethernet_network();  break;
        default:                 start_wifi_network();      break;
    }
}

// ========== 对外工具 ==========
bool network_is_ready(void)           { return network_ready; }
net_type_t network_current_type(void) { return current_net_type; }

void set_ip_raw(uint32_t ip) { ip_raw = ip; }
uint32_t get_ip_raw(void)    { return ip_raw; }

/* 改网口 + 重启 */
void change_network_type_and_reboot(net_type_t new_type) {
    store_network_type(new_type);
    ESP_LOGW(TAG, "已写入新网络类型 %d, 即将重启...", new_type);
    vTaskDelay(pdMS_TO_TICKS(1000));   // 给日志一点时间
    esp_restart();
}


void bomb_task(void *arg) {
    static uint8_t big[1500];
    for (int i = 0; i < 2000; i++) {
        for (int i = 0; i < 4; i++) {
            esp_eth_transmit(eth_handle, big, sizeof(big));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelete(nullptr);
}


