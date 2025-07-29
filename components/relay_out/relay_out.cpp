#include "relay_out.h"
#include "stm32_tx.h"
#include "commons.h"

#define TAG "RELAY_OUT"

void SingleRelayDevice::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    ESP_LOGI_CYAN(TAG, "单继电器设备[%s]收到操作[%s]", name.c_str(), operation.c_str());
    if (operation == "开" || operation == "打开") {
        open_self(should_log);
    } else if (operation == "关" || operation == "关闭") {
        close_self(should_log);
    } else if (operation == "反转") {
        if (isOn()) {
            close_self(should_log);
        } else {
            open_self(should_log);
        }
    }
}

void SingleRelayDevice::open_self(bool should_log) {
    controlRelay(channel, true);
    updateButtonIndicator(true);
    change_state(true);
    sync_link_devices("开");
    close_repel_devices();
}

void SingleRelayDevice::close_self(bool should_log) {
    controlRelay(channel, false);
    updateButtonIndicator(false);
    change_state(false);
    sync_link_devices("关");
}

void SingleRelayDevice::syncAssBtnToDevState() {
    updateButtonIndicator(isOn());
}

bool SingleRelayDevice::isOn() const { return LordManager::instance().readRelayPhysicsState(channel); }

void SingleRelayDevice::updateButtonIndicator(bool state) {
    for (const auto [pid, bid] : associated_buttons) {
        if (Panel* panel = LordManager::instance().getPanelByPid(pid)) {
            panel->wishIndicatorByButton(bid, state);
        }
    }
}
