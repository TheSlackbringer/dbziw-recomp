#include "install_location.h"
#include "install_state.h"
#include "setup_layout.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace dbziw::setup;
namespace fs = std::filesystem;
static int failures = 0;
#ifdef _WIN32
static constexpr bool kWindowsHost = true;
#else
static constexpr bool kWindowsHost = false;
#endif
static void check(bool value, const char *name)
{
    std::cout << (value ? "PASS: " : "FAIL: ") << name << '\n';
    failures += !value;
}
static std::string read(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
static void write(const fs::path &p, const std::string &s)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}
static void env(const char *value)
{
#ifdef _WIN32
    _putenv_s("DBZIW_DATA_DIR", value);
#else
    if (*value)
        setenv("DBZIW_DATA_DIR", value, 1);
    else
        unsetenv("DBZIW_DATA_DIR");
#endif
}
int main()
{
    const auto scratch =
        fs::temp_directory_path() /
        ("dbziw_install_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto launcher = scratch / "package with spaces";
    fs::create_directories(launcher);
    fs::path result;
    bool explicitOverride = false;
    std::string error;
    env("");
    check(resolveInstallLocation(launcher, {}, result, explicitOverride, error) && result == launcher / "dbziw-data" &&
              !explicitOverride,
          "missing configuration uses dbziw-data beneath launcher");
#ifdef _WIN32
    check(suggestedInstallFolder() == fs::path(LR"(C:\Games\DBZIW-Recomp)"), R"(the Windows folder field starts at C:\Games\DBZIW-Recomp)");
#else
    setenv("HOME", "/home/fixture", 1);
    check(suggestedInstallFolder() == fs::path("/home/fixture/Games/DBZIW-Recomp"), "the Linux folder field starts at ~/Games/DBZIW-Recomp");
    setenv("HOME", "relative/home", 1);
    check(suggestedInstallFolder().empty(), "a relative $HOME leaves the folder field empty");
    unsetenv("HOME");
    check(suggestedInstallFolder().empty(), "no $HOME leaves the folder field empty");
#endif
    InstallFolderSelection selection{launcher, launcher};
    check(selection.choose({}) && selection.destination == launcher,
          "cancelled folder picker preserves selection");
    const auto child = launcher / pathFromText("custom \xc3\xb8 \xe6\xb8\xac\xe8\xa9\xa6");
    selection.frozen = true;
    check(!selection.choose(child) && selection.destination == launcher, "active operation freezes destination");
    selection.frozen = false;
    check(selection.choose(child) && selection.destination == child, "custom Unicode child folder chosen");
    check(!fs::exists(child), "choosing a folder does not create it");
    const auto external = scratch / "external choice";
    fs::create_directories(external);
    env(pathText(external).c_str());
    check(resolveInstallLocation(launcher, {}, result, explicitOverride, error) && result == external && explicitOverride,
          "environment overrides the default");
    check(resolveInstallLocation(launcher, scratch / "command", result, explicitOverride, error) && result == scratch / "command",
          "command line overrides environment");
    env("");
    check(!fs::exists(launcher / "dbziw-install.json"), "no choice is stored beside the launcher");
    check(!installPathFits(fs::path("C:/") / std::string(200, 'x'), error) || !kWindowsHost, "a data folder too deep for Windows is refused");
    check(installPathFits(fs::path("C:/Games/DBZIW-Recomp/dbziw-data"), error), "the suggested folder fits");
    write(scratch / "file", "not a directory");
    check(!validateInstallDirectory(scratch / "file", error) && error.find("file") != std::string::npos,
          "file destination rejected with selected path");
    check(checkInstallDirectory(scratch / "unmade" / "deep", error) && !fs::exists(scratch / "unmade"),
          "checking a missing destination does not create it");
    check(checkInstallDirectory(external, error) && fs::is_directory(external),
          "existing destination passes the check");
    check(!checkInstallDirectory(scratch / "file", error) && error.find("file") != std::string::npos,
          "checking a file destination rejects it");
    check(!checkInstallDirectory(scratch / "file" / "child", error) && error.find("child") != std::string::npos,
          "checking a destination below a file rejects it");
    size_t leftovers = 0;
    for (const auto &entry : fs::directory_iterator(scratch))
        leftovers += entry.path().filename().string().rfind(".dbziw-write-check-", 0) == 0;
    check(leftovers == 0, "checking a destination leaves no access-check file");
    const auto package = scratch / "overlapping-package";
    write(package / "app/source.cpp", "authored source");
    for (const char *folder : {"external/keep", "recomp/keep", "tools/keep", "BUILD_ID"})
        write(package / folder, "synthetic");
    const auto data = package / "app/custom-data";
    write(data / "save", "preserve");
    fs::path builder;
    check(prepareLinuxBuildRoot(package, data, builder, error) &&
              prepareLinuxBuildRoot(package, data, builder, error),
          "nested custom destination can prepare its Linux builder twice");
    check(read(builder / "app/source.cpp") == "authored source" && !fs::exists(builder / "app/custom-data") &&
              read(data / "save") == "preserve",
          "builder excludes the installation subtree without copying itself or player files");
    std::cout << (failures ? "FAIL" : "PASS") << ": install-location fixtures\n";
    return failures ? 1 : 0;
}
