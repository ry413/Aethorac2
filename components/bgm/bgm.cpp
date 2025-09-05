#include "bgm.h"
#include "stm32_tx.h"
#include "commons.h"
#include "rs485_comm.h"

#define TAG "BGM"

void BGM::execute(std::string operation, std::string parameter, ActionGroup* self_action_group, bool should_log) {
    ESP_LOGI_CYAN(TAG, "单继电器设备[%s]收到操作[%s]", name.c_str(), operation.c_str());
    if (operation == "打开功放") {
        generate_response(0x80, 0x01, 0x00, 0x26, 0x01);
    } else if (operation == "关闭功放") {
        generate_response(0x80, 0x01, 0x00, 0x26, 0x00);
    } else if (operation == "播放") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_PLAY);
    } else if (operation == "停止") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_STOP);
    } else if (operation == "播放/暂停") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_PLAY_AND_PAUSE);
    } else if (operation == "上一首") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_PREV);
    } else if (operation == "下一首") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_NEXT);
    } else if (operation == "音量加") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_VOLUME_UP);
    } else if (operation == "音量减") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_VOLUME_DOWN);
    } else if (operation == "打开蓝牙模式") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_BL_OPEN);
    } else if (operation == "关闭蓝牙模式") {
        generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_BL_CLOSE);
    } else if (operation == "反转模式") {
        if (curr_mode == BGMMode::BL) {
            generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_BL_CLOSE);
        } else {
            generate_response(BGM_CON, 0x00, 0x00, 0x00, BGM_CON_BL_OPEN);
        }
    }
}

void BGM::syncAssBtnToDevState() {
    updateButtonIndicator(curr_mode == BGMMode::BL);
}

void BGM::changeMode(BGMMode mode) {
    curr_mode = mode;
    syncAssBtnToDevState();
}

void BGM::updateButtonIndicator(bool state) {
    for (const auto [pid, bid] : associated_buttons) {
        if (Panel* panel = LordManager::instance().getPanelByPid(pid)) {
            panel->wishIndicatorByButton(bid, state);
        }
    }
}
