// Authored regression (DBZIW 034): a memory card is a host folder, and the card listing must hold only what a PS2
// card can: "." and "..", then the folder's own entries, without host-only files (dot files, Windows Explorer's
// desktop.ini and Thumbs.db, names longer than a card entry). DBZIW accepts a save only when its folder lists
// exactly 5 entries. No game code or data; the card is a temporary folder.
#include "ps2_runtime.h"
#include "ps2_stubs.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {
constexpr uint32_t kPath = 0x10000u, kTable = 0x20000u, kCmd = 0x30000u, kResult = 0x30004u;

void require(bool ok, const std::string &what)
{
    if (!ok)
    {
        std::cerr << "FAILED: " << what << '\n';
        throw std::runtime_error(what);
    }
}

void reg(R5900Context &c, unsigned n, uint32_t v) { c.r[n] = _mm_cvtsi32_si128(static_cast<int>(v)); }

void touch(const std::filesystem::path &p, size_t bytes)
{
    std::ofstream(p, std::ios::binary) << std::string(bytes, 'x');
}

// sceMcGetDir(port 0, slot 0, path, 0, maxent, table), then sceMcSync for the entry count; returns the names.
std::vector<std::string> listing(PS2Runtime &runtime, std::vector<uint8_t> &ram, const char *path, int &count)
{
    std::memset(ram.data() + kTable, 0, 64 * 32);
    std::strcpy(reinterpret_cast<char *>(ram.data() + kPath), path);
    R5900Context ctx{};
    reg(ctx, 4, 0); reg(ctx, 5, 0); reg(ctx, 6, kPath); reg(ctx, 7, 0); reg(ctx, 8, 32); reg(ctx, 9, kTable);
    ps2_stubs::sceMcGetDir(ram.data(), &ctx, &runtime);
    R5900Context sync{};
    reg(sync, 4, 1); reg(sync, 5, kCmd); reg(sync, 6, kResult);
    ps2_stubs::sceMcSync(ram.data(), &sync, &runtime);
    std::memcpy(&count, ram.data() + kResult, sizeof(count));
    std::vector<std::string> names;
    for (int i = 0; i < count && i < 32; ++i)
        names.emplace_back(reinterpret_cast<const char *>(ram.data() + kTable + i * 64 + 32));
    return names;
}
}

int main()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "dbziw_mc_dir_listing_test";
    try
    {
        fs::remove_all(root);
        const fs::path save = root / "mc0" / "BESLES-55347DBZIW";
        fs::create_directories(save);
        touch(save / "icon.sys", 0x3c4);
        touch(save / "icon0.ico", 0xb006);
        touch(save / "BESLES-55347DBZIW", 0x4320);

        PS2Runtime::IoPaths paths = PS2Runtime::getIoPaths();
        paths.mcRoot = root / "mc0";
        PS2Runtime::setIoPaths(paths);
        PS2Runtime runtime;
        std::vector<uint8_t> ram(0x2000000u, 0);

        int count = 0;
        auto names = listing(runtime, ram, "/BESLES-55347DBZIW/*", count);
        require(count == 5, "a clean save folder lists 5 entries, got " + std::to_string(count));

        touch(save / "desktop.ini", 10);
        touch(save / "Thumbs.db", 10);
        touch(save / ".DS_Store", 10);
        touch(save / "._icon.sys", 10);
        touch(save / "a-name-longer-than-a-card-entry-allows.txt", 10);
        names = listing(runtime, ram, "/BESLES-55347DBZIW/*", count);
        require(count == 5, "host-only files are not listed, got " + std::to_string(count));
        for (const char *want : {".", "..", "icon.sys", "icon0.ico", "BESLES-55347DBZIW"})
        {
            bool found = false;
            for (const auto &n : names)
                found = found || n == want;
            require(found, std::string("listed: ") + want);
        }

        fs::remove_all(root);
        std::cout << "memory-card listing tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
