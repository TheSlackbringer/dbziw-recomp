#include "installation.h"
#include "install_location.h"
#include "install_state.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace dbziw::setup;
namespace fs = std::filesystem;
int failures = 0;
void check(bool ok, const char *name)
{
    std::cout << (ok ? "PASS: " : "FAIL: ") << name << '\n';
    failures += !ok;
}
void put(const fs::path &p, const std::string &text)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}
std::string read(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
void env(const char *name, const fs::path &value)
{
#ifdef _WIN32
    _wputenv_s(pathFromText(name).c_str(), value.c_str());
#else
    setenv(name, pathText(value).c_str(), 1);
#endif
}
void installed(const fs::path &root)
{
    put(root / "game/SLES_553.47", "authored synthetic non-retail bytes\n");
    std::string error;
    if (!writeInstallMarker(root, kSupportedDiscs[0], error))
        check(false, error.c_str());
}
int main()
{
    auto scratch =
        fs::temp_directory_path() /
        ("dbziw_modes_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    // Windows temp paths can use short names (RUNNER~1), but installation roots are resolved.
    fs::create_directories(scratch);
    scratch = fs::canonical(scratch);
    env("LOCALAPPDATA", scratch / "home");
    env("HOME", scratch / "home");
    env("XDG_CONFIG_HOME", scratch / "config-home");
    env("XDG_DATA_HOME", scratch / "data-home");
    std::string error;
    Installation install;
    const auto root = scratch / pathFromText("release ø 測試") / "dbziw-data";
    const auto games = scratch / "Games";
    fs::create_directories(games);
    check(newInstallationRoot(games) == games / "DBZIW-Recomp/dbziw-data",
          "custom parent gets a self-contained DBZIW-Recomp installation");
    check(newInstallationRoot(games / "DBZIW-Recomp") == games / "DBZIW-Recomp/dbziw-data",
          "selecting the branded folder does not create a second branded folder");
    check(newInstallationRoot(root) == root, "the data folder does not get nested");
    installed(games / "DBZIW-Recomp/dbziw-data");
    check(existingInstallationRoot(games) == games / "DBZIW-Recomp/dbziw-data", "find an existing DBZIW child from its parent");
    fs::remove(games / "DBZIW-Recomp/dbziw-data/game/dbziw.verified");
    check(loadInstallation(root, install, error) && install.mode == SaveMode::Normal && !install.recorded,
          "new installation defaults to normal saves");
    check(install.userData == normalUserDataDirectory() && install.configDir == normalConfigDirectory(),
          "normal saves and config use platform user directories");
    check(prepareInstallation(install, error), "prepare normal installation atomically");
    installed(root);
    put(install.userData / "mc0/BESLES-55347/save", "current progress");
    put(install.userData / "mc1/slot2", "second card");
    put(install.configDir / "dbziw.ini", "settings");
    auto oldUser = install.userData, oldConfig = install.configDir;
    check(rememberNormalInstallation(install, error),
          "normal install location recorded outside downloaded release");
    {
        InstallationLock held;
        check(held.acquire(root, error), "lock existing installation");
        bool blocked = false;
        std::thread other([&] {
            auto copy = install;
            std::string lockError;
            blocked = !switchSaveMode(copy, SaveMode::Portable, SaveConflict::Ask, lockError);
        });
        other.join();
        check(blocked, "another management transaction cannot race the active installation lock");
    }
    check(switchSaveMode(install, SaveMode::Portable, SaveConflict::Ask, error),
          "normal to portable copies saves and settings");
    check(install.userData == root / "user-data" && install.configDir == install.userData,
          "portable paths stay with installation");
    check(read(install.userData / "mc0/BESLES-55347/save") == "current progress" &&
              read(install.configDir / "dbziw.ini") == "settings",
          "portable progress/settings copied exactly");
    check(read(oldUser / "mc0/BESLES-55347/save") == "current progress" &&
              read(oldConfig / "dbziw.ini") == "settings",
          "mode switch retains original files");
    Installation restarted;
    check(loadInstallation(root, restarted, error) && restarted.recorded &&
              restarted.mode == SaveMode::Portable,
          "update reads persisted portable mode without asking again");
    put(install.userData / "mc0/BESLES-55347/save", "new portable progress");
    const auto before = read(root / "install.json");
    check(!switchSaveMode(install, SaveMode::Normal, SaveConflict::Ask, error) &&
              error.find("conflict") != std::string::npos,
          "conflicting destination saves require an explicit choice");
    check(read(root / "install.json") == before && install.mode == SaveMode::Portable,
          "conflict leaves old mode and source progress active");
    check(switchSaveMode(install, SaveMode::Normal, SaveConflict::UseSource, error) &&
              read(oldUser / "mc0/BESLES-55347/save") == "new portable progress",
          "explicit current-save choice switches mode");
    bool backup = false;
    for (const auto &entry : fs::directory_iterator(oldUser / "mc0/BESLES-55347"))
        if (entry.path().filename() != "save" && read(entry.path()) == "current progress")
            backup = true;
    check(backup, "overwritten destination progress has a verified backup");
    check(switchSaveMode(install, SaveMode::Portable, SaveConflict::Ask, error),
          "switch again after keeping verified source copies");
    put(install.userData / "mc0/BESLES-55347/save", "different portable branch");
    check(switchSaveMode(install, SaveMode::Normal, SaveConflict::KeepDestination, error) &&
              read(oldUser / "mc0/BESLES-55347/save") == "new portable progress",
          "explicit destination-save choice retains destination progress");
    const auto newZip = scratch / "downloads/new-version";
    fs::create_directories(newZip);
    std::vector<fs::path> candidates;
    check(discoverInstallations(newZip, candidates, error) && candidates.size() == 1 &&
              fs::equivalent(candidates[0], root),
          "fresh ZIP finds recorded installation");
    check(loadInstallation(candidates[0], restarted, error) && restarted.mode == SaveMode::Normal,
          "discovery retains installation mode");
    const auto local = newZip / "dbziw-data";
    installed(local);
    check(discoverInstallations(newZip, candidates, error) && candidates.size() == 1 &&
              fs::equivalent(candidates[0], local),
          "valid local installation takes priority over machine record");
    const auto portableOnly = scratch / "portable-only/dbziw-data";
    Installation standalone;
    check(loadInstallation(portableOnly, standalone, error), "load independent portable installation");
    standalone.mode = SaveMode::Portable;
    standalone.userData = standalone.configDir = portableOnly / "user-data";
    const auto indexBefore = read(normalConfigDirectory() / "installations.json");
    check(prepareInstallation(standalone, error) &&
              read(normalConfigDirectory() / "installations.json") == indexBefore,
          "portable install makes no machine index write");
    const auto legacy = scratch / "legacy";
    installed(legacy);
    put(legacy / "install.json", R"({"version":1,"save_mode":"portable"})");
    put(legacy / "user-data/mc0/BESLES-55347/save", "legacy progress");
    put(legacy / "user-data/dbziw.ini", "legacy settings");
    put(legacy / "build/_deps/keep", "dependency sentinel");
    Installation imported;
    loadInstallation(scratch / "import/dbziw-data", imported, error);
    imported.mode = SaveMode::Portable;
    imported.userData = imported.configDir = imported.root / "user-data";
    check(importInstallation(legacy, imported, SaveConflict::Ask, error),
          "copy import reuses installed game files");
    check(read(imported.userData / "mc0/BESLES-55347/save") == "legacy progress" &&
              read(imported.configDir / "dbziw.ini") == "legacy settings" &&
              read(imported.root / "build/_deps/keep") == "dependency sentinel",
          "import retains saves/settings and build dependencies");
    check(read(legacy / "user-data/mc0/BESLES-55347/save") == "legacy progress" &&
              read(legacy / "user-data/dbziw.ini") == "legacy settings",
          "the source of a copy stays unchanged");
    put(imported.root / "package/launcher/DBZIW.exe", "packaged launcher");
    put(imported.root / "package/tools/keep", "packaged tools");
    put(imported.root / "builder/app/keep", "generated builder");
    put(imported.root / "recomp/keep", "generated code");
    put(imported.root / "cache/snapshots/keep", "player snapshot");
    const auto machineIndex = read(normalConfigDirectory() / "installations.json");
    {
        InstallationLock held;
        check(held.acquire(imported.root, error), "lock installation for simulated setup");
        bool blocked = false;
        std::thread attempt([&] {
            std::string lockError;
            blocked = !uninstallInstallation(imported.root, true, lockError);
        });
        attempt.join();
        check(blocked && fs::exists(imported.root / "game/SLES_553.47"),
              "uninstall cannot remove files while another operation holds the lock");
    }
    check(uninstallInstallation(imported.root, true, error),
          "uninstall verified portable game files");
    check(!fs::exists(imported.root / "game/SLES_553.47") &&
              !fs::exists(imported.root / "game/dbziw.verified") &&
              !fs::exists(imported.root / "build/_deps/keep") &&
              !fs::exists(imported.root / "builder") && !fs::exists(imported.root / "recomp"),
          "uninstall removes extracted and built files");
    check(!fs::exists(imported.root / "install.json") &&
              read(imported.userData / "mc0/BESLES-55347/save") == "legacy progress" &&
              !fs::exists(imported.configDir / "dbziw.ini") &&
              read(imported.root / "package/launcher/DBZIW.exe") == "packaged launcher" &&
              read(imported.root / "package/tools/keep") == "packaged tools" &&
              !fs::exists(imported.root / "cache") &&
              read(normalConfigDirectory() / "installations.json") == machineIndex,
          "uninstall keeps portable progress and leaves files outside the branded payload");
    check(uninstallInstallation(legacy, true, error) &&
              !fs::exists(legacy / "user-data/dbziw.ini") &&
              read(legacy / "user-data/mc0/BESLES-55347/save") == "legacy progress" &&
              !fs::exists(legacy / "build/_deps/keep"),
          "uninstall of a plain folder keeps saves in place and removes settings and build files");
    put(scratch / "unverified/build/keep", "unrelated");
    check(!uninstallInstallation(scratch / "unverified", true, error) &&
              read(scratch / "unverified/build/keep") == "unrelated",
          "uninstall refuses an unverified folder");
    const auto fullFolder = scratch / "uninstall" / "DBZIW-Recomp";
    const auto fullData = fullFolder / "dbziw-data";
    installed(fullData);
    put(fullData / "install.json", R"({"version":1,"save_mode":"portable"})");
    put(fullData / "user-data/mc0/progress", "keep this save");
    put(fullData / "user-data/dbziw.ini", "old settings");
    put(fullData / "cache/snapshots/snapshot", "old snapshot");
    put(fullData / "logs/build.log", "old log");
    put(fullFolder / "README.txt", "old documentation");
#ifndef _WIN32
    put(scratch / "outside", "symlink destination");
    fs::create_directories(fullData / "builder");
    fs::create_symlink(scratch / "outside", fullData / "builder/linked-tool");
#endif
    check(uninstallInstallation(fullData, true, error) &&
              read(fullData / "user-data/mc0/progress") == "keep this save" &&
              !fs::exists(fullData / "install.json") && !fs::exists(fullData / "user-data/dbziw.ini") &&
              !fs::exists(fullData / "cache") && !fs::exists(fullData / "logs") &&
              !fs::exists(fullData / "game") && !fs::exists(fullFolder / "README.txt"),
          "full uninstall leaves only the selected portable saves");
#ifndef _WIN32
    check(read(scratch / "outside") == "symlink destination",
          "uninstall removes internal Linux symlinks without touching their targets");
#endif
    check(loadInstallation(fullData, restarted, error) && restarted.mode == SaveMode::Portable,
          "reinstall recognizes retained portable saves without an installation record");
    installed(fullData);
    put(fullData / "install.json", R"({"version":1,"save_mode":"portable"})");
    check(uninstallInstallation(fullData, false, error) && !fs::exists(fullFolder),
          "deleting portable saves removes the entire empty branded installation");
    const auto normalFolder = scratch / "normal-uninstall/DBZIW-Recomp";
    const auto otherFolder = scratch / "other-uninstall/DBZIW-Recomp";
    Installation normalCopy, otherCopy;
    installed(normalFolder / "dbziw-data");
    installed(otherFolder / "dbziw-data");
    check(loadInstallation(normalFolder / "dbziw-data", normalCopy, error) &&
              loadInstallation(otherFolder / "dbziw-data", otherCopy, error),
          "load both normal uninstall fixtures");
    configureSaveMode(normalCopy, SaveMode::Normal);
    configureSaveMode(otherCopy, SaveMode::Normal);
    check(prepareInstallation(normalCopy, error) && prepareInstallation(otherCopy, error),
          "record both normal installations");
    put(normalCopy.userData / "mc0/progress", "shared progress");
    put(normalCopy.configDir / "dbziw.ini", "shared settings");
    put(normalFolder / "unrelated.txt", "unrelated document");
    check(!uninstallInstallation(normalCopy.root, false, error) && fs::exists(normalCopy.root / "game/SLES_553.47"),
          "uninstall refuses to delete shared saves before removing any game files");
    check(uninstallInstallation(normalCopy.root, true, error) &&
              read(normalCopy.userData / "mc0/progress") == "shared progress" &&
              read(normalCopy.configDir / "dbziw.ini") == "shared settings" &&
              read(normalFolder / "unrelated.txt") == "unrelated document" &&
              fs::exists(otherCopy.root / "game/SLES_553.47"),
          "uninstall preserves shared saves and settings, unrelated files and the other install");
    env("LOCALAPPDATA", {});
    env("HOME", {});
    env("XDG_CONFIG_HOME", {});
    env("XDG_DATA_HOME", {});
    Installation noHome;
    check(loadInstallation(scratch / "without-home/dbziw-data", noHome, error),
          "first setup can offer portable mode without a user home");
    configureSaveMode(noHome, SaveMode::Portable);
    check(prepareInstallation(noHome, error) && noHome.userData == noHome.root / "user-data",
          "portable setup does not depend on machine-local user folders");
    env("LOCALAPPDATA", scratch / "home");
    env("HOME", scratch / "home");
    env("XDG_CONFIG_HOME", scratch / "config-home");
    env("XDG_DATA_HOME", scratch / "data-home");
    put(imported.root / "install.json", "{broken");
    check(!loadInstallation(imported.root, restarted, error),
          "malformed mode metadata cannot silently switch saves");
    std::cout << (failures ? "FAIL" : "PASS") << ": storage modes\n";
    return failures ? 1 : 0;
}
