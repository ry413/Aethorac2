#include "json.hpp"
#include <esp_log.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <memory>
#include <esp_timer.h>

#include "lord_manager.h"
#include "identity.h"
#include "indicator.h"
#include "action_group.h"
#include "commons.h"
#include "room_state.h"
#include "lamp.h"
#include "relay_out.h"
#include "drycontact_out.h"
#include "curtain.h"
#include "air_conditioner.h"
#include <rs485_comm.h>
#include "yyjson.h"
#define TAG "JSON_CODEC"

using json = nlohmann::json;

std::vector<std::string_view> splitByLineView(std::string_view content) {
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start < content.size()) {
        size_t pos = content.find('\n', start);
        if (pos == std::string_view::npos) {
            lines.emplace_back(content.data() + start, content.size() - start);
            break;
        }
        lines.emplace_back(content.data() + start, pos - start);
        start = pos + 1;
    }
    return lines;
}

inline const char* json_get_str_safe(yyjson_val* obj, const char* key, const char* def = "") {
    yyjson_val *v = yyjson_obj_get(obj, key);
    return (v && yyjson_is_str(v)) ? yyjson_get_str(v) : def;
}

inline int json_get_int_safe(yyjson_val* obj, const char* key, int def = 0) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    return (v && yyjson_is_int(v)) ? yyjson_get_int(v) : def;
}

inline bool json_get_bool_safe(yyjson_val* obj, const char* key, bool def = false) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    return (v && yyjson_is_bool(v)) ? yyjson_get_bool(v) : def;
}

void parseLocalLogicConfig(void) {
    int file_fd = open(LOGIC_CONFIG_FILE_PATH, O_RDONLY);
    if (file_fd < 0) {
        ESP_LOGE(TAG, "打开本地配置文件失败: %s (%s)", LOGIC_CONFIG_FILE_PATH, strerror(errno));
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        ESP_LOGE(TAG, "获取文件状态失败: %s (%s)", LOGIC_CONFIG_FILE_PATH, strerror(errno));
        close(file_fd);
        return;
    }

    size_t file_size = file_stat.st_size;
    if (file_size <= 0) {
        ESP_LOGE(TAG, "文件为空: %s", LOGIC_CONFIG_FILE_PATH);
        close(file_fd);
        return;
    }

    // 读取文件内容
    std::string config_json(file_size, '\0');
    size_t total_read = 0;
    while (total_read < file_size) {
        ssize_t n = read(file_fd, &config_json[total_read], file_size - total_read);
        if (n < 0) {
            ESP_LOGE(TAG, "读取文件失败: %s (%s)", LOGIC_CONFIG_FILE_PATH, strerror(errno));
            close(file_fd);
            return;
        }
        if (n == 0) break;
        total_read += n;
    }
    close(file_fd);

    if (config_json.empty()) {
        ESP_LOGW(TAG, "本地没有配置文件");
        return;
    }
    printCurrentFreeMemory("读完文件");

    auto& lord = LordManager::instance();
    lord.clearAll();

    const auto lines = splitByLineView(config_json);
    if (lines.size() < 2) {
        ESP_LOGE(TAG, "本地配置文件错误");
        return;
    }
    
    ESP_LOGI(TAG, "================ 解析全局配置 ================");
    yyjson_doc* common_config_doc = yyjson_read(lines[2].data(), lines[2].size(), YYJSON_READ_NOFLAG);
    yyjson_val* common_config_root = yyjson_doc_get_root(common_config_doc);
    printCurrentFreeMemory();
    if (yyjson_val* common_config_obj = yyjson_obj_get(common_config_root, "c"); yyjson_is_obj(common_config_obj)) {
        // if (yyjson_val* air_config_obj = yyjson_obj_get(common_config_obj, "airConfig")) {
        //     auto& air_config = AirConGlobalConfig::getInstance();
        //     air_config.default_target_temp = json_get_int_safe(air_config_obj, "defaultTargetTemp", 26);
        //     air_config.default_mode = static_cast<ACMode>(json_get_int_safe(air_config_obj, "defaultMode", 0));
        //     air_config.default_fan_speed = static_cast<ACFanSpeed>(json_get_int_safe(air_config_obj, "defaultFanSpeed", 0));
        //     air_config.stop_threshold = json_get_int_safe(air_config_obj, "stopThreshold", 1);
        //     air_config.rework_threshold = json_get_int_safe(air_config_obj, "reworkThreshold", 1);
        //     air_config.stop_action = static_cast<ACStopAction>(json_get_int_safe(air_config_obj, "stopAction", 0));
        //     air_config.remove_card_air_usable = json_get_bool_safe(air_config_obj, "removeCardAirUsable", false);
        //     if (yyjson_val* auto_fan_obj = yyjson_obj_get(air_config_obj, "autoFan"); yyjson_is_obj(auto_fan_obj)) {
        //         air_config.low_diff = json_get_int_safe(auto_fan_obj, "lowFanTempDiff", 2);
        //         air_config.high_diff = json_get_int_safe(auto_fan_obj, "highFanTempDiff", 2);
        //         air_config.auto_fun_wind_speed = static_cast<ACFanSpeed>(json_get_int_safe(auto_fan_obj, "autoVentFanSpeed", 1));
        //     } else {
        //         ESP_LOGW(TAG, "配置[autoFan]错误");
        //     }
        //     air_config.shutdown_after_duration = json_get_int_safe(air_config_obj, "shutdownAfterDuration", 0);
        //     air_config.shutdown_after_fan_speed = static_cast<ACFanSpeed>(json_get_int_safe(air_config_obj, "shutdownAfterFanSpeed", 0));
        // } else {
        //     ESP_LOGW(TAG, "配置[airConfig]错误");
        // }
    } else {
        ESP_LOGW(TAG, "配置[c]错误");
    }
    yyjson_doc_free(common_config_doc);

    ESP_LOGI(TAG, "================ 解析设备 ================");
    printCurrentFreeMemory();
    yyjson_doc* devices_config_doc = yyjson_read(lines[3].data(), lines[3].size(), YYJSON_READ_NOFLAG);
    yyjson_val* devices_config_root = yyjson_doc_get_root(devices_config_doc);
    if (yyjson_val* devices_arr = yyjson_obj_get(devices_config_root, "d"); yyjson_is_arr(devices_arr)) {
        size_t idx, max;
        yyjson_val* dev_obj;

        yyjson_arr_foreach(devices_arr, idx, max, dev_obj) {
            if (!dev_obj || !yyjson_is_obj(dev_obj)) continue;

            DeviceType dtype = static_cast<DeviceType>(json_get_int_safe(dev_obj, "type", (int)DeviceType::NONE));
            uint16_t did = json_get_int_safe(dev_obj, "did", -1);
            if (dtype == DeviceType::NONE) {
                ESP_LOGW(TAG, "错误的设备类型, did(%u)", did);
            }

            const char* name = json_get_str_safe(dev_obj, "n", "");
            const char* carry_state = json_get_str_safe(dev_obj, "ct", "");

            std::vector<uint16_t> link_dids;
            if (yyjson_val* lkds_arr = yyjson_obj_get(dev_obj, "lkds"); yyjson_is_arr(lkds_arr)) {
                size_t idx1, max1;
                yyjson_val* item1;
                yyjson_arr_foreach(lkds_arr, idx1, max1, item1) {
                    if (item1 && yyjson_is_int(item1)) {
                        link_dids.push_back(yyjson_get_int(item1));
                    }
                }
            }

            std::vector<uint16_t> repel_dids;
            if (yyjson_val* rpds_arr = yyjson_obj_get(dev_obj, "rpds"); yyjson_is_arr(rpds_arr)) {
                size_t idx1, max1;
                yyjson_val* item1;
                yyjson_arr_foreach(rpds_arr, idx1, max1, item1) {
                    if (item1 && yyjson_is_int(item1)) {
                        repel_dids.push_back(yyjson_get_int(item1));
                    }
                }
            }

            switch (dtype) {
                case DeviceType::LAMP: {
                    uint8_t ch = json_get_int_safe(dev_obj, "ch", 127);
                    ESP_LOGI(TAG, "注册Lamp, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name, ch, carry_state);
                    lord.registerLamp(did, name, carry_state, ch, link_dids, repel_dids);
                    break;
                }
                case DeviceType::CURTAIN: {
                    uint8_t oc = json_get_int_safe(dev_obj, "oc", 127);
                    uint8_t cc = json_get_int_safe(dev_obj, "cc", 127);
                    uint64_t rt = json_get_int_safe(dev_obj, "rt", 10);
                    ESP_LOGI(TAG, "注册Curtain, did(%u), nm(%s), oc(%u), cc(%u), rt(%llu), st(%s)",
                                                did, name, oc, cc, rt, carry_state);
                    lord.registerCurtain(did, name, carry_state,oc, cc, rt);
                    break;
                }
                case DeviceType::INFRARED_AIR: {
                    uint8_t airId = json_get_int_safe(dev_obj, "aid", 0);
                    ESP_LOGI(TAG, "注册InfraredAir, did(%u), nm(%s), id(%u), st(%s)",
                                                    did, name, airId, carry_state);
                    lord.registerIngraredAir(did, name, carry_state, airId);
                    break;
                }
                case DeviceType::SINGLE_AIR: {
                    uint8_t airId = json_get_int_safe(dev_obj, "aid", 0);
                    uint8_t wc = json_get_int_safe(dev_obj, "wc", 127);
                    uint8_t lc = json_get_int_safe(dev_obj, "lc", 127);
                    uint8_t mc = json_get_int_safe(dev_obj, "mc", 127);
                    uint8_t hc = json_get_int_safe(dev_obj, "hc", 127);
                    ESP_LOGI(TAG, "注册SingleAir, did(%u), nm(%s), id(%u), wc(%u), lc(%u), mc(%u), hc(%u), st(%s)",
                                                    did, name, airId, wc, lc, mc, hc, carry_state);
                    lord.registerSingleAir(did, name, carry_state, airId, wc, lc, mc, hc);
                    break;
                }
                case DeviceType::RS485: {
                    const char* code = json_get_str_safe(dev_obj, "cd", "");
                    ESP_LOGI(TAG, "注册RS485, did(%u), nm(%s), code(%s), st(%s)",
                                                did, name, code, carry_state);
                    lord.registerRs485(did, name, carry_state, code);
                    break;
                }
                case DeviceType::RELAY: {
                    uint8_t ch = json_get_int_safe(dev_obj, "ch", 127);
                    ESP_LOGI(TAG, "注册RelayOut, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name, ch, carry_state);
                    lord.registerRelayOut(did, name, carry_state, ch, link_dids, repel_dids);
                    break;
                }
                case DeviceType::DRY_CONTACT: {
                    uint8_t ch = json_get_int_safe(dev_obj, "ch", 127);
                    ESP_LOGI(TAG, "注册DryContactOut, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name, ch, carry_state);
                    lord.registerDryContactOut(did, name, carry_state, ch, link_dids, repel_dids);
                    break;
                }
                case DeviceType::DOORBELL: {
                    uint8_t ch = json_get_int_safe(dev_obj, "ch", 127);
                    ESP_LOGI(TAG, "注册门铃, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name, ch, carry_state);
                    lord.registerRelayOut(did, name, carry_state, ch, link_dids, repel_dids);
                    break;
                }
                case DeviceType::HEARTBEAT:
                case DeviceType::ROOM_STATE:
                case DeviceType::DELAYER:
                case DeviceType::ACTION_GROUP_OP:
                case DeviceType::SNAPSHOT:
                case DeviceType::INDICATOR: {
                    ESP_LOGI(TAG, "注册预设设备, did(%u), nm(%s)",
                                                did, name);
                    lord.registerPreset(did, name, carry_state, dtype);
                    break;
                }
                default:
                    ESP_LOGE(TAG, "未处理的设备类型: %i", (int)dtype);
                    break;
            }
        }
    } else {
        ESP_LOGW(TAG, "配置[d]错误");
    }
    yyjson_doc_free(devices_config_doc);

    ESP_LOGI(TAG, "================ 解析自定义模式 ================");
    printCurrentFreeMemory();
    yyjson_doc* action_groups_config_doc = yyjson_read(lines[4].data(), lines[4].size(), YYJSON_READ_NOFLAG);
    yyjson_val* action_groups_config_root = yyjson_doc_get_root(action_groups_config_doc);
    if (yyjson_val* action_groups_arr = yyjson_obj_get(action_groups_config_root, "a"); yyjson_is_arr(action_groups_arr)) {
        size_t idx, max;
        yyjson_val* ag_obj;

        yyjson_arr_foreach(action_groups_arr, idx, max, ag_obj) {
            if (!ag_obj || !yyjson_is_obj(ag_obj)) continue;
            const char* name = json_get_str_safe(ag_obj, "n", "");
            uint16_t aid = json_get_int_safe(ag_obj, "aid", -1);
            bool is_mode = json_get_bool_safe(ag_obj, "m", false);

            std::vector<AtomicAction> actions;
            if (yyjson_val* action_arr = yyjson_obj_get(ag_obj, "a"); yyjson_is_arr(action_arr)) {
                size_t idx1, max1;
                yyjson_val* item1;
                yyjson_arr_foreach(action_arr, idx1, max1, item1) {
                    if (!item1 || !yyjson_is_obj(item1)) continue;
                    uint16_t target_did = json_get_int_safe(item1, "t", -1);
                    if (IDevice* dev = lord.getDeviceByDid(target_did)) {
                        actions.push_back(AtomicAction{
                            .target_device = dev,
                            .operation = json_get_str_safe(item1, "o", ""),
                            .parameter = json_get_str_safe(item1, "p", "")
                        });
                    } else {
                        ESP_LOGW(TAG, "设备(%u)不存在", target_did);
                    }
                }
            } else {
                ESP_LOGE(TAG, "配置[a][a]错误");
            }
            ESP_LOGI(TAG, "注册模式, aid(%u), nm(%s), is_mode(%u), size(%u)",
                                     aid, name, is_mode, actions.size());
            lord.registerActionGroup(aid, name, is_mode, actions);
        }
    } else {
        ESP_LOGW(TAG, "配置[a]错误");
    }
    yyjson_doc_free(action_groups_config_doc);

    ESP_LOGI(TAG, "================ 解析输入 ================");
    printCurrentFreeMemory();
    yyjson_doc* inputs_config_doc = yyjson_read(lines[5].data(), lines[5].size(), YYJSON_READ_NOFLAG);
    yyjson_val* inputs_config_root = yyjson_doc_get_root(inputs_config_doc);
    if (yyjson_val* inputs_arr = yyjson_obj_get(inputs_config_root, "i"); yyjson_is_arr(inputs_arr)) {
        size_t idx, max;
        yyjson_val* input_obj;
        yyjson_arr_foreach(inputs_arr, idx, max, input_obj) {
            if (!input_obj || !yyjson_is_obj(input_obj)) continue;
            const char* name = json_get_str_safe(input_obj, "n", "");
            uint16_t iid = json_get_int_safe(input_obj, "iid", -1);
            InputType itype = static_cast<InputType>(json_get_int_safe(input_obj, "type", (int)InputType::NONE));
            // InputTag tag = static_cast<InputTag>(json_get_int_safe(input_obj, "tg", (int)InputTag::NONE));
            std::set<InputTag> tags_set;
            if (yyjson_val* tgs_arr = yyjson_obj_get(input_obj, "tgs"); yyjson_is_arr(tgs_arr)) {
                size_t idx1, max1;
                yyjson_val* item1;
                yyjson_arr_foreach(tgs_arr, idx1, max1, item1) {
                    if (item1 && yyjson_is_int(item1)) {
                        tags_set.insert(static_cast<InputTag>(yyjson_get_int(item1)));
                    }
                }
            }
            
            std::vector<std::unique_ptr<ActionGroup>> action_groups;
            if (yyjson_val* ag_arr = yyjson_obj_get(input_obj, "a"); yyjson_is_arr(ag_arr)) {
                size_t idx1, max1;
                yyjson_val* actions_arr;
                yyjson_arr_foreach(ag_arr, idx1, max1, actions_arr) {
                    if (!actions_arr || !yyjson_is_arr(actions_arr)) continue;
                    size_t idx2, max2;
                    yyjson_val* action_obj;
                    std::vector<AtomicAction> actions;
                    yyjson_arr_foreach(actions_arr, idx2, max2, action_obj) {
                        if (!action_obj || !yyjson_is_obj(action_obj)) continue;
                        uint16_t target_did = json_get_int_safe(action_obj, "t", -1);
                        if (IDevice* dev = lord.getDeviceByDid(target_did)) {
                            actions.push_back(AtomicAction{
                                .target_device = dev,
                                .operation = json_get_str_safe(action_obj, "o", ""),
                                .parameter = json_get_str_safe(action_obj, "p", "")
                            });
                        } else {
                            ESP_LOGW(TAG, "设备(%u)不存在", target_did);
                        }
                    }
                    action_groups.push_back(std::make_unique<ActionGroup>(static_cast<uint16_t>(-1), "", false, actions));
                }
            } else {
                ESP_LOGE(TAG, "配置[i][a]错误, iid: %u", iid);
            }

            if (itype == InputType::PANEL_BTN) {
                uint8_t pid = json_get_int_safe(input_obj, "pid", -1);;
                uint8_t bid = json_get_int_safe(input_obj, "bid", -1);;
                // 如果有lightBindDevice, 就把此面板按键绑定给对应设备
                if (int lbd = json_get_int_safe(input_obj, "lbd", -1); lbd > -1) {
                    if (IDevice* dev = lord.getDeviceByDid(lbd)) {
                        DeviceType dev_type = dev->getType();
                        if (dev_type == DeviceType::LAMP) {
                            if (Lamp* lamp = dynamic_cast<Lamp*>(dev)) {
                                lamp->addAssBtn(PanelButtonPair({pid, bid}));
                                ESP_LOGI(TAG, "绑定%u,%u至%s(%u)", pid, bid, dev->getName().c_str(), dev->getDid());
                            }
                        } else if (dev_type == DeviceType::CURTAIN) {
                            if (Curtain* curtain = dynamic_cast<Curtain*>(dev)) {
                                // 遍历当前按键的所有动作组
                                for (auto& ag : action_groups) {
                                    for (auto& a : ag->actions) {
                                        if (a.operation == "开") {
                                            curtain->addOpenAssBtn(PanelButtonPair({pid, bid}));
                                            ESP_LOGI(TAG, "绑定%u,%u至%s(%u) 开", pid, bid, dev->getName().c_str(), dev->getDid());
                                        } else if (a.operation == "关") {
                                            curtain->addCloseAssBtn(PanelButtonPair({pid, bid}));
                                            ESP_LOGI(TAG, "绑定%u,%u至%s(%u) 关", pid, bid, dev->getName().c_str(), dev->getDid());
                                        }
                                    }
                                }
                            }
                        } else if (dev_type == DeviceType::RELAY) {
                            if (SingleRelayDevice* relay = dynamic_cast<SingleRelayDevice*>(dev)) {
                                relay->addAssBtn(PanelButtonPair({pid, bid}));
                                ESP_LOGI(TAG, "绑定%u,%u至%s(%u)", pid, bid, dev->getName().c_str(), dev->getDid());
                            }
                        } else if (dev_type == DeviceType::DRY_CONTACT) {
                            if (DryContactOut* dry = dynamic_cast<DryContactOut*>(dev)) {
                                dry->addAssBtn(PanelButtonPair({pid, bid}));
                                ESP_LOGI(TAG, "绑定%u,%u至%s(%u)", pid, bid, dev->getName().c_str(), dev->getDid());
                            }
                        } else {
                            ESP_LOGW(TAG, "无效的关联设备: %s(%u)", dev->getName().c_str(), dev->getDid());
                        }
                    }
                }
                ESP_LOGI(TAG, "注册按键, iid(%u), nm(%s), pid(%u), bid(%u), tags_size(%d), g_size(%u)",
                                        iid, name, pid, bid, tags_set.size(), action_groups.size());
                lord.registerPanelKeyInput(iid, name, tags_set, pid, bid, std::move(action_groups));
            } else if (itype == InputType::DRY_CONTACT) {
                uint8_t channel = json_get_int_safe(input_obj, "ch");
                TriggerType tt = static_cast<TriggerType>(json_get_int_safe(input_obj, "tt", (int)TriggerType::NONE));
                uint64_t duration = 1;
                if (tt == TriggerType::INFRARED) {
                    duration = json_get_int_safe(input_obj, "du", 1);
                }

                ESP_LOGI(TAG, "注册干接点输入, iid(%d), tp(%d), nm(%s), ch(%d), tt(%d), tags_size(%d), g_size(%d), du(%lld)",
                        iid, (int)itype, name, channel, (int)tt, tags_set.size(), action_groups.size(), duration);
                lord.registerDryContactInput(iid, name, tags_set, channel, tt, duration, std::move(action_groups));
            } else if (itype == InputType::VOICE_CMD) {
                const char* code = json_get_str_safe(input_obj, "cd");
                ESP_LOGI(TAG, "注册语音指令输入, iid(%d), tp(%d), nm(%s), code(%s), tags_size(%d), g_size(%d)",
                        iid, (int)itype, name, code, tags_set.size(), action_groups.size());
                lord.registerVoiceInput(iid, name, tags_set, code, std::move(action_groups));
            }
        }
    } else {
        ESP_LOGW(TAG, "配置[i]错误");
    }
    yyjson_doc_free(inputs_config_doc);
    ESP_LOGI(TAG, "================ 配置解析完成 ================");
    IndicatorHolder::getInstance().callAllAndClear();               // 同步指示灯
    generate_response(AIR_CON, AIR_CON_INQUIRE_XZ, 0x00, 0x00, 0x00);  // 逼迫温控器上报状态

    printCurrentFreeMemory();
}

json generateRegisterInfo() {
    json j;
    auto& lord = LordManager::instance();
    try {
        j["mac"] = getSerialNum();
        j["type"] = "regis";

        auto& config_version = lord.getConfigGenerageTime();
        j["config_version"] = config_version;

        std::string hotel_name, room_name;
        read_room_info_from_nvs(hotel_name, room_name);
        j["hotel_name"] = hotel_name;
        j["room_name"] = room_name;

        int64_t uptime_us = esp_timer_get_time();
        int64_t uptime_s = uptime_us / 1000000;
        j["run_time"] = uptime_s;

        j["fireware_version"] = AETHORAC_VERSION;
        j["model_name"] = MODEL_NAME;

        // 各种设备信息
        j["lights"] = json::array();
        for(auto* lamp : lord.getDevicesByType<Lamp>()) {
            json lamp_obj;
            lamp_obj["id"] = std::to_string(lamp->getDid());
            lamp_obj["name"] = lamp->getName();
            lamp_obj["state"] = lamp->isOn() ? "1" : "0";
            j["lights"].push_back(lamp_obj);
        }

        j["curtains"] = json::array();
        for (auto* curtain : lord.getDevicesByType<Curtain>()) {
            json cur_obj;
            cur_obj["id"] = std::to_string(curtain->getDid());
            cur_obj["name"] = curtain->getName();
            cur_obj["state"] = curtain->isOn() ? "1" : "0";
            j["curtains"].push_back(cur_obj);
        }
        
        j["others"] = json::array();
        for (SingleRelayDevice* relay : lord.getDevicesByType<SingleRelayDevice>()) {
            if (dynamic_cast<Lamp*>(relay)) {

            } else {
                json relay_obj;
                relay_obj["id"] = relay->getDid();
                relay_obj["name"] = relay->getName();
                relay_obj["state"] = relay->isOn() ? "1" : "0";
                j["others"].push_back(relay_obj);
            }
        }
        for (DryContactOut* dry : lord.getDevicesByType<DryContactOut>()) {
            json dry_obj;
            dry_obj["id"] = dry->getDid();
            dry_obj["name"] = dry->getName();
            dry_obj["state"] = dry->isOn() ? "1" : "0";
            j["others"].push_back(dry_obj);
        }

        j["airs"] = json::array();
        for (auto* air : lord.getDevicesByType<AirConBase>()) {
            json air_obj;
            air_obj["id"] = std::to_string(air->getDid());
            air_obj["air_id"] = std::to_string(air->getAcId());
            air_obj["name"] = air->getName();
            air_obj["state"] = air->isOn() ? "1" : "0";
            air_obj["mode"] = std::to_string(static_cast<int>(air->get_mode()));
            air_obj["fan_speed"] = std::to_string(static_cast<int>(air->get_fan_speed()));
            air_obj["target_temp"] = std::to_string(air->get_target_temp());
            j["airs"].push_back(air_obj);
        }

        j["modes"] = json::array();
        for (auto* mode : lord.getAllModeActionGroup()) {
            j["modes"].push_back(mode->getName());
        }
    } catch(const std::exception& e) {
        ESP_LOGW(TAG, "生成注册信息时出错: %s", e.what());
    }

    return j;
}

json generateReportStates() {
    json j;
    auto& lord = LordManager::instance();
    try {
        j["mac"] = getSerialNum();
        j["type"] = "alldevicestate";

        j["lights"] = json::array();
        for (Lamp* lamp : lord.getDevicesByType<Lamp>()) {
            json lamp_obj;
            lamp_obj["id"] = lamp->getDid();
            lamp_obj["state"] = lamp->isOn() ? 1 : 0;
            j["lights"].push_back(lamp_obj);
        }

        j["curtains"] = json::array();
        for (Curtain* cur : lord.getDevicesByType<Curtain>()) {
            json cur_obj;
            cur_obj["id"] = cur->getDid();
            cur_obj["state"] = cur->isOn() ? 1 : 0;
            j["curtains"].push_back(cur_obj);
        }

        j["others"] = json::array();
        for (SingleRelayDevice* relay : lord.getDevicesByType<SingleRelayDevice>()) {
            if (dynamic_cast<Lamp*>(relay)) {

            } else {
                json relay_obj;
                relay_obj["id"] = relay->getDid();
                relay_obj["state"] = relay->isOn() ? 1 : 0;
                j["others"].push_back(relay_obj);
            }
        }
        for (DryContactOut* dry : lord.getDevicesByType<DryContactOut>()) {
            json dry_obj;
            dry_obj["id"] = dry->getDid();
            dry_obj["state"] = dry->isOn() ? 1 : 0;
            j["others"].push_back(dry_obj);
        }

        j["airs"] = json::array();
        for (AirConBase* air : lord.getDevicesByType<AirConBase>()) {
            json air_obj;
            air_obj["id"] = air->getDid();
            air_obj["state"] = air->isOn() ? 1 : 0;
            air_obj["mode"] = static_cast<int>(air->get_mode());
            air_obj["fan_speed"] = static_cast<int>(air->get_fan_speed());
            air_obj["target_temp"] = air->get_target_temp();
            air_obj["room_temp"] = air->get_room_temp();
            j["airs"].push_back(air_obj);
        }

        j["mode"] = lord.getLastModeName();
        j["states"] = get_states_json();
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "生成状态报告时出错: %s", e.what());
    }

    return j;
}