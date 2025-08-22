#pragma once

// 这个理应与Laminor2的DeviceType顺序一样
enum class DeviceType {
  LAMP,
  CURTAIN,
  INFRARED_AIR,
  SINGLE_AIR,
  RS485,
  RELAY,
  DRY_CONTACT,
  DOORBELL,

  // 预设设备类型
  HEARTBEAT,
  ROOM_STATE,
  DELAYER,
  ACTION_GROUP_OP,
  SNAPSHOT,
  INDICATOR,
  NONE = 255
};

// 这些也是
enum class InputType {
  PANEL_BTN,
  DRY_CONTACT,
  VOICE_CMD,
  NONE = 255
};

enum class InputTag {
  REMOVE_CARD_USABLE,
  IS_ALIVE_CHANNEL,
  IS_DOOR_CHANNEL,
  IS_DOORBELL_CHANNEL,
  IGNORE_ANY_KEY_EXECUTE,
  NONE = 255
};

// 干接点输入触发类型
enum class TriggerType {
  LOW_LEVEL,
  HIGH_LEVEL,
  INFRARED,
  INFRARED_TIMEOUT,
  NONE = 255
};
