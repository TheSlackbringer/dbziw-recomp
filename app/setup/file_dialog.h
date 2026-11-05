#pragma once
// "Browse..." for the launcher: the system's own pickers for a disc image and a folder.
// Windows: the standard Open dialog. Linux: zenity or kdialog when installed. Drag and drop and typing the path
// work everywhere, so a missing picker is never a dead end.
#include <filesystem>
#include <string>

namespace dbziw::setup {

// true if this system has a picker the wizard can use.
bool fileDialogAvailable();

// Shows the picker; returns the chosen file, or empty when cancelled or unavailable.
std::filesystem::path pickDiscImage(const std::string &title);

// Shows the folder picker; returns the chosen folder, or empty when cancelled or unavailable.
std::filesystem::path pickFolder(const std::string &title);

} // namespace dbziw::setup
