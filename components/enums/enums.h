#pragma once

// enum class InputType {
//     LOW_LEVEL,
//     HIGH_LEVEL,
//     INFRARED
// };
// 这个理应与Laminor2的DeviceType顺序一样
enum class DeviceType {
    LAMP,
    CURTAIN,
    INFRARED_AIR,
    SINGLE_AIR,
    RS485,
    RELAY,
    DRY_CONTACT,

    // 预设设备类型
    HEARTBEAR,
    ROOM_STATE,
    DELAYER,
    ACTION_GROUP_OP,
    SNAPSHOT
};

// 这些也是
enum class InputType {
  PANEL_BTN,
  DRY_CONTACT
};

enum class InputTag {
  REMOVE_CARD_USABLE,
  IS_ALIVE_CHANNEL,
  IS_DOOR_CHANNEL,
  IS_DOORBELL_CHANNEL,
  NONE = 255
};

// 干接点输入触发类型
enum class TriggerType {
  LOW_LEVEL,
  HIGH_LEVEL,
  INFRARED,
  INFRARED_TIMEOUT,
};
