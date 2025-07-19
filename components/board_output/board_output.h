#pragma once

#include <stdint.h>

// 输出类型
enum class OutputType {
    RELAY,
    DRY_CONTACT,
    LIGHT_MODULATOR
};

// 一个输出通道
class BoardOutput {
public:
    uint8_t host_board_id;
    OutputType type;
    uint8_t channel;
    uint16_t uid;

    // 闭合与断开电路
    void connect();
    void disconnect();

private:
    enum class State { CONNECTED, DISCONNECTED };
    State current_state = State::DISCONNECTED;

};