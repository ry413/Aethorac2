#include "voice_command.h"
#include <string>
#include "esp_log.h"

#include "action_group.h"
#include "rs485_comm.h"
#include "lamp.h"
// #include "other_device.h"

// void VoiceCommand::execute() {
//     if (is_sleep()) {
//         wakeup_heartbeat();

//         auto lamps = DeviceManager::getInstance().getDevicesOfType<Lamp>();
//         for (auto& lamp : lamps) {
//             if (lamp->isOn()) {
//                 lamp->updateButtonIndicator(true);
//             }
//         }

//         auto others = DeviceManager::getInstance().getDevicesOfType<PresetDevice>();
//         for (auto& other : others) {
//             if (other->type == OtherDeviceType::OUTPUT_CONTROL) {
//                 if (other->isOn()) {
//                     other->updateButtonIndicator(true);
//                 }
//             }
//         }
//     }

//     if (current_index < action_groups.size()) {
//         action_groups[current_index]->executeAllAtomicAction(mode_name);

//         current_index = (current_index + 1) % action_groups.size();
//     }
// }