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
    // printCurrentFreeMemory("分割完");

    yyjson_doc *doc = yyjson_read(lines[1].data(), lines[1].size(), YYJSON_READ_NOFLAG);
    yyjson_val *root = yyjson_doc_get_root(doc);
    // printCurrentFreeMemory("yyjson后");

    if (yyjson_val *name = yyjson_obj_get(root, "tm")) {

    }

    try {
        json obj = json::parse(lines[1]);
        printCurrentFreeMemory("parse后");

        ESP_LOGI(TAG, "================ 解析全局配置 ================");
        auto& common_config_obj = obj["c"];
        auto& air_config_obj = common_config_obj["airConfig"];
        auto& air_config = AirConGlobalConfig::getInstance();
        air_config.default_target_temp = air_config_obj["defaultTargetTemp"];
        air_config.default_mode = static_cast<ACMode>(air_config_obj["defaultMode"]);
        air_config.default_fan_speed = static_cast<ACFanSpeed>(air_config_obj["defaultFanSpeed"]);
        air_config.stop_threshold = air_config_obj["stopThreshold"];
        air_config.rework_threshold = air_config_obj["reworkThreshold"];
        air_config.stop_action = static_cast<ACStopAction>(air_config_obj["stopAction"]);
        air_config.remove_card_air_usable = air_config_obj["removeCardAirUsable"];
        air_config.low_diff = air_config_obj["autoFan"]["lowFanTempDiff"];
        air_config.high_diff = air_config_obj["autoFan"]["highFanTempDiff"];
        air_config.shutdown_after_duration = air_config_obj["shutdownAfterDuration"];
        air_config.shutdown_after_fan_speed = static_cast<ACFanSpeed>(air_config_obj["shutdownAfterFanSpeed"]);

        // 注册设备
        ESP_LOGI(TAG, "================ 解析设备 ================");
        printCurrentFreeMemory();
        for (auto& dev: obj["d"]) {
            uint16_t did = dev["did"];
            DeviceType type = static_cast<DeviceType>(dev["type"]);
            std::string name = dev["n"];
            std::string carry_state = dev.value("ct", "");

            std::vector<uint16_t> link_dids;
            if (dev.contains("lkds") && dev["lkds"].is_array()) {
                for (auto& link_did : dev["lkds"]) {
                    uint16_t did = static_cast<uint16_t>(link_did.get<unsigned int>());
                    link_dids.push_back(did);
                }
            }

            std::vector<uint16_t> repel_dids;
            if (dev.contains("rpds") && dev["rpds"].is_array()) {
                for (auto& repel_did : dev["rpds"]) {
                    uint16_t did = static_cast<uint16_t>(repel_did.get<unsigned int>());
                    repel_dids.push_back(did);
                }
            }

            switch (type) {
                case DeviceType::LAMP: {
                    uint8_t ch = dev["ch"];
                    ESP_LOGI(TAG, "注册Lamp, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name.c_str(), ch, carry_state.c_str());
                    lord.registerLamp(did, name, carry_state, ch, link_dids, repel_dids);
                    break;
                }
                case DeviceType::CURTAIN: {
                    uint8_t oc = dev["oc"];
                    uint8_t cc = dev["cc"];
                    uint64_t rt = dev["rt"];
                    ESP_LOGI(TAG, "注册Curtain, did(%u), nm(%s), oc(%u), cc(%u), rt(%llu), st(%s)",
                                                did, name.c_str(), oc, cc, rt, carry_state.c_str());
                    lord.registerCurtain(did, name, carry_state,oc, cc, rt);
                    break;
                }
                case DeviceType::INFRARED_AIR: {
                    uint8_t airId = dev["aid"];
                    ESP_LOGI(TAG, "注册InfraredAir, did(%u), nm(%s), id(%u), st(%s)",
                                                    did, name.c_str(), airId, carry_state.c_str());
                    lord.registerIngraredAir(did, name, carry_state, airId);
                    break;
                }
                case DeviceType::SINGLE_AIR: {
                    uint8_t airId = dev["aid"];
                    uint8_t wc = dev["wc"];
                    uint8_t lc = dev["lc"];
                    uint8_t mc = dev["mc"];
                    uint8_t hc = dev["hc"];
                    ESP_LOGI(TAG, "注册SingleAir, did(%u), nm(%s), id(%u), wc(%u), lc(%u), mc(%u), hc(%u), st(%s)",
                                                 did, name.c_str(), airId, wc, lc, mc, hc, carry_state.c_str());
                    lord.registerSingleAir(did, name, carry_state, airId, wc, lc, mc, hc);
                    break;
                }
                case DeviceType::RS485: {
                    std::string code = dev["cd"];
                    ESP_LOGI(TAG, "注册RS485, did(%u), nm(%s), code(%s), st(%s)",
                                             did, name.c_str(), code.c_str(), carry_state.c_str());
                    lord.registerRs485(did, name, carry_state, code);
                    break;
                }
                case DeviceType::RELAY: {
                    uint8_t ch = dev["ch"];
                    ESP_LOGI(TAG, "注册RelayOut, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name.c_str(), ch, carry_state.c_str());
                    lord.registerRelayOut(did, name, carry_state, ch);
                    break;
                }
                case DeviceType::DRY_CONTACT: {
                    uint8_t ch = dev["ch"];
                    ESP_LOGI(TAG, "注册DryContactOut, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name.c_str(), ch, carry_state.c_str());
                    lord.registerDryContactOut(did, name, carry_state, ch, link_dids, repel_dids);
                    break;
                }
                case DeviceType::DOORBELL: {
                    uint8_t ch = dev["ch"];
                    ESP_LOGI(TAG, "注册门铃, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name.c_str(), ch, carry_state.c_str());
                    lord.registerRelayOut(did, name, carry_state, ch);
                    break;
                }
                case DeviceType::HEARTBEAT:
                case DeviceType::ROOM_STATE:
                case DeviceType::DELAYER:
                case DeviceType::ACTION_GROUP_OP:
                case DeviceType::SNAPSHOT:
                case DeviceType::INDICATOR: {
                    ESP_LOGI(TAG, "注册预设设备, did(%u), nm(%s)",
                                               did, name.c_str());
                    lord.registerPreset(did, name, carry_state, type);
                    break;
                }
                default:
                    ESP_LOGE(TAG, "未处理的设备类型: %i", (int)type);
                    break;
            }
        }
        // 解析动作组
        ESP_LOGI(TAG, "================ 解析自定义模式 ================");
        printCurrentFreeMemory();
        for (auto& actionGroup : obj["a"]) {
            std::string name = actionGroup["n"];
            uint16_t aid = actionGroup["aid"];
            bool is_mode = actionGroup.value("m", false);

            std::vector<AtomicAction> actions;
            for (auto& action : actionGroup["a"]) {
                uint16_t target_did = action["t"];
                IDevice* dev = lord.getDeviceByDid(target_did);
                if (dev != nullptr) {
                    actions.push_back(AtomicAction{
                        .target_device = dev,
                        .operation = action["o"],
                        .parameter = action.value("p", "")
                    });
                }
            }
            ESP_LOGI(TAG, "注册模式, aid(%u), nm(%s), is_mode(%u), size(%u)",
                                     aid, name.c_str(), is_mode, actions.size());
            lord.registerActionGroup(aid, name, is_mode, actions);
        }
        // 解析输入
        ESP_LOGI(TAG, "================ 解析输入 ================");
        printCurrentFreeMemory();
        for (auto& input: obj["i"]) {
            std::string name = input["n"];
            uint16_t iid = input["iid"];
            InputType type = static_cast<InputType>(input["type"]);
            InputTag tag = static_cast<InputTag>(input.value("tg", InputTag::NONE));

            std::vector<std::unique_ptr<ActionGroup>> action_groups;
            for (auto& action_group : input["a"]) {
                std::vector<AtomicAction> actions;
                for (auto& action : action_group) {
                    uint8_t target_did = action["t"];
                    IDevice* dev = lord.getDeviceByDid(target_did);
                    if (dev != nullptr) {
                        actions.push_back(AtomicAction{
                            .target_device = dev,
                            .operation = action["o"],
                            .parameter = action.value("p", "")
                        });
                    } else {
                        ESP_LOGW(TAG, "设备(%u)不存在", target_did);
                    }
                }
                // 基本上是幽灵动作组, 是Input私有的
                action_groups.push_back(std::make_unique<ActionGroup>(static_cast<uint16_t>(-1), "", false, actions));
            }

            if (type == InputType::PANEL_BTN) {
                uint8_t pid = input["pid"];
                uint8_t bid = input["bid"];
                // 如果有lightBindDevice, 就把此面板按键绑定给对应设备
                if (int lbd = input.value("lbd", -1); lbd > -1) {
                    if (IDevice* dev = lord.getDeviceByDid(lbd)) {
                        DeviceType dev_type = dev->getType();
                        if (dev_type == DeviceType::LAMP) {
                            if (Lamp* lamp = dynamic_cast<Lamp*>(dev)) {
                                lamp->addAssBtn(PanelButtonPair({pid, bid}));
                            }
                        } else if (dev_type == DeviceType::CURTAIN) {
                            if (Curtain* curtain = dynamic_cast<Curtain*>(dev)) {
                                // 遍历当前按键的所有动作组
                                for (auto& ag : action_groups) {
                                    for (auto& a : ag->actions) {
                                        if (a.operation == "开") {
                                            curtain->addOpenAssBtn(PanelButtonPair({pid, bid}));
                                        } else if (a.operation == "关") {
                                            curtain->addCloseAssBtn(PanelButtonPair({pid, bid}));
                                        }
                                    }
                                }
                            }
                        } else if (dev_type == DeviceType::RELAY) {
                            if (SingleRelayDevice* relay = dynamic_cast<SingleRelayDevice*>(dev)) {
                                relay->addAssBtn(PanelButtonPair({pid, bid}));
                            }
                        } else if (dev_type == DeviceType::DRY_CONTACT) {
                            if (DryContactOut* dry = dynamic_cast<DryContactOut*>(dev)) {
                                dry->addAssBtn(PanelButtonPair({pid, bid}));
                            }
                        } else {
                            ESP_LOGW(TAG, "无效的关联设备: %s(%u)", dev->getName().c_str(), dev->getDid());
                        }
                        ESP_LOGI(TAG, "绑定%u,%u至%s(%u)", pid, bid, dev->getName().c_str(), dev->getDid());
                    }
                }

                ESP_LOGI(TAG, "注册按键, iid(%u), nm(%s), pid(%u), bid(%u), tag(%d), g_size(%u)",
                                        iid, name.c_str(), pid, bid, (int)tag, action_groups.size());
                lord.registerPanelKeyInput(iid, name, tag, pid, bid, std::move(action_groups));

            } else if (type == InputType::DRY_CONTACT) {
                uint8_t channel = input["ch"];
                TriggerType tt = static_cast<TriggerType>(input["tt"]);
                uint64_t duration = 0;
                if (tt == TriggerType::INFRARED) {
                    duration = input["du"];
                }

                ESP_LOGI(TAG, "注册干接点输入, iid(%d), tp(%d), nm(%s), ch(%d), tt(%d), tag(%d), g_size(%d), du(%lld)",
                        iid, (int)type, name.c_str(), channel, (int)tt, (int)tag, action_groups.size(), duration);
                lord.registerDryContactInput(iid, type, name, tag, channel, tt, duration, std::move(action_groups));
            }
        }

        ESP_LOGI(TAG, "================ 配置解析完成 ================");
        IndicatorHolder::getInstance().callAllAndClear();               // 同步指示灯
        generate_response(AIR_CON, AIR_CON_INQUIRE, 0x00, 0x00, 0x00);  // 逼迫温控器上报状态

        printCurrentFreeMemory();
    } catch(const std::exception& e) {
        ESP_LOGE(TAG, "%s: %s, 内容: %s", __func__, e.what(), config_json.c_str());
    }
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
            json relay_obj;
            relay_obj["id"] = relay->getDid();
            relay_obj["name"] = relay->getName();
            relay_obj["state"] = relay->isOn() ? "1" : "0";
            j["others"].push_back(relay_obj);
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
            json relay_obj;
            relay_obj["id"] = relay->getDid();
            relay_obj["state"] = relay->isOn() ? 1 : 0;
            j["others"].push_back(relay_obj);
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
        j["states"] = getRoomStates();
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "生成状态报告时出错: %s", e.what());
    }

    return j;
}