#include "voice_command.h"
#include <string>
#include "esp_log.h"

#include "action_group.h"
#include "rs485_comm.h"
#include "lamp.h"
#include "lord_manager.h"
#include <drycontact_out.h>

#define TAG "VOICE_CMD"

void VoiceCommand::execute() {
    ESP_LOGI_CYAN(TAG, "语音指令[%s]开始执行动作组(%u/%u)", name.c_str(), current_index + 1, action_groups.size());
    static auto& lord = LordManager::instance();

    if (lord.isSleep()) {
        lord.useAliveHeartBeat();
        for (auto* dry : lord.getDevicesByType<DryContactOut>()) {
            dry->syncAssBtnToDevState();
        }
    }

    // 语音输入应该不需要任意键执行

    if (current_index < action_groups.size()) {
        action_groups[current_index]->executeAllAtomicAction();

        current_index = (current_index + 1) % action_groups.size();
    }
}
