#include <string>
#include <vector>
#include "idevice.h"

class RS485Command : public IDevice {
public:
    std::vector<uint8_t> code;
    void execute(std::string operation, std::string parameter, int action_group_id = -1, bool should_log = false) override;
};
