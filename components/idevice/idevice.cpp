#include "idevice.h"
#include "room_state.h"
#include "manager_base.h"

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

// void IDevice::sync_link_devices(std::string operation) {
//     // 如果此时是(进入)某种模式, 就不操控联动设备
//     if (!LordManager::getInstance().getCurrMode().empty()) {
//         return;
//     }
//     operated_flag = true;  // 标记为已被动过
//     for (auto link_uid : link_device_uids) {
//         auto device = DeviceManager::getInstance().getItem(link_uid);
//         // 只操作未被操作的
//         if (!device->is_operated()) {
//             device->execute(operation, "");
//         }
//     }
//     operated_flag = false;  // 在这里重置标记
// }

// void IDevice::close_repel_devices(void) {
//     for (auto repel_uid : repel_device_uids) {
//         DeviceManager::getInstance().getItem(repel_uid)->execute("关闭", "");
//     }
// }