#include "manager_base.h"
#include <string.h>
#include "voice_command.h"
// #include "board_config.h"

// void VoiceCommandManager::add_voice_command(std::shared_ptr<VoiceCommand> command) {
//     all_voice_commands.push_back(command);
// }

// void VoiceCommandManager::handle_voice_command(uint8_t* data) {
//     for (auto& command : all_voice_commands) {
//         if (memcmp(command->code.data(), data, 8) == 0) {
//             command->execute();
//         }
//     }
// }

// std::shared_ptr<BoardOutput> BoardManager::getBoardOutput(uint16_t uid) {
//     for (const auto& [board_id, board] : getAllItems()) {
//         auto it = board->outputs.find(uid);
//         if (it != board->outputs.end()) {
//             return it->second; // 返回共享指针
//         }
//     }
//     return nullptr; // 未找到，返回空指针
// }