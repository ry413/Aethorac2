#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <charconv>
#include "preset_device.h"
#include "lord_manager.h"
#include "commons.h"
#include "rs485_comm.h"
#include "indicator.h"
#include "room_state.h"
#include "relay_out.h"
#include "drycontact_out.h"
#include "my_mqtt.h"

#define TAG "PRESET_DEVICE"
std::unordered_map<uint16_t, bool> all_device_onoff_snapshot = {};  // <did, isOn> 房间设备快照

void PresetDevice::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    ESP_LOGI_CYAN(TAG, "[%s]收到操作[%s], par[%s], aid[%d]", name.c_str(), operation.c_str(), parameter.c_str(), self_action_group ? self_action_group->getAid() : -1);
    static auto& lord = LordManager::instance();
    switch (type) {
        case DeviceType::HEARTBEAT:
            // 如果收到睡眠操作, 改变心跳包
            if (operation == "睡眠") {
                // 注册所有指示灯的函数, 等当前动作组结束后会发布
                lord.wishIndicatorAllPanel(false);
                // 切换为睡眠心跳包
                lord.useSleepHeartBeat();
            } else if (operation == "拔卡") {
                lord.useSleepHeartBeat();
                lord.setAlive(false);
            } else if (operation == "插卡") {
                // 进入插卡状态
                lord.useAliveHeartBeat();
                lord.setAlive(true);
                // 插卡时固定广播一下时间, 这应该没什么问题
                time_t now = get_current_timestamp();
                if (now != 0) {
                    uint8_t p2, p3, p4, p5;
                    p2 = (now >> 24) & 0xFF;
                    p3 = (now >> 16) & 0xFF;
                    p4 = (now >> 8)  & 0xFF;
                    p5 = now & 0xFF;
                    
                    generate_response(ALL_TIME_SYNC, p2, p3, p4, p5);
                }
            }
            break;
        
        case DeviceType::ROOM_STATE:
            if (operation == "添加") {
                add_state(parameter);
                // 如果是SOS, 立即上报状态
                if (parameter == "SOS") {
                    report_states();;
                }
            } else if (operation == "删除") {
                remove_state(parameter);
            } else if (operation == "反转") {
                toggle_state(parameter);
            } else if (operation == "如果存在此状态则跳出") {
                if (exist_state(parameter)) {
                    if (self_action_group) {
                        self_action_group->suicide();
                    } else {
                        ESP_LOGE(TAG, "不存在的self_action_group");
                    }
                }
            }
            break;
        case DeviceType::DELAYER:
            if (operation == "延时") {
                int delay = 0;
                auto result = std::from_chars(parameter.data(), parameter.data() + parameter.size(), delay);
                if (result.ec != std::errc() || delay < 0) {
                    ESP_LOGE(TAG, "延时参数非法: %s", parameter.c_str());
                    break;
                }

                ESP_LOGI(TAG, "延时%d秒\n", delay);
                // 在进入延时前先发布已注册的指示灯函数
                IndicatorHolder::getInstance().callAllAndClear();
                if (self_action_group) {
                    if (!self_action_group->delay_ms(delay * 1000)) {
                        ESP_LOGW(TAG, "延时被取消，中止后续动作");
                        break;
                    }
                } else {
                    vTaskDelay(delay * 1000 / portTICK_PERIOD_MS);
                }
            }
            break;
        case DeviceType::ACTION_GROUP_OP: {
            int id = 0;
            auto result = std::from_chars(parameter.data(), parameter.data() + parameter.size(), id);
            if (result.ec != std::errc() || id < 0) {
                ESP_LOGE(TAG, "非法的动作组ID: %s", parameter.c_str());
                break;
            }

            if (operation == "调用") {
                if (auto* ag = lord.getActionGroupByAid(id)) {
                    ESP_LOGI(TAG, "调用[%s](%u)", ag->getName().c_str(), ag->getAid());
                    ag->executeAllAtomicAction();
                }
            } else if (operation == "中断") {
                if (auto* ag = lord.getActionGroupByAid(id)) {
                    ESP_LOGI(TAG, "中断[%s](%u)", ag->getName().c_str(), ag->getAid());
                    ag->suicide();
                }
            } else if (operation == "生成任意键执行") {
                lord.setAnyKeyActionGroup(id);
            } else if (operation == "删除任意键执行") {
                lord.clearAnyKeyActionGroup();
            }
            break;
        }
        case DeviceType::SNAPSHOT: {
            if (operation == "记录快照") {
                for(auto* dev : lord.getDevicesByType<IDevice>()) {
                    // 跳过预设设备
                    if (dynamic_cast<PresetDevice*>(dev) != nullptr) {
                        continue;
                    }
                    all_device_onoff_snapshot[dev->getDid()] = dev->isOn();
                }
            } else if (operation == "读取并删除快照") {
                for (auto [did, is_on] : all_device_onoff_snapshot) {
                    if (IDevice* dev = lord.getDeviceByDid(did)) {
                        auto type = dev->getType();
                        switch (type) {
                            case DeviceType::LAMP:
                            case DeviceType::CURTAIN:
                            case DeviceType::INFRARED_AIR:
                            case DeviceType::SINGLE_AIR:
                            case DeviceType::DRY_CONTACT:
                                dev->execute(is_on ? "开" : "关", "");
                                break;
                            case DeviceType::RELAY:
                                if (auto* relay = dynamic_cast<SingleRelayDevice*>(dev)) {
                                    if (relay->getType() != DeviceType::DOORBELL) {
                                        relay->execute(is_on ? "开" : "关", "");
                                    }
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
                all_device_onoff_snapshot.clear();
            } else if (operation == "删除快照") {
                all_device_onoff_snapshot.clear();
            } else if (operation == "清除快照并跳出") {
                all_device_onoff_snapshot.clear();
                if (self_action_group) {
                    self_action_group->suicide();
                } else {
                    ESP_LOGE(TAG, "不存在的self_action_group");
                }
            }
            break;
        }
        case DeviceType::INDICATOR: {
            int pid, bid;
            if (sscanf(parameter.c_str(), "%d,%d", &pid, &bid) != 2) {
                ESP_LOGE(TAG, "Indicator错误, 无法获取按键: %s", parameter.c_str());
                break;
            }
            if (Panel* panel = lord.getPanelByPid(pid)) {
                if (operation == "亮") {
                    panel->wishIndicatorByButton(bid, true);
                } else if (operation == "灭") {
                    panel->wishIndicatorByButton(bid, false);
                } else if (operation == "亮1秒") {
                    panel->shortLightIndicator(bid);
                }
            } else {
                ESP_LOGE(TAG, "Indicator错误, 无法获取面板(pid: %d", pid);
            }
        }
        default:
            break;
    }
}