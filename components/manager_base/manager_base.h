#ifndef MANAGER_BASE_H
#define MANAGER_BASE_H

#include <variant>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <memory>

class ActionGroup;

// 基础模板类 SingletonManager，接受派生类作为模板参数
template <typename Derived>
class SingletonManager {
public:
    // 返回派生类的单例实例
    static Derived& getInstance() {
        static Derived instance; // 静态局部变量，保证线程安全
        return instance;
    }

    // 禁用拷贝构造和赋值运算
    SingletonManager(const SingletonManager&) = delete;
    SingletonManager& operator=(const SingletonManager&) = delete;

protected:
    SingletonManager() = default;
    ~SingletonManager() = default;
};

// ResourceManager 模板，提供通用资源管理功能，接受 KeyType、ValueType 和 Derived
template <typename KeyType, typename ValueType, typename Derived>
class ResourceManager : public SingletonManager<Derived> {
public:
    void addItem(const KeyType& id, std::shared_ptr<ValueType> item) {
        std::lock_guard<std::mutex> lock(map_mutex);
        resource_map[id] = item;
    }

    std::shared_ptr<ValueType> getItem(const KeyType& id) const {
        auto it = resource_map.find(id);
        return (it != resource_map.end()) ? it->second : nullptr;
    }

    void removeItem(const KeyType& id) {
        std::lock_guard<std::mutex> lock(map_mutex);
        resource_map.erase(id);
    }

    const std::unordered_map<KeyType, std::shared_ptr<ValueType>>& getAllItems() const {
        return resource_map;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(map_mutex);
        resource_map.clear();
    }

protected:
    ResourceManager() = default;
    ~ResourceManager() = default;

private:
    mutable std::mutex map_mutex;
    std::unordered_map<KeyType, std::shared_ptr<ValueType>> resource_map;

};

class PanelButton;      // 前向声明
class BoardOutput;
class IDevice;

// 设备管理者
class DeviceManager : public SingletonManager<DeviceManager> {
public:
    void addItem(const uint16_t& id, std::shared_ptr<IDevice> item) {
        std::lock_guard<std::mutex> lock(map_mutex);
        resource_map[id] = item;
        printf("已添加设备 %d\n", id);
    }

    std::shared_ptr<IDevice> getItem(const uint16_t& id) const {
        std::lock_guard<std::mutex> lock(map_mutex);
        auto it = resource_map.find(id);
        return (it != resource_map.end()) ? it->second : nullptr;
    }

    void removeItem(const uint16_t& id) {
        std::lock_guard<std::mutex> lock(map_mutex);
        resource_map.erase(id);
    }

    const std::unordered_map<uint16_t, std::shared_ptr<IDevice>>& getAllItems() const {
        std::lock_guard<std::mutex> lock(map_mutex);
        return resource_map;
    }

    // 返回所有特定类型的设备
    template <typename T>
    std::vector<std::shared_ptr<T>> getDevicesOfType() const {
        static_assert(std::is_base_of<IDevice, T>::value, "T must derive from IDevice");

        std::vector<std::shared_ptr<T>> devices;
        std::lock_guard<std::mutex> lock(map_mutex);
        
        for (const auto& [id, device] : resource_map) {
            if (auto casted = std::dynamic_pointer_cast<T>(device)) {
                devices.push_back(casted);
            }
        }
        return devices;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(map_mutex);
        resource_map.clear();
    }

protected:
    DeviceManager() = default;
    ~DeviceManager() = default;

private:
    friend class SingletonManager<DeviceManager>;
    mutable std::mutex map_mutex;
    std::unordered_map<uint16_t, std::shared_ptr<IDevice>> resource_map;
};

// 动作组管理者
class ActionGroupManager : public SingletonManager<ActionGroupManager> {
public:
    void addItem(const uint16_t& id, std::shared_ptr<ActionGroup> item) {
        std::lock_guard<std::mutex> lock(map_mutex);
        resource_map[id] = item;
    }

    std::shared_ptr<ActionGroup> getItem(const uint16_t& id) const {
        std::lock_guard<std::mutex> lock(map_mutex);
        auto it = resource_map.find(id);
        return (it != resource_map.end()) ? it->second : nullptr;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(map_mutex);
        resource_map.clear();
    }

protected:
    ActionGroupManager() = default;
    ~ActionGroupManager() = default;

private:
    friend class SingletonManager<ActionGroupManager>;
    mutable std::mutex map_mutex;
    std::unordered_map<uint16_t, std::shared_ptr<ActionGroup>> resource_map;
};

class VoiceCommand;

// 语音指令管理者
class VoiceCommandManager {
public:
    static VoiceCommandManager& getInstance() {
        static VoiceCommandManager instance;
        return instance;
    }

    VoiceCommandManager(const VoiceCommandManager&) = delete;
    VoiceCommandManager& operator=(const VoiceCommandManager&) = delete;

    void add_voice_command(std::shared_ptr<VoiceCommand> command);
    void handle_voice_command(uint8_t* data);

    void clear() {
        all_voice_commands.clear();
    }

private:
    VoiceCommandManager() = default;
    
    std::vector<std::shared_ptr<VoiceCommand>> all_voice_commands;
};



// 板子管理类
class BoardConfig;
class BoardManager : public ResourceManager<uint16_t, BoardConfig, BoardManager> {
    friend class SingletonManager<BoardManager>;
private:
    BoardManager() = default;

public:
    std::shared_ptr<BoardOutput> getBoardOutput(uint16_t uid);
};

// 面板管理类
class Panel;
class PanelManager : public ResourceManager<uint8_t, Panel, PanelManager> {
    friend class SingletonManager<PanelManager>;
private:
    PanelManager() = default;
};

// 超级管理者
class LordManager {
public:
    static LordManager& getInstance() {
        static LordManager instance;
        return instance;
    }

    LordManager(const LordManager&) = delete;
    LordManager& operator=(const LordManager&) = delete;

    // 设置配置数据
    void setVersion(const std::string& version) {
        config_version = version;
    }

    void setAliveChannel(const uint8_t channel) {
        alive_channel = channel;
    }

    void setDoorChannel(const uint8_t channel) {
        door_channel = channel;
    }

    void setWaitToDieState(const std::string& state) {
        wait_to_die_state = state;
    }
    
    void setWaitToDieSec(int sec) {
        wait_to_die_sec = sec;
    }

    // 获取配置数据
    const std::string& getConfigVersion() const { return config_version; }
    uint8_t getAliveChannel() { return alive_channel; }
    uint8_t getDoorChannel() { return door_channel; }
    const std::string& getWaitToDieState() const { return wait_to_die_state; }
    int getWaitToDieSec() const { return wait_to_die_sec; }

    void setCurrMode(const std::string& mode) { curr_mode = mode; }
    std::string getCurrMode(void) { return curr_mode; }

    void clear() {
        config_version.clear();
        wait_to_die_state.clear();
        wait_to_die_sec = 0;
        curr_mode.clear();
    }
    
private:
    LordManager() = default;

    // 配置数据
    std::string config_version;
    uint8_t alive_channel;
    uint8_t door_channel;
    std::string wait_to_die_state;
    int wait_to_die_sec = 0;

    std::string curr_mode;
};

#endif // MANAGER_BASE_H
