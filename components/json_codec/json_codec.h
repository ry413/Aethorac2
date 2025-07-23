#pragma once

std::vector<std::string_view> splitByLineView(std::string_view content);
void parseLocalLogicConfig(void);
nlohmann::json generateRegisterInfo();
nlohmann::json generateReportStates();