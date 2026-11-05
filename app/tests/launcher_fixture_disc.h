#pragma once
// Launcher tests: a made-up disc of one small file, so an "installed" fixture needs no disc-sized files and nothing from the game.
#include <cstdint>

namespace dbziw::setup {

struct DiscFile
{
    const char *path;
    uint64_t size;
    const char *sha256;
};

inline constexpr DiscFile kDbziwPal[] = {
    {"SLES_553.47", 36u, "0000000000000000000000000000000000000000000000000000000000000000"},
};

} // namespace dbziw::setup
