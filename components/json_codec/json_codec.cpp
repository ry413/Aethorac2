#include "json.hpp"
#include <esp_log.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <memory>
#include <esp_timer.h>

#include "lord_manager.h"
#include "identity.h"
#include "action_group.h"
#include "commons.h"
#include "room_state.h"
#include "lamp.h"
#include "curtain.h"
#include "air_conditioner.h"


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
    // print_free_memory("开始解析ndjson");
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

    auto& lord = LordManager::instance();
    lord.clearAll();
    // printf("config_json: %s\n", config_json.c_str());

    // DeviceManager::getInstance().clear();
    // BoardManager::getInstance().clear();
    // ActionGroupManager::getInstance().clear();
    // PanelManager::getInstance().clear();
    // VoiceCommandManager::getInstance().clear();
    // LordManager::getInstance().clear();
    const auto lines = splitByLineView(config_json);
    if (lines.size() < 2) {
        ESP_LOGE(TAG, "本地配置文件错误");
        return;
    }

    try {
        json obj = json::parse(lines[1]);

        // 注册设备
        ESP_LOGI(TAG, "================ 解析设备 ================");
        printCurrentFreeMemory();
        for (auto& dev: obj["d"]) {
            uint16_t did = dev["did"];
            DeviceType type = static_cast<DeviceType>(dev["type"]);
            std::string name = dev["n"];
            std::string carry_state = dev.value("ct", "");

            switch (type) {
                case DeviceType::LAMP: {
                    uint8_t ch = dev["ch"];
                    ESP_LOGI(TAG, "注册Lamp, did(%u), nm(%s), ch(%u), st(%s)",
                                            did, name.c_str(), ch, carry_state.c_str());
                    lord.registerLamp(did, name, carry_state, ch);
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
                    lord.registerDryContactOut(did, name, carry_state, ch);
                    break;
                }
                case DeviceType::HEARTBEAR:
                case DeviceType::ROOM_STATE:
                case DeviceType::DELAYER:
                case DeviceType::ACTION_GROUP_OP:
                case DeviceType::SNAPSHOT: {
                    ESP_LOGI(TAG, "注册预设设备, did(%u), nm(%s)",
                                               did, name.c_str());
                    lord.registerPreset(did, name, carry_state, type);
                    break;
                }
                default:
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
                        } else if (dev->getType() == DeviceType::CURTAIN) {
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
        printCurrentFreeMemory();
    } catch(const std::exception& e) {
        ESP_LOGE(TAG, "%s: %s", __func__, e.what());
    }
    // std::string board_config;
    // const auto lines = splitByLineView(json_str);
    // for (auto& line : lines) {
    //     json obj = json::parse(line);
    //     if (obj.size() != 1) {
    //         ESP_LOGE(TAG, "该行不是单键: %.*s", line.size(), line.data());
    //         return;
    //     }

    //     auto it = obj.begin();
    //     const std::string& key = it.key();
    //     const json& value = it.value();

    //     if (key == "type") {
    //         std::string type = value.get<std::string>();
    //         if (type != "Laminor") {
    //             ESP_LOGE(TAG, "错误的数据类型: %s", type.c_str());
    //             return;
    //         }
    //     } else if (key == "适配版本") {
    //         std::string fit_version = value.get<std::string>();

    //         // if(fit_version != FIT_LAMINOR_VERSION) {
    //         //     char buf[64];
    //         //     snprintf(buf, sizeof(buf), "版本不适配: %s / %s", fit_version.c_str(), FIT_LAMINOR_VERSION);
    //         //     ESP_LOGE(TAG, "%s", buf);
    //         //     publish_debug_log(buf);
    //         //     return;
    //         // }
    //     } else if (key == "一般配置") {
    //         print_free_memory("一般");
    //         if (value.is_object()) {
    //             if (value.contains("config_version") && value["config_version"].is_string()) {
    //                 std::string config_version = value.value("config_version", "");
    //                 LordManager::getInstance().setVersion(config_version);
    //                 ESP_LOGI(TAG, "配置版本: %s", config_version.c_str());
    //             }
    //             // std::string wait_die_state;
    //             // if (value.contains("waitDieStateName") && value["waitDieStateName"].is_string()) {
    //             //     wait_die_state = value.value("waitDieStateName", "");
    //             //     if (!wait_die_state.empty()) {
    //             //         LordManager::getInstance().setWaitToDieState(wait_die_state);

    //             //         int wait_to_die_sec = 0;
    //             //         if (value.contains("waitDieSec") && value["waitDieSec"].is_string()) {
    //             //             wait_to_die_sec = std::stoi(value.value("waitDieSec", ""));
    //             //         }
    //             //         if (wait_to_die_sec > 0) {
    //             //             LordManager::getInstance().setWaitToDieSec(wait_to_die_sec);
                            
    //             //             // xTaskCreate([](void *param) {
    //             //             //     const std::string& state = LordManager::getInstance().getWaitToDieState();
    //             //             //     int sec = LordManager::getInstance().getWaitToDieSec();
    //             //             //     while (true) {
    //             //             //         vTaskDelay(sec / portTICK_PERIOD_MS);
    //             //             //         if (!exist_state(state)) {
    //             //             //             // 不在指定状态若干秒后, 熄灭所有背光并进入拔卡状态
    //             //             //             auto all_panel = PanelManager::getInstance().getAllItems();
    //             //             //             for (const auto& panel : all_panel) {   
    //             //             //                 panel.second->turn_prere_all_buttons();
    //             //             //                 panel.second->publish_bl_state();
    //             //             //             }
    //             //             //             vTaskDelay(300 / portTICK_PERIOD_MS);
    //             //             //             sleep_heartbeat();
    //             //             //             set_alive(false);
    //             //             //         }
    //             //             //     }
    //             //             // }, "wait_die_task", 4096, nullptr, 3, nullptr);
    //             //         }
    //             //     }
    //             // }
                
    //             if (value.contains("alive_channel") && value["alive_channel"].is_string()) {
    //                 const std::string& channelStr = value["alive_channel"];
    //                 try {
    //                     unsigned int channel = std::stoul(channelStr);
    //                     LordManager::getInstance().setAliveChannel(channel);
    //                 } catch (const std::exception& e) {
    //                     ESP_LOGE(TAG, "Invalid alive_channel value: %s, error: %s", channelStr.c_str(), e.what());
    //                 }
    //             }

    //             if (value.contains("door_channel") && value["door_channel"].is_string()) {
    //                 const std::string& channelStr = value["door_channel"];
    //                 try {
    //                     unsigned int channel = std::stoul(channelStr);
    //                     LordManager::getInstance().setDoorChannel(channel);
    //                 } catch (const std::exception& e) {
    //                     ESP_LOGE(TAG, "Invalid door_channel value: %s, error: %s", channelStr.c_str(), e.what());
    //                 }
    //             }
    //         }
    //     } else if (key == "板子列表") {
    //         ESP_LOGI(TAG, "解析 板子 配置");
    //         print_free_memory("板子");
    //         board_config = line;    // 保存板子配置行, 等到for结束后解析板子输入
            
    //         if (value.is_array()) {
    //             // 遍历数组中的每个 JSON 对象
    //             for (auto& board_item : value) {
    //                 auto board = std::make_shared<BoardConfig>();
    //                 board->id = board_item.value("id", 0u);
                    
    //                 ESP_LOGI(TAG, "解析 板 %d 输出列表", board->id);
    //                 if (board_item.contains("os") && board_item["os"].is_array()) {
    //                     for (auto& output_item : board_item["os"]) {
    //                         auto output = std::make_shared<BoardOutput>();
    //                         output->host_board_id = output_item.value("hBId", 0u);
    //                         output->type = static_cast<OutputType>(output_item.value("tp", 0));
    //                         output->channel = output_item.value("ch", 0u);
    //                         output->uid = output_item.value("uid", 0u);
    //                         board->outputs[output->uid] = output;
    //                     }
    //                 }
    //                 BoardManager::getInstance().addItem(board->id, board);
    //             }
    //         }
    //     } else if (key == "灯列表") {
    //         ESP_LOGI(TAG, "解析 灯 配置");
    //         print_free_memory("灯");
    //         if (value.is_array()) {
    //             for (auto& lamp_item : value) {
    //                 auto lamp = std::make_shared<Lamp>();
    //                 lamp->uid = lamp_item.value("uid", 0u);
    //                 lamp->type = static_cast<LampType>(lamp_item.value("tp", 0));
    //                 lamp->name = lamp_item.value("nm", "");
    //                 lamp->output = BoardManager::getInstance().getBoardOutput(lamp_item.value("oUid", 0u));
    //                 if (lamp->output == nullptr) {
    //                     ESP_LOGE(TAG, "灯 %s 没有输出\n", lamp->name.c_str());
    //                 }
    //                 // 影响状态
    //                 if (lamp_item.contains("cauSt")) {
    //                     lamp->cause_state = lamp_item["cauSt"].get<std::string>();
    //                 }
    //                 // 联动设备
    //                 if (lamp_item.contains("linkDUids") && lamp_item["linkDUids"].is_array()) {
    //                     for (auto& link_device_uid : lamp_item["linkDUids"]) {
    //                         uint16_t uid = static_cast<uint16_t>(link_device_uid.get<unsigned int>());
    //                         lamp->link_device_uids.push_back(uid);
    //                     }
    //                 }
    //                 // 排斥设备
    //                 if (lamp_item.contains("repelDUids") && lamp_item["repelDUids"].is_array()) {
    //                     for (auto& repel_device_uid : lamp_item["repelDUids"]) {
    //                         uint16_t uid = static_cast<uint16_t>(repel_device_uid.get<unsigned int>());
    //                         lamp->repel_device_uids.push_back(uid);
    //                     }
    //                 }
    //                 // 关联按钮
    //                 if (lamp_item.contains("assBtns") && lamp_item["assBtns"].is_array()) {
    //                     for (auto& btn_item : lamp_item["assBtns"]) {
    //                         uint8_t panel_id = static_cast<uint8_t>(btn_item["pId"].get<unsigned int>());
    //                         uint8_t button_id = static_cast<uint8_t>(btn_item["bId"].get<unsigned int>());
    //                         lamp->associated_buttons.push_back(AssociatedButton(panel_id, button_id));
    //                     }
    //                 }
    //                 DeviceManager::getInstance().addItem(lamp->uid, lamp);
    //             }
    //         }
    //     } else if (key == "空调通用配置") {
    //         ESP_LOGI(TAG, "解析 空调 配置");
    //         print_free_memory("空调通用配置");
    //         if (value.is_object()) {
    //             auto& airConManager = AirConManager::getInstance();
    //             airConManager.default_target_temp = value["defaultTemp"].get<uint8_t>();
    //             airConManager.default_mode = value["defaultMode"].get<ACMode>();
    //             airConManager.default_fan_speed = value["defaultFanSpeed"].get<ACFanSpeed>();
    //             airConManager.stop_threshold = value["stopThreshold"].get<uint8_t>();
    //             airConManager.rework_threshold = value["reworkThreshold"].get<uint8_t>();
    //             airConManager.stop_action = value["stopAction"].get<ACStopAction>();
    //             airConManager.remove_card_air_usable = value["removeCardAirUsable"].get<int>();
    //             airConManager.low_diff = value["lowFanTempDiff"].get<uint8_t>();
    //             airConManager.high_diff = value["highFanTempDiff"].get<uint8_t>();
    //             airConManager.auto_fun_wind_speed = value["autoVentSpeed"].get<ACFanSpeed>();
    //         }
    //     } else if (key == "空调列表") {
    //         ESP_LOGI(TAG, "解析 空调列表");
    //         print_free_memory("空调列表");
    //         if (value.is_array()) {
    //             for (auto& air_item : value) {
    //                 ACType ac_type = air_item["tp"].get<ACType>();
    //                 if (ac_type == ACType::SINGLE_PIPE_FCU) {
    //                     auto airCon = std::make_shared<SinglePipeFCU>();
    //                     airCon->uid = air_item["uid"].get<unsigned int>();
    //                     airCon->name = air_item["nm"].get<std::string>();
    //                     airCon->ac_id = air_item["id"].get<unsigned int>();
    //                     airCon->ac_type = ac_type;
    //                     airCon->low_output = BoardManager::getInstance().getBoardOutput(air_item["lowUid"].get<unsigned int>());
    //                     airCon->mid_output = BoardManager::getInstance().getBoardOutput(air_item["midUid"].get<unsigned int>());
    //                     airCon->high_output = BoardManager::getInstance().getBoardOutput(air_item["highUid"].get<unsigned int>());
    //                     airCon->water1_output = BoardManager::getInstance().getBoardOutput(air_item["water1Uid"].get<unsigned int>());
                        
    //                     DeviceManager::getInstance().addItem(airCon->uid, airCon);
    //                 } else if (ac_type == ACType::INFRARED) {
    //                     auto airCon = std::make_shared<InfraredAC>();
    //                     airCon->uid = air_item["uid"].get<unsigned int>();
    //                     airCon->name = air_item["nm"].get<std::string>();
    //                     airCon->ac_id = air_item["id"].get<unsigned int>();
    //                     airCon->ac_type = ac_type;
    //                     airCon->set_code_base(air_item["codeBase"].get<std::string>());
                        
    //                     DeviceManager::getInstance().addItem(airCon->uid, airCon);
    //                 }
    //             }
    //         }
    //     } else if (key == "窗帘列表") {
    //         ESP_LOGI(TAG, "解析 窗帘 配置");
    //         print_free_memory("窗帘");
    //         if (value.is_array()) {
    //             for (auto& cur_item : value) {
    //                 auto curtain = std::make_shared<Curtain>();
    //                 curtain->uid = cur_item["uid"].get<unsigned int>();
    //                 curtain->name = cur_item["nm"].get<std::string>();
    //                 curtain->output_open = BoardManager::getInstance().getBoardOutput(
    //                     cur_item["oOUid"].get<unsigned int>());
    //                 curtain->output_close = BoardManager::getInstance().getBoardOutput(
    //                     cur_item["oCUid"].get<unsigned int>());
    //                 curtain->run_duration = cur_item["runDur"].get<unsigned int>();

    //                 // 解析关联按钮
    //                 if (cur_item.contains("assBtns") && cur_item["assBtns"].is_array()) {
    //                     for (auto& btn_item : cur_item["assBtns"]) {
    //                         uint8_t panel_id = static_cast<uint8_t>(btn_item["pId"].get<unsigned int>());
    //                         uint8_t button_id = static_cast<uint8_t>(btn_item["bId"].get<unsigned int>());
    //                         curtain->associated_buttons.push_back(AssociatedButton(panel_id, button_id));
    //                     }
    //                 }
    //                 DeviceManager::getInstance().addItem(curtain->uid, curtain);
    //             }
    //         }
    //     } else if (key == "485指令码列表") {
    //         ESP_LOGI(TAG, "解析 RS485 配置");
    //         print_free_memory("485");
    //         if (value.is_array()) {
    //             for (auto& rs485_item : value) {
    //                 auto command = std::make_shared<RS485Command>();
    //                 command->uid = rs485_item["uid"].get<unsigned int>();
    //                 command->name = rs485_item["nm"].get<std::string>();
    //                 command->code = pavectorseHexToFixedArray(rs485_item["code"].get<std::string>());
    //                 DeviceManager::getInstance().addItem(command->uid, command);
    //             }
    //         }
    //     } else if (key == "其他设备列表") {
    //         ESP_LOGI(TAG, "解析 其他设备 配置");
    //         print_free_memory("其他设备");
    //         if (value.is_array()) {
    //             for (auto& dev_item : value) {
    //                 auto device = std::make_shared<OtherDevice>();
    //                 device->uid = dev_item["uid"].get<unsigned int>();
    //                 device->type = static_cast<OtherDeviceType>(dev_item["tp"].get<int>());
    //                 device->name = dev_item["nm"].get<std::string>();
    //                 if (device->type == OtherDeviceType::OUTPUT_CONTROL) {
    //                     device->output = BoardManager::getInstance().getBoardOutput(
    //                         dev_item["oUid"].get<unsigned int>());
    //                 }
    //                 // 影响状态
    //                 if (dev_item.contains("cauSt")) {
    //                     device->cause_state = dev_item["cauSt"].get<std::string>();
    //                 }
    //                 // 联动设备
    //                 if (dev_item.contains("linkDUids") && dev_item["linkDUids"].is_array()) {
    //                     for (auto& link_device_uid : dev_item["linkDUids"]) {
    //                         uint16_t uid = static_cast<uint16_t>(link_device_uid.get<unsigned int>());
    //                         device->link_device_uids.push_back(uid);
    //                     }
    //                 }
    //                 // 排斥设备
    //                 if (dev_item.contains("repelDUids") && dev_item["repelDUids"].is_array()) {
    //                     for (auto& repel_device_uid : dev_item["repelDUids"]) {
    //                         uint16_t uid = static_cast<uint16_t>(repel_device_uid.get<unsigned int>());
    //                         device->repel_device_uids.push_back(uid);
    //                     }
    //                 }
    //                 // 关联按钮
    //                 if (dev_item.contains("assBtns") && dev_item["assBtns"].is_array()) {
    //                     for (auto& btn_item : dev_item["assBtns"]) {
    //                         uint8_t panel_id = static_cast<uint8_t>(btn_item["pId"].get<unsigned int>());
    //                         uint8_t button_id = static_cast<uint8_t>(btn_item["bId"].get<unsigned int>());
    //                         device->associated_buttons.push_back(AssociatedButton(panel_id, button_id));
    //                     }
    //                 }
    //                 DeviceManager::getInstance().addItem(device->uid, device);
    //             }
    //         }
    //     } else if (key == "面板列表") {
    //         ESP_LOGI(TAG, "解析 面板 配置");
    //         print_free_memory("面板");
    //         if (value.is_array()) {
    //             for (auto& panel_item : value) {
    //                 auto panel = std::make_shared<Panel>();
    //                 panel->id = panel_item["id"].get<unsigned int>();
    //                 panel->name = panel_item["nm"].get<std::string>();

    //                 if (panel_item.contains("btns") && panel_item["btns"].is_array()) {
    //                     for (auto& button_item : panel_item["btns"]) {
    //                         auto button = std::make_shared<PanelButton>();
    //                         button->id = button_item["id"].get<unsigned int>();
    //                         button->name = button_item["nm"].get<std::string>();
    //                         button->host_panel = panel;
    //                         if (button_item.contains("modeName")) {
    //                             button->mode_name = button_item["modeName"].get<std::string>();
    //                         }

    //                         if (button_item.contains("actGps") && button_item["actGps"].is_array()) {
    //                             for (auto& action_group_item : button_item["actGps"]) {
    //                                 auto action_group = std::make_shared<PanelButtonActionGroup>();
    //                                 action_group->uid = action_group_item["uid"].get<int>();
    //                                 action_group->pressed_polit_actions = 
    //                                     static_cast<ButtonPolitAction>(action_group_item["pPAct"].get<int>());
    //                                 action_group->pressed_other_polit_actions = 
    //                                     static_cast<ButtonOtherPolitAction>(action_group_item["pOPAct"].get<int>());

    //                                 if (action_group_item.contains("atoActs") && action_group_item["atoActs"].is_array()) {
    //                                     for (auto& atomic_action_item : action_group_item["atoActs"]) {
    //                                         AtomicAction atomic_action;
    //                                         atomic_action.target_device = DeviceManager::getInstance().getItem(
    //                                             atomic_action_item["dUid"].get<unsigned int>());
    //                                         atomic_action.operation = atomic_action_item.value("op", "");
    //                                         atomic_action.parameter = atomic_action_item.value("pa", "");
    //                                         action_group->atomic_actions.push_back(atomic_action);
    //                                     }
    //                                 }
    //                                 ActionGroupManager::getInstance().addItem(action_group->uid, action_group);
    //                                 button->action_groups.push_back(action_group);
    //                             }
    //                         }

    //                         if (button_item.contains("rcu") && button_item["rcu"].is_number()) {
    //                             button->remove_card_usable = true; // 存在rcu字段就说明值是1了 // rcu其实是removeCardUsable的缩写
    //                         }
    //                         panel->buttons[button->id] = button;
    //                     }
    //                 }
    //                 PanelManager::getInstance().addItem(panel->id, panel);
    //             }
    //         }
    //     } else if (key == "语音指令列表") {
    //         ESP_LOGI(TAG, "解析 语音 配置");
    //         print_free_memory("语音指令");
    //         if (value.is_array()) {
    //             for (auto& voice_item : value) {
    //                 auto voice_command = std::make_shared<VoiceCommand>();
    //                 voice_command->name = voice_item["nm"].get<std::string>();
    //                 voice_command->code = pavectorseHexToFixedArray(voice_item["code"].get<std::string>());
                    
    //                 if (voice_item.contains("actGps") && voice_item["actGps"].is_array()) {
    //                     for (auto& action_group_item : voice_item["actGps"]) {
    //                         auto action_group = std::make_shared<VoiceCommandActionGroup>();
    //                         action_group->uid = action_group_item["uid"].get<unsigned int>();

    //                         if (action_group_item.contains("atoActs") && action_group_item["atoActs"].is_array()) {
    //                             for (auto& atomic_action_item : action_group_item["atoActs"]) {
    //                                 AtomicAction atomic_action;
    //                                 atomic_action.target_device = DeviceManager::getInstance().getItem(
    //                                     atomic_action_item["dUid"].get<unsigned int>());
    //                                 atomic_action.operation = atomic_action_item.value("op", "");
    //                                 atomic_action.parameter = atomic_action_item.value("pa", "");

    //                                 action_group->atomic_actions.push_back(atomic_action);
    //                             }
    //                         }
    //                         // 不把这些动作组添加进ActionGroupManager
    //                         voice_command->action_groups.push_back(action_group);
    //                     }
    //                 }
    //                 VoiceCommandManager::getInstance().add_voice_command(voice_command);
    //             }
    //         }
    //     }
    // } // for end

    // if (!board_config.empty()) {
    //     ESP_LOGI(TAG, "解析 板子输入");
    //     print_free_memory("板子输入");
        
    //     json obj = json::parse(board_config);

    //     auto it = obj.begin();
    //     const std::string& key = it.key();
    //     const json& value = it.value();

    //     if (value.is_array()) {
    //         for (auto& board_item : value) {
    //             auto board = BoardManager::getInstance().getItem(board_item["id"].get<unsigned int>());

    //             ESP_LOGI(TAG, "解析 板 %d 输入列表", board->id);
    //             if (board_item.contains("is") && board_item["is"].is_array()) {
    //                 for (auto& input_item : board_item["is"]) {
    //                     BoardInput input;
    //                     input.host_board_id = input_item["hBId"].get<unsigned int>();
    //                     input.channel = input_item["ch"].get<unsigned int>();
    //                     input.type = static_cast<InputType>(input_item["iTp"].get<int>());
    //                     if (input_item.contains("modeName")) {
    //                         input.mode_name = input_item["modeName"].get<std::string>();
    //                     }
    //                     if (input.type == InputType::INFRARED) {
    //                         input.infrared_duration = input_item["infDu"].get<uint16_t>();
    //                     }
    
    //                     if (input_item.contains("actGps") && input_item["actGps"].is_array()) {
    //                         for (auto& action_group_item : input_item["actGps"]) {
    //                             auto action_group = std::make_shared<InputActionGroup>();
    //                             action_group->uid = action_group_item["uid"].get<unsigned int>();
    
    //                             if (action_group_item.contains("atoActs") && action_group_item["atoActs"].is_array()) {
    //                                 for (auto& atomic_action_item : action_group_item["atoActs"]) {
    //                                     AtomicAction atomic_action;
    //                                     atomic_action.target_device = DeviceManager::getInstance().getItem(
    //                                         atomic_action_item["dUid"].get<unsigned int>());
    //                                     atomic_action.operation = atomic_action_item.value("op", "");
    //                                     atomic_action.parameter = atomic_action_item.value("pa", "");
    
    //                                     action_group->atomic_actions.push_back(atomic_action);
    //                                 }
    //                             }
    //                             ActionGroupManager::getInstance().addItem(action_group->uid, action_group);
    //                             input.action_groups.push_back(action_group);
    //                         }
    //                     }
    //                     board->inputs.push_back(input);
    //                 }
    //             }
    //             // 启动所有红外输入的定时器
    //             for (auto& input : board->inputs) {
    //                 if (input.type == InputType::INFRARED) {
    //                     input.init_infrared_timer();
    //                 }
    //             }
    //         }
    //     }
    // }

    // ESP_LOGI(TAG, "解析所有设备的关联按钮");
    // // 窗帘
    // auto curtains = DeviceManager::getInstance().getDevicesOfType<Curtain>();
    // for (auto& curtain : curtains) {
    //     for (auto& associated_button : curtain->associated_buttons) {
    //         auto panel_instance = PanelManager::getInstance().getItem(associated_button.panel_id);
    //         auto panel_button = panel_instance->buttons[associated_button.button_id];
    //         for (auto& action_group : panel_button->action_groups) {
    //             for (auto& atomic_action : action_group->atomic_actions) {
    //                 if (atomic_action.operation == "打开") {
    //                     curtain->open_buttons.push_back(panel_button);
    //                 } else if (atomic_action.operation == "关闭") {
    //                     curtain->close_buttons.push_back(panel_button);
    //                 } else if (atomic_action.operation == "反转") {
    //                     curtain->reverse_buttons.push_back(panel_button);
    //                 }
    //             }
    //         }
    //     }
    // }

    // ESP_LOGI(TAG, "所有配置解析完成");

    // // 查询"存活"通道是否存活
    // uart_frame_t frame;
    // build_frame(0x08, 0, LordManager::getInstance().getAliveChannel(), 0x00, 0x00, &frame);
    // send_frame(&frame);
}

json generateRegisterInfo() {
    json j;
    try {
        j["mac"] = getSerialNum();
        j["type"] = "regis";

        auto& config_version = LordManager::instance().getConfigGenerageTime();
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
        for(auto* lamp : LordManager::instance().getDevicesByType<Lamp>()) {
            json lamp_obj;
            lamp_obj["id"] = std::to_string(lamp->getDid());
            lamp_obj["name"] = lamp->getName();
            lamp_obj["state"] = lamp->isOn() ? "1" : "0";
            j["lights"].push_back(lamp_obj);
        }

        j["curtains"] = json::array();
        for (auto* curtain : LordManager::instance().getDevicesByType<Curtain>()) {
            json cur_obj;
            cur_obj["id"] = std::to_string(curtain->getDid());
            cur_obj["name"] = curtain->getName();
            cur_obj["state"] = curtain->getState() ? "1" : "0";
            j["curtains"].push_back(cur_obj);
        }
        
        j["others"] = json::array();
        // auto others = DeviceManager::getInstance().getDevicesOfType<OtherDevice>();
        // for (auto& other : others) {
        //     if (other->type == OtherDeviceType::OUTPUT_CONTROL) {
        //         nlohmann::json other_obj;
        //         other_obj["id"] = std::to_string(other->uid);
        //         other_obj["name"] = other->name;
        //         other_obj["output_type"] = std::to_string(static_cast<int>(other->output->type));
        //         other_obj["output_num"] = std::to_string(other->output->channel);
        //         other_obj["state"] = other->isOn() ? "1" : "0";
        //         j["others"].push_back(other_obj);
        //     }
        // }

        j["airs"] = json::array();
        for (auto* air : LordManager::instance().getDevicesByType<AirConBase>()) {
            json air_obj;
            air_obj["id"] = std::to_string(air->getDid());
            air_obj["air_id"] = std::to_string(air->getAcId());
            air_obj["name"] = air->getName();
            air_obj["state"] = air->get_state() ? "1" : "0";
            air_obj["mode"] = std::to_string(static_cast<int>(air->get_mode()));
            air_obj["fan_speed"] = std::to_string(static_cast<int>(air->get_fan_speed()));
            air_obj["target_temp"] = std::to_string(air->get_target_temp());
            j["airs"].push_back(air_obj);
        }

        j["modes"] = json::array();
        for (auto* mode : LordManager::instance().getAllModeActionGroup()) {
            j["modes"].push_back(mode->getName());
        }
    } catch(const std::exception& e) {
        ESP_LOGW(TAG, "生成注册信息时出错: %s", e.what());
    }

    return j;
}

json generateReportStates() {
    json j;
    try {

        j["mac"] = getSerialNum();
        j["type"] = "alldevicestate";

        j["lights"] = json::array();
        for (Lamp* lamp : LordManager::instance().getDevicesByType<Lamp>()) {
            json lamp_obj;
            lamp_obj["id"] = lamp->getDid();
            lamp_obj["state"] = lamp->isOn() ? 1 : 0;
            j["lights"].push_back(lamp_obj);
        }

        j["curtains"] = json::array();
        for (Curtain* cur : LordManager::instance().getDevicesByType<Curtain>()) {
            json cur_obj;
            cur_obj["id"] = cur->getDid();
            cur_obj["state"] = cur->getState() ? 1 : 0;
            j["curtains"].push_back(cur_obj);
        }

        // auto others = DeviceManager::getInstance().getDevicesOfType<OtherDevice>();
        // size_t othersCount = 0;
        // for (auto& other : others) {
        //     if (other->type == OtherDeviceType::OUTPUT_CONTROL) {
        //         ++othersCount;
        //     }
        // }
        j["others"] = json::array();
        // j["others"].get_ref<nlohmann::json::array_t&>().reserve(othersCount);
        // for (auto& other : others) {
        //     if (other->type == OtherDeviceType::OUTPUT_CONTROL) {
        //         nlohmann::json other_obj;
        //         other_obj["id"] = other->uid;
        //         other_obj["state"] = other->isOn() ? 1 : 0;
        //         j["others"].push_back(other_obj);
        //     }
        // }

        j["airs"] = json::array();
        for (AirConBase* air : LordManager::instance().getDevicesByType<AirConBase>()) {
            json air_obj;
            air_obj["id"] = air->getDid();
            air_obj["state"] = air->get_state() ? 1 : 0;
            air_obj["mode"] = static_cast<int>(air->get_mode());
            air_obj["fan_speed"] = static_cast<int>(air->get_fan_speed());
            air_obj["target_temp"] = air->get_target_temp();
            air_obj["room_temp"] = air->get_room_temp();
            j["airs"].push_back(air_obj);
        }

        j["mode"] = LordManager::instance().getLastModeName();
        j["states"] = getRoomStates();
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "生成状态报告时出错: %s", e.what());
    }

    return j;
}