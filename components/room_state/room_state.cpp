#include "room_state.h"
#include <algorithm>

void add_state(const std::string& state) {
    std::lock_guard<std::mutex> lock(state_mutex);  // 加锁保护
    if (std::find(state_array.begin(), state_array.end(), state) == state_array.end()) {
        // 如果状态不存在，则添加
        state_array.push_back(state);
    }
}

bool remove_state(const std::string& state) {
    std::lock_guard<std::mutex> lock(state_mutex);  // 加锁保护
    auto it = std::find(state_array.begin(), state_array.end(), state);
    if (it != state_array.end()) {
        // 如果找到状态，则删除
        state_array.erase(it);
        return true;  // 删除成功
    }
    return false;  // 未找到，删除失败
}

void toggle_state(const std::string &state) {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto it = std::find(state_array.begin(), state_array.end(), state);
    if (it != state_array.end()) {
        state_array.erase(it);  // 如果存在，则删除
    } else {
        state_array.push_back(state);  // 如果不存在，则添加
    }
}

bool exist_state(const std::string &state) {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto it = std::find(state_array.begin(), state_array.end(), state);
    return it != state_array.end();
}

std::vector<std::string> getRoomStates() {
    std::lock_guard<std::mutex> lock(state_mutex);
    return state_array;  // 返回副本
}