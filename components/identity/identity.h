#pragma once

#include <string>

#define MODEL_NAME "xzrcu24"            // 板子型号
#define AETHORAC_VERSION "1.0.2"        // 固件版本

#define SERIAL_PART_LABEL  "serial_num"
#define SERIAL_LEN         8

#define LOGIC_CONFIG_FILE_PATH  "/littlefs/config.json"

const char *getSerialNum();
void read_room_info_from_nvs(std::string &hotel_name, std::string &room_name);