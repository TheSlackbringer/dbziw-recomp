#pragma once
// Setup wizard step "generate the game's C++": runs the recompiler on the main program.
#include <filesystem>
#include <functional>
#include <string>

namespace dbziw::setup {

struct CodegenProfile
{
    const char *name;
    const char *config;    // recompiler configuration, relative to root
    const char *input;     // disc file it translates, relative to root
    const char *ghidraCsv; // function list, relative to root
    const char *output;    // generated C++ folder, relative to root
};

inline constexpr CodegenProfile kCodegenProfiles[] = {
    {"main", "recomp/dbz_iw.toml", "game/SLES_553.47", "recomp/ghidra_functions.csv", "recomp/output"},
};

// Rewrites the input / ghidra_output / output lines of a recompiler configuration (TOML text) to the given paths.
std::string rewriteRecompConfig(const std::string &config, const std::filesystem::path &input,
                                 const std::filesystem::path &ghidraCsv, const std::filesystem::path &output);

// Makes `target` hold exactly the files of `source`.
bool syncDirectory(const std::filesystem::path &source, const std::filesystem::path &target, std::string &error);

// Runs the recompiler for every profile.
bool generateCode(const std::filesystem::path &root, const std::filesystem::path &recompiler,
                  const std::filesystem::path &logDir,
                  const std::function<bool(const char *, int, int)> &progress, std::string &error,
                  const std::filesystem::path &dataDir = {});

} // namespace dbziw::setup
