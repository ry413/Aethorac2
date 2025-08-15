#include "room_state.h"
#include <algorithm>
#include "commons.h"

void add_state(const std::string& state) {
    std::lock_guard<std::mutex> lock(state_mutex);
    state_map[state] = get_current_timestamp();
}

bool remove_state(const std::string& state) {
    std::lock_guard<std::mutex> lock(state_mutex);
    return state_map.erase(state) != 0;
}

void toggle_state(const std::string &state) {
    std::lock_guard<std::mutex> lock(state_mutex);

    auto it = state_map.find(state);
    if (it != state_map.end())
        state_map.erase(it);
    else
        state_map.emplace(state, get_current_timestamp());
}

bool exist_state(const std::string &state) {
    std::lock_guard<std::mutex> lock(state_mutex);
    return state_map.find(state) != state_map.end();
}

nlohmann::json get_states_json() {
    std::lock_guard<std::mutex> lock(state_mutex);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [name, ts] : state_map) {
        arr.push_back({
            {"name", name},
            {"happentime", static_cast<long long>(ts)}
        });
    }
    return arr;     // 返回的 json 支持复制与移动
}