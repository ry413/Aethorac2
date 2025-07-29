#include <esp_log.h>
#include "idevice.h"
#include "room_state.h"
#include "lord_manager.h"
#include "stm32_tx.h"

#define TAG "IDEVICE"

void IDevice::change_state(bool state) {
    if (carry_state.empty()) {
        return;
    }
    if (state) {
        add_state(carry_state);
    } else {
        remove_state(carry_state);
    }
}

void IDevice::sync_link_devices(std::string operation, bool should_log) {
    static auto& lord = LordManager::instance();

    // 如果should_log是false, 说明此时正在执行某种情景模式, 那就不操控联动设备
    if (!should_log) {
        return;
    }

    operated_flag = true;  // 标记为已被动过
    for (auto link_did : link_dids) {
        if (IDevice* dev = lord.getDeviceByDid(link_did)) {
            if (!dev->isOperated()) {
                dev->execute(operation, "", nullptr, should_log);
            }
        }
    }
    operated_flag = false;  // 在这里重置标记
}

void IDevice::close_repel_devices(void) {
    static auto& lord = LordManager::instance();

    for (auto repel_did : repel_dids) {
        if (IDevice* dev = lord.getDeviceByDid(repel_did)) {
            dev->execute("关", "");
        }
    }
}