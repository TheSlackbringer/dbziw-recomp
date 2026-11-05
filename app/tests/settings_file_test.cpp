// DBZIW 007 (from 028 M2): the player's settings file. PS2X_SETTINGS_FILE chooses it, so the launcher can put it in the user's folder or with a
// portable installation; every setting survives a write and a read; comments and other lines stay; nothing appears in the working folder.
#include "ps2_runtime.h"
#include "runtime/ps2_settings.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

// The runtime links against the generated function table; this test has none.
extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace fs = std::filesystem;

namespace {
void require(bool ok, const char *what)
{
    if (!ok)
        throw std::runtime_error(what);
}

std::string load(const char *key)
{
    std::string value;
    return ps2x::settings::loadText(key, value) ? value : "<missing>";
}

std::string slurp(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
} // namespace

int main()
{
    try
    {
        const fs::path root = fs::temp_directory_path() / "dbziw settings test Jürgen";
        fs::remove_all(root);
        const fs::path work = root / "working", folder = root / "config";
        fs::create_directories(work);
        fs::create_directories(folder);
        fs::current_path(work);
        const fs::path file = folder / "dbziw.ini";
        const auto text = file.u8string();
#ifdef _WIN32
        _putenv_s("PS2X_SETTINGS_FILE", std::string(text.begin(), text.end()).c_str());
#else
        setenv("PS2X_SETTINGS_FILE", std::string(text.begin(), text.end()).c_str(), 1);
#endif
        require(load("resolution_scale") == "<missing>", "no file yet: missing");

        std::ofstream(file) << "; a player's comment\nunknown_key=kept\nresolution_scale=1 ; old value\n";
        require(load("resolution_scale") == "1", "reads a value with a trailing comment");

        ps2x::settings::saveNumber("resolution_scale", 4);
        ps2x::settings::saveNumber("aa_samples", 8);
        ps2x::settings::saveText("show_fps", "on");
        require(load("resolution_scale") == "4", "resolution_scale round trip");
        require(load("aa_samples") == "8", "aa_samples round trip");
        require(load("show_fps") == "on", "text round trip");
        ps2x::settings::saveNumber("aa_samples", 2);
        require(load("aa_samples") == "2", "second write replaces the value");

        const std::string saved = slurp(file);
        require(saved.find("; a player's comment") != std::string::npos, "comment kept");
        require(saved.find("unknown_key=kept") != std::string::npos, "unknown setting kept");
        require(saved.find("aa_samples=8") == std::string::npos, "no stale line");

        require(!fs::exists(work / "dbziw.ini") && !fs::exists(work / "dw5xl.ini"), "nothing in the working folder");
        fs::current_path(root.parent_path());
        fs::remove_all(root);
        std::cout << "settings file tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
