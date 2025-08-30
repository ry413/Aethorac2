#pragma once

#include <memory>
#include <set>
#include <string>
#include "enums.h"
#include "action_group.h"

class InputBase {
public:
    InputBase(uint16_t iid, InputType type, const std::string& name, std::set<InputTag> tags, std::vector<std::unique_ptr<ActionGroup>>&& action_groups)
        : iid(iid), type(type), name(name), tags(tags), action_groups(std::move(action_groups)) {}
        
    virtual void execute() = 0;
    uint16_t getIid() const { return iid; }
    InputType getType() const { return type; }
    const std::string& getName() const { return name; }
    std::set<InputTag> getTags() const { return tags; }

protected:
    uint16_t iid;
    InputType type;
    std::string name;
    std::set<InputTag> tags;
    std::vector<std::unique_ptr<ActionGroup>> action_groups;

    uint8_t current_index = 0;
};
