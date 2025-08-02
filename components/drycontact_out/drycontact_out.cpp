#include "drycontact_out.h"
#include "stm32_tx.h"
#include "lord_manager.h"
#include "commons.h"

#define TAG "DRYCONTACT_OUT"

void DryContactOut::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    ESP_LOGI_CYAN(TAG, "干接点输出[%s]收到操作[%s]", name.c_str(), operation.c_str());
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

void DryContactOut::open_self(bool should_log) {
    controlDrycontactOut(channel, true);
    current_state = State::ON;
    updateButtonIndicator(true);
    change_state(true);
    sync_link_devices("开");
    close_repel_devices();
}

void DryContactOut::close_self(bool should_log) {
    controlDrycontactOut(channel, false);
    current_state = State::OFF;
    updateButtonIndicator(false);
    change_state(false);
    sync_link_devices("关");
}

void DryContactOut::updateButtonIndicator(bool state) {
    for (const auto [pid, bid] : associated_buttons) {
        if (Panel* panel = LordManager::instance().getPanelByPid(pid)) {
            panel->wishIndicatorByButton(bid, state);
        }
    }
}
