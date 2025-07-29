#include <string>
#include <vector>
#include "idevice.h"
#include "commons.h"

class RS485Command : public IDevice {
public:
    RS485Command(uint16_t did, const std::string& name, const std::string& carry_state, const std::string& code)
        : IDevice(did, DeviceType::RS485, name, carry_state) {
        this->code = pavectorseHexToFixedArray(code);
    }

    void execute(std::string operation, std::string parameter, ActionGroup* self_action_group = nullptr, bool should_log = false) override;
    void addAssBtn(PanelButtonPair) override { ESP_LOGW("RS485Command", "指令码设备不应该添加关联按钮"); }
    void syncAssBtnToDevState() override { ESP_LOGW("RS485Command", "指令码设备不应该有关联按钮"); }
    bool isOn() const override { ESP_LOGW("RS485Command", "指令码设备不应该调用isOn"); return false; }
    void updateButtonIndicator(bool state) override { ESP_LOGW("RS485Command", "指令码设备不应该更新指示灯"); }

private:
    std::vector<uint8_t> code;
};
