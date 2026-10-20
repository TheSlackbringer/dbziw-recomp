#pragma once
// Where the setup wizard finds the project and its tools.
#include "build_game.h"

#include <filesystem>
#include <string>

namespace dbziw::setup {

struct SetupLayout
{
    std::filesystem::path root;
    std::filesystem::path recompiler;
    BuildTools tools;
};

// The project root at or above `start`: the first folder that has app/CMakeLists.txt and recomp/. Empty if none.
std::filesystem::path findRoot(const std::filesystem::path &start);

// The tools for `root`; `windows` selects the Windows names and the bundled compiler. false + error when a tool
// the build needs is missing.
bool findLayout(const std::filesystem::path &root, bool windows, SetupLayout &layout, std::string &error);

} // namespace dbziw::setup
