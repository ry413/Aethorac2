#include "lord_manager.h"
#include <esp_log.h>
#include "stm32_tx.h"
#include "action_group.h"
#include "iinput.h"
#include "channel_input.h"

#include "preset_device.h"
#include "lamp.h"
#include "curtain.h"
#include "air_conditioner.h"
#include "rs485_command.h"
#include "relay_out.h"
#include "drycontact_out.h"

#define TAG "LORD_MANAGER"

void LordManager::registerPreset(uint16_t did, const std::string& name, const std::string& carry_state,
                                 DeviceType type) {
    auto dev = std::make_unique<PresetDevice>(did, name, carry_state, type);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerLamp(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel, const std::vector<uint16_t> link_dids, const std::vector<uint16_t> repel_dids) {
    auto dev = std::make_unique<Lamp>(did, name, carry_state, channel, readRelayPhysicsState(channel));
    dev->addLinkDidsAndRepelDids(link_dids, repel_dids);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerCurtain(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t open_ch, uint8_t close_ch, uint64_t runtime) {
    auto dev = std::make_unique<Curtain>(did, name, carry_state, open_ch, close_ch, runtime);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerIngraredAir(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t airId) {
    auto dev = std::make_unique<InfraredAC>(did, name, carry_state, airId);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerSingleAir(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t airId, uint8_t wc, uint8_t lc, uint8_t mc, uint8_t hc) {
    auto dev = std::make_unique<SinglePipeFCU>(did, name, carry_state, airId, wc, lc, mc, hc);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerRs485(uint16_t did, const std::string& name, const std::string& carry_state, const std::string& code) {
    auto dev = std::make_unique<RS485Command>(did, name, carry_state, code);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerRelayOut(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel, const std::vector<uint16_t> link_dids, const std::vector<uint16_t> repel_dids) {
    auto dev = std::make_unique<SingleRelayDevice>(did, DeviceType::RELAY, name, carry_state, channel, readRelayPhysicsState(channel));
    dev->addLinkDidsAndRepelDids(link_dids, repel_dids);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerDryContactOut(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel, const std::vector<uint16_t> link_dids, const std::vector<uint16_t> repel_dids) {
    auto dev = std::make_unique<DryContactOut>(did, name, carry_state, channel);
    dev->addLinkDidsAndRepelDids(link_dids, repel_dids);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerDoorbell(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel) {
    auto dev = std::make_unique<SingleRelayDevice>(did, DeviceType::DOORBELL, name, carry_state, channel, readRelayPhysicsState(channel));
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerActionGroup(uint16_t aid, const std::string& name, bool is_mode, std::vector<AtomicAction> actions) {
    auto actionGroup = std::make_unique<ActionGroup>(aid, name, is_mode, actions);
    action_groups_map[actionGroup->getAid()] = std::move(actionGroup);
}

void LordManager::registerPanelKeyInput(uint16_t iid, const std::string& name, InputTag tag, uint8_t pid, uint8_t bid, std::vector<std::unique_ptr<ActionGroup>>&& action_groups) {
    // 查找是否已存在面板实例
    auto it = panels_map.find(pid);
    Panel* panel = (it != panels_map.end()) ? it->second.get() : nullptr;

    // 如果不存在就创建面板实例
    if (!panel) {
        auto fresh = std::make_unique<Panel>(pid);
        panel = fresh.get();
        panels_map.emplace(pid, std::move(fresh));
    }

    // 把button塞给它
    if (!panel->addButton(iid, name, bid, tag, std::move(action_groups))) {
        ESP_LOGW(TAG, "Panel %u 已经有 Button %u, 忽略", pid, bid);
        // 失败的话action_groups就已经丢失了
    }
}

void LordManager::registerDryContactInput(uint16_t iid, InputType type, const std::string& name, InputTag tag, uint8_t channel, TriggerType trigger_type, uint64_t duration, std::vector<std::unique_ptr<ActionGroup>>&& action_groups) {
    auto input = std::make_unique<ChannelInput>(iid, type, name, tag, channel, trigger_type, duration, std::move(action_groups));
    if (trigger_type == TriggerType::INFRARED) {
        input->init_infrared_timer();
    } else {
        // 注册时看看插拔卡输入通道的物理状态
        if (tag == InputTag::IS_ALIVE_CHANNEL) {
            if (readDrycontactInputPhysicsState(channel)) {
                setAlive(true);
                useAliveHeartBeat();
            } else {

            }
        }
    }
    channel_inputs_map[input->getIid()] = std::move(input);
}

IDevice* LordManager::getDeviceByDid(uint16_t did) {
    auto it = devices_map.find(did);
    return it != devices_map.end() ? it->second.get() : nullptr;
}

ActionGroup* LordManager::getActionGroupByAid(uint16_t aid) {
    auto it = action_groups_map.find(aid);
    return it != action_groups_map.end() ? it->second.get() : nullptr;
}

std::vector<ActionGroup*> LordManager::getAllModeActionGroup() {
    std::vector<ActionGroup*> result;
    for (const auto& [aid, ag_ptr] : action_groups_map) {
        if (ag_ptr && ag_ptr->is_mode()) {
            result.push_back(ag_ptr.get());
        }
    }
    return result;
}

std::vector<ChannelInput*> LordManager::getAllChannelInputByChannelNum(uint8_t channel_num) {
    std::vector<ChannelInput*> result;
    for (auto& [_, input_ptr] : channel_inputs_map) {
        if (input_ptr && input_ptr->channel == channel_num) {
            result.push_back(input_ptr.get());
        }
    }
    return result;
}

ChannelInput* LordManager::getAliveChannel() {    
    for (auto& [iid, input] : channel_inputs_map) {
        if (input->getTag() == InputTag::IS_ALIVE_CHANNEL) {
            return input.get();
        }
    }
    ESP_LOGE(TAG, "不存在拥有插拔卡标记的通道");
    return nullptr;
}

Panel* LordManager::getPanelByPid(uint8_t pid) {
    auto it = panels_map.find(pid);
    return it != panels_map.end() ? it->second.get() : nullptr;
}

void LordManager::handlePanel(uint8_t panel_id, uint8_t target_buttons, uint8_t old_bl_state) {
    if (auto panel = getPanelByPid(panel_id); panel) {
        panel->switchReport(target_buttons, old_bl_state);
    } else {
        ESP_LOGW(TAG, "id为%d的面板不存在", panel_id);
    }
}

void LordManager::wishIndicatorAllPanel(bool state) {
    for (const auto& [pid, panel_ptr] : panels_map) {
        if (panel_ptr) {
            panel_ptr->wishIndicatorByPanel(state);
        }
    }
}

void LordManager::updateAirState(uint8_t states, uint8_t temps) {
    uint8_t air_id = states & 0x07;

    for (auto* air : getDevicesByType<AirConBase>()) {
        if (air->getAcId() == air_id) {
            air->update_state(states, temps);
        }
    }
}

void LordManager::updateRoomTemp(uint8_t air_id, uint8_t room_temp) {
    for (auto* air : getDevicesByType<AirConBase>()) {
        if (air->getAcId() == air_id) {
            air->update_room_temp(room_temp);
        }
    }
}

void LordManager::clearAll() {
    useSleepHeartBeat();
    config_generate_time.clear();
    last_mode_name.clear();
    devices_map.clear();
    action_groups_map.clear();
    channel_inputs_map.clear();
    panels_map.clear();
}

bool LordManager::execute_any_key_action_group() {
    if (any_key_execute_action_group_id > -1) {
        auto action_group = getActionGroupByAid(any_key_execute_action_group_id);
        any_key_execute_action_group_id = -1;       // 必须清除这个再调用executeAllAtomicAction
        action_group->executeAllAtomicAction();     // 其实不如说, 这三行是最优解, 别无他法
        return true;
    } else {
        return false;
    }
}

void LordManager::syncAllRelayPhysicsOnoff() {
    for (int i = 1; i <= 42; i++) {
        sendStm32Cmd(CMD_RELAY_QUERY, 0x00, i, 0x00, 0x00);
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

void LordManager::updateRelayPhysicsState(uint8_t channel, uint8_t is_on) {
    xSemaphoreTake(relay_physics_map_mutex, pdMS_TO_TICKS(3000));
    relay_physics_map[channel] = is_on;
    xSemaphoreGive(relay_physics_map_mutex);
}

bool LordManager::readRelayPhysicsState(uint8_t channel) {
    xSemaphoreTake(relay_physics_map_mutex, pdMS_TO_TICKS(3000));
    bool result = false;
    auto it = relay_physics_map.find(channel);
    if (it != relay_physics_map.end()) {
        result = it->second;
    } else {
        ESP_LOGW(TAG, "试图读取未定义继电器[%u]状态", channel);
    }
    xSemaphoreGive(relay_physics_map_mutex);
    return result;
}

void LordManager::syncAllDrycontactInputPhysicsOnoff() {
    for (int i = 1; i <= 16; i++) {
        sendStm32Cmd(CMD_DRYCONTACT_INPUT_QUERY, 0x00, i, 0x00, 0x00);
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

void LordManager::updateDrycontactInputPhysicsState(uint8_t channel, uint8_t is_on) {
    xSemaphoreTake(drycontactInput_physics_map_mutex, pdMS_TO_TICKS(3000));
    drycontactInput_physics_map[channel] = is_on;
    xSemaphoreGive(drycontactInput_physics_map_mutex);
}

bool LordManager::readDrycontactInputPhysicsState(uint8_t channel) {
    xSemaphoreTake(drycontactInput_physics_map_mutex, pdMS_TO_TICKS(3000));
    bool result = false;
    auto it = drycontactInput_physics_map.find(channel);
    if (it != drycontactInput_physics_map.end()) {
        result = it->second;
    } else {
        ESP_LOGW(TAG, "试图读取未定义干接点输出[%u]状态", channel);
    }
    xSemaphoreGive(drycontactInput_physics_map_mutex);
    return result;
}

void LordManager::onDoorOpened() {
    door_open = true;
    last_door_open_time = esp_timer_get_time() / 1000;
    ESP_LOGI(__func__, "已开门");
}

void LordManager::onDoorClosed() {
    door_open = false;
    last_door_close_time = esp_timer_get_time() / 1000;
    ESP_LOGI(__func__, "已关门");
}

