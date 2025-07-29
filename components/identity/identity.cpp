#include <stdexcept>
#include <cstring>
#include <esp_partition.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include "identity.h"
#include "commons.h"

#define TAG "IDENTITY"

static bool isValidSerial(const char *serial) {
    if (!serial) return false;

    size_t len = strlen(serial);

    if (len != 8) return false;

    for (size_t i = 0; i < 8; ++i) {
        if (!isdigit((unsigned char)serial[i])) {
            return false;
        }
    }
    return true;
}

const char *getSerialNum() {
    static char serial_buf[SERIAL_LEN + 1] = {0};
    static bool is_initialized = false;

    if (!is_initialized) {
        const esp_partition_t *part = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,        // data
            (esp_partition_subtype_t)0x79,  // subtype=0x79
            SERIAL_PART_LABEL               // 名字=serial_num
        );
        if (!part) {
            ESP_LOGE(TAG, "serial_num分区不存在");
            urgentPublishDebugLog("serial_num分区不存在");
            return "";
        }

        esp_err_t err = esp_partition_read(part, 0,  // offset=0
                                           serial_buf, SERIAL_LEN);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "读取serial_num分区失败, err=0x%x", err);
            urgentPublishDebugLog("读取serial_num分区失败");
            return "";
        }

        serial_buf[SERIAL_LEN] = '\0';

        if (!isValidSerial(serial_buf)) {
            ESP_LOGW(TAG, "无效或缺失的serial number: '%s'", serial_buf);
            urgentPublishDebugLog("无效或缺失的serial number");
            serial_buf[0] = '\0';
        }

        is_initialized = true;
    }

    return serial_buf;
}

// 从nvs中获取酒店名与房号
// 如果某项不存在，则赋予默认值
void read_room_info_from_nvs(std::string &hotel_name, std::string &room_name) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 句柄失败: %s", esp_err_to_name(err));
        // 打开失败时直接赋默认值
        hotel_name = "defHotel";
        room_name = "defRoom";
        return;
    }

    size_t required_size = 0;
    // 读取 hotel_name
    err = nvs_get_str(handle, "hotel_name", NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char *hotel_buf = (char *)malloc(required_size);
        if (hotel_buf) {
            err = nvs_get_str(handle, "hotel_name", hotel_buf, &required_size);
            if (err == ESP_OK) {
                hotel_name = std::string(hotel_buf);
            } else {
                ESP_LOGE(TAG, "读取 hotel_name 失败: %s", esp_err_to_name(err));
                hotel_name = "DefaultHotelName";
            }
            free(hotel_buf);
        } else {
            ESP_LOGE(TAG, "分配内存失败");
            hotel_name = "DefaultHotelName";
        }
    } else {
        ESP_LOGI(TAG, "NVS 中没有找到 hotel_name, 使用默认值");
        hotel_name = "DefaultHotelName";
    }

    // 读取 room_name
    required_size = 0;
    err = nvs_get_str(handle, "room_name", NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char *room_buf = (char *)malloc(required_size);
        if (room_buf) {
            err = nvs_get_str(handle, "room_name", room_buf, &required_size);
            if (err == ESP_OK) {
                room_name = std::string(room_buf);
            } else {
                ESP_LOGE(TAG, "读取 room_name 失败: %s", esp_err_to_name(err));
                room_name = "DefaultRoomName";
            }
            free(room_buf);
        } else {
            ESP_LOGE(TAG, "分配内存失败");
            room_name = "DefaultRoomName";
        }
    } else {
        ESP_LOGI(TAG, "NVS 中没有找到 room_name, 使用默认值");
        room_name = "DefaultRoomName";
    }

    nvs_close(handle);
}