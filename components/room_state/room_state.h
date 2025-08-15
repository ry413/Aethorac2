#pragma once

#include <vector>
#include <mutex>
#include <unordered_map>
#include "../json.hpp"

// 当前房间的图示状态, 显示在后台的
static std::unordered_map<std::string, time_t> state_map;
static std::mutex state_mutex;
void add_state(const std::string& state);
bool remove_state(const std::string& state);
void toggle_state(const std::string &state);
bool exist_state(const std::string &state);
nlohmann::json get_states_json();