#include "lord_manager.h"
#include <esp_log.h>
#include "lamp.h"
#include "action_group.h"
#include "iinput.h"
#include "channel_input.h"

#define TAG "LORD_MANAGER"

void LordManager::updateChannelState(uint8_t board_id, uint8_t channel, uint8_t is_on) {
    
}


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
void LordManager::registerPreset(uint8_t did, const std::string& name, const std::string&carry_state, DeviceType type) {

}

void LordManager::registerLamp(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t channel) {
    auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerCurtain(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t open_ch, uint8_t close_ch, uint32_t runtime) {
    // auto dev = std::make_unique<Lamp>(did, name, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerIngraredAir(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t airId) {
    // auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerSingleAir(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t airId, uint8_t wc, uint8_t lc, uint8_t mc, uint8_t hc) {
    // auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerRs485(uint8_t did, const std::string& name, const std::string& carry_state, const std::string& code) {
    // auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerRelayOut(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t channel) {
    // auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

void LordManager::registerDryContactOut(uint8_t did, const std::string& name, const std::string& carry_state, uint8_t channel) {
    // auto dev = std::make_unique<Lamp>(did, name, carry_state, channel);
    // devices_map[dev->getDid()] = std::move(dev);
}

IDevice* LordManager::getDeviceByDid(uint8_t did) {
    auto it = devices_map.find(did);
    return it != devices_map.end() ? it->second.get() : nullptr;
}

void LordManager::registerActionGroup(uint8_t aid, const std::string& name, bool is_mode, std::vector<AtomicAction> actions) {
    auto actionGroup = std::make_unique<ActionGroup>(aid, name, is_mode, actions);
    action_groups_map[actionGroup->getAid()] = std::move(actionGroup);
}

void LordManager::registerPanelKeyInput(uint8_t iid, const std::string& name, InputTag tag, uint8_t pid, uint8_t bid, std::vector<std::unique_ptr<ActionGroup>>&& action_groups) {
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

void LordManager::registerDryContactInput(uint8_t iid, InputType type, const std::string& name, InputTag tag, uint8_t channel, TriggerType trigger_type, uint64_t duration, std::vector<std::unique_ptr<ActionGroup>>&& action_groups) {
    auto input = std::make_unique<ChannelInput>(iid, type, name, tag, channel, trigger_type, duration, std::move(action_groups));
    channel_inputs[input->getIid()] = std::move(input);
}
