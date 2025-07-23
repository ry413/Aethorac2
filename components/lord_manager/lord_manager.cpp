#include "lord_manager.h"
#include <esp_log.h>
#include "action_group.h"
#include "iinput.h"
#include "channel_input.h"
#include "lamp.h"
#include "curtain.h"
#include "air_conditioner.h"

#define TAG "LORD_MANAGER"

void LordManager::clearAll() {
    useSleepHeartBeat();
    config_generate_time.clear();
    last_mode_name.clear();
    devices_map.clear();
    action_groups_map.clear();
    channel_inputs.clear();
    panels_map.clear();
}

void LordManager::updateChannelState(uint8_t board_id, uint8_t channel, uint8_t is_on) {}

void LordManager::executeInputAction(uint8_t board_id, uint8_t channel, uint8_t is_on) {
    
}

// void wakeup_heartbeat() {
//     ESP_LOGI(TAG, "切换至插卡心跳");
//     heartbeat_code = pavectorseHexToFixedArray("7FC0FFFF0080BD7E");
// }

// void sleep_heartbeat() {
//     ESP_LOGI(TAG, "切换至睡眠心跳");
//     heartbeat_code = pavectorseHexToFixedArray("7FC0FFFF00003D7E");
// }

// bool is_sleep() {
//     return heartbeat_code == pavectorseHexToFixedArray("7FC0FFFF00003D7E");
// }
void LordManager::registerPreset(uint16_t did, const std::string& name, const std::string&carry_state, DeviceType type) {

}

void LordManager::registerLamp(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel) {
    auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
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
    // auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerRelayOut(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel) {
    // auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerDryContactOut(uint16_t did, const std::string& name, const std::string& carry_state, uint8_t channel) {
    // auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

IDevice* LordManager::getDeviceByDid(uint16_t did) const {
    auto it = devices_map.find(did);
    return it != devices_map.end() ? it->second.get() : nullptr;
}

std::vector<ActionGroup*> LordManager::getAllModeActionGroup() const {
    std::vector<ActionGroup*> result;
    for (const auto& [aid, ag_ptr] : action_groups_map) {
        if (ag_ptr && ag_ptr->is_mode()) {
            result.push_back(ag_ptr.get());
        }
    }
    return result;
}

Panel* LordManager::getPanelByPid(uint8_t pid) const {
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

void LordManager::updateButtonIndicator(PanelButtonPair assoc, bool state) {
    if (auto panel = getPanelByPid(assoc.panel_id); panel) {
        panel->updateButtonIndicator(assoc.button_id, state);
    } else {
        ESP_LOGW(TAG, "id为%d的面板不存在", assoc.panel_id);
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
    channel_inputs[input->getIid()] = std::move(input);
}
