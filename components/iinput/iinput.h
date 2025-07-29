#pragma once

#include <memory>
#include <string>
#include "enums.h"
#include "action_group.h"

class InputBase {
public:
    InputBase(uint16_t iid, InputType type, const std::string& name, InputTag tag, std::vector<std::unique_ptr<ActionGroup>>&& action_groups)
        : iid(iid), type(type), name(name), tag(tag), action_groups(std::move(action_groups)) {}
        
    virtual void execute() = 0;
    uint16_t getIid() const { return iid; }
    InputType getType() const { return type; }
    const std::string& getName() const { return name; }
    InputTag getTag() const { return tag; }

protected:
    uint16_t iid;
    InputType type;
    std::string name;
    InputTag tag;
    std::vector<std::unique_ptr<ActionGroup>> action_groups;

    uint8_t current_index = 0;
};
