#include <stdexcept>
#include <cstring>
#include <esp_partition.h>
#include <esp_log.h>
#include "identity.h"

#define TAG "IDENTITY"

bool isValidSerial(const char *serial) {
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
            // urgentPublishDebugLog("serial_num分区不存在");
            return "";
        }

        esp_err_t err = esp_partition_read(part, 0,  // offset=0
                                           serial_buf, SERIAL_LEN);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "读取serial_num分区失败, err=0x%x", err);
            // urgentPublishDebugLog("读取serial_num分区失败");
            return "";
        }

        serial_buf[SERIAL_LEN] = '\0';

        if (!isValidSerial(serial_buf)) {
            ESP_LOGW(TAG, "无效或缺失的serial number: '%s'", serial_buf);
            // urgentPublishDebugLog("无效或缺失的serial number");
            serial_buf[0] = '\0';
        }

        is_initialized = true;
    }

    return serial_buf;
}