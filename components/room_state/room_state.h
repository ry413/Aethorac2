#ifndef ROOM_STATE_H
#define ROOM_STATE_H

#include <vector>
#include <mutex>

// 当前房间的图示状态, 显示在后台的
static std::vector<std::string> state_array;
static std::mutex state_mutex;
void add_state(const std::string& state);
bool remove_state(const std::string& state);
void toggle_state(const std::string &state);
bool exist_state(const std::string &state);
std::vector<std::string> get_states();

#endif