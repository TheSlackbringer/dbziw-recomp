#pragma once
#include <filesystem>
#include <string>

namespace dbziw::setup {
bool scheduleUninstall(const std::filesystem::path &package, const std::filesystem::path &root,
                       bool keepSaves, std::filesystem::path &failureLog, std::string &error);
}
