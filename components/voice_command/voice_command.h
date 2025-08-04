#ifndef VOICE_COMMAND_H
#define VOICE_COMMAND_H

#include <string>
#include <vector>
#include "commons.h"
#include "action_group.h"
#include "iinput.h"

class VoiceCommand : public InputBase {
public:
    VoiceCommand(uint16_t iid, const std::string& name, InputTag tag, const std::string& code, std::vector<std::unique_ptr<ActionGroup>>&& action_groups)
        : InputBase(iid, InputType::VOICE_CMD, name, tag, std::move(action_groups)) {
            this->code = pavectorseHexToFixedArray(code);
        }

    void execute() override;
    std::vector<uint8_t> getCode() const { return code; }
private:
    std::vector<uint8_t> code;
};


#endif // VOICE_COMMAND_H