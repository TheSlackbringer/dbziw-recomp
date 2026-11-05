// Disc-free stand-in for the game (DW5-E's install_fake_game.cpp). Never linked into the game or the launcher.
#include "install_location.h"
#include "setup_json.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
using namespace dbziw::setup;
namespace fs = std::filesystem;

static void write(const fs::path &p, const std::string &s)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}

static fs::path environmentPath(const char *name)
{
#ifdef _WIN32
    const auto key = pathFromText(name).wstring();
    const wchar_t *value = _wgetenv(key.c_str());
    return value ? fs::path(value) : fs::path{};
#else
    const char *value = std::getenv(name);
    return value ? pathFromText(value) : fs::path{};
#endif
}

static std::string report(const std::vector<std::string> &args, const char *extra = "")
{
    std::string list;
    for (const auto &arg : args)
        list += (list.empty() ? "" : ",") + json::quote(arg);
    return "{\"args\":[" + list + "],\"cwd\":" + json::quote(pathText(fs::current_path())) +
           ",\"mc_root\":" + json::quote(pathText(environmentPath("PS2X_MC_ROOT"))) +
           ",\"settings\":" + json::quote(pathText(environmentPath("PS2X_SETTINGS_FILE"))) + extra + "}";
}

int main(int argc, char **argv)
{
    const auto args = nativeArguments(argc, argv);
    const auto reports = environmentPath("DBZIW_FIXTURE_REPORT");
    const std::string mode = args.size() > 1 ? args[1] : "game";
    if (mode == "hold")
    {
        std::cout << "ready" << std::endl;
        return std::cin.get() == EOF ? 5 : 0;
    }
    if (mode == "bootstrap-probe")
    {
        write(reports / "bootstrap.json", report(args));
        const char *exit = std::getenv("DBZIW_FIXTURE_EXIT");
        return exit ? std::atoi(exit) : 0;
    }
#ifdef _WIN32
    const std::string pid = ",\"pid\":" + std::to_string(GetCurrentProcessId());
#else
    const std::string pid;
#endif
    std::cout << "fixture game output" << std::endl;
    if (!reports.empty())
        write(reports / "game.json", report(args, pid.c_str()));
#ifdef _WIN32
    const auto eventName = environmentPath("DBZIW_FIXTURE_EVENT").wstring();
    if (!eventName.empty())
    {
        const auto event = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
        if (!event)
            return 3;
        SetEvent(event);
        CloseHandle(event);
        const auto acknowledgement = environmentPath("DBZIW_FIXTURE_ACK_EVENT").wstring();
        const auto ack = OpenEventW(SYNCHRONIZE, FALSE, acknowledgement.c_str());
        if (!ack)
            return 4;
        const auto completed = WaitForSingleObject(ack, 10000);
        CloseHandle(ack);
        if (completed != WAIT_OBJECT_0)
            return 5;
    }
#endif
    return 0;
}
