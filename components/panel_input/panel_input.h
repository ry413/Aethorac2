#pragma once

#include <unordered_map>
#include <memory>
#include <vector>
#include <unordered_map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "action_group.h"
#include "iinput.h"

class PanelButtonInput : public InputBase {
public:
    PanelButtonInput(uint8_t iid, const std::string& name, uint8_t bid, InputTag tag, std::vector<std::unique_ptr<ActionGroup>>&& action_groups)
        : InputBase(iid, InputType::PANEL_BTN, name, tag, std::move(action_groups)), bid(bid) {}

    void execute() override;
    uint8_t getBid() const { return bid; }

private:
    uint8_t bid;
};

class Panel {
public:
    Panel(uint8_t pid)
        : pid(pid) {}
    
    bool addButton(uint8_t iid, const std::string& name, uint8_t bid, InputTag tag, std::vector<std::unique_ptr<ActionGroup>>&& action_groups) {
        auto btn = std::make_unique<PanelButtonInput>(iid, name, bid, tag, std::move(action_groups));
        auto [it, ok] = buttons_map.try_emplace(bid, std::move(btn));
        return ok;
    }
    uint8_t getPid() const { return pid; }
    
private:
    uint8_t pid;
    std::unordered_map<uint8_t, std::unique_ptr<PanelButtonInput>> buttons_map;
};