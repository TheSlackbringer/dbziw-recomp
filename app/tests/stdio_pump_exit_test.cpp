// Authored regression (DBZIW 011): text written just before the runtime's abrupt exits survives the stdio pump.
// The stall watchdog writes its report, drains the pump and calls _Exit(124); the crash signal handler cannot drain,
// so it writes its banner with writeDirect to stderr's original destination and calls _Exit(139). This test runs
// itself as a child with stdout and stderr redirected to one file (as the test runner starts the game), installs the
// pump like main(), does what each exit path does, and checks the file. No game code or data.
#include "runtime/ps2_stdio_pump.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    int child(const std::string &mode)
    {
        if (!ps2x::stdio_pump::installStdio())
            return 3;
        ps2x::stdio_pump::installLineStreams();
        if (mode == "drain")
        {
            std::cerr << "[WATCHDOG STALL] report line " << 1 << std::endl;
            std::fprintf(stderr, "[WATCHDOG STALL] last line\n");
            std::cout.flush();
            std::cerr.flush();
            ps2x::stdio_pump::drain();
            std::_Exit(124);
        }
        if (mode == "direct")
        {
            std::fprintf(stderr, "queued before the crash\n");
            const char banner[] = "[GUEST CRASH] banner\n";
            ps2x::stdio_pump::writeDirect(2, banner, sizeof(banner) - 1u);
            std::_Exit(139);
        }
        return 2;
    }

    std::string runChild(const char *self, const char *mode, const std::filesystem::path &log)
    {
        std::ostringstream cmd;
#if defined(_WIN32)
        cmd << "\"\"" << self << "\" " << mode << " > \"" << log.string() << "\" 2>&1\"";
#else
        cmd << "\"" << self << "\" " << mode << " > \"" << log.string() << "\" 2>&1";
#endif
        std::system(cmd.str().c_str());
        std::ifstream in(log, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        std::string out = text.str(); // Windows text-mode streams write \r\n
        for (size_t at; (at = out.find("\r\n")) != std::string::npos;)
            out.erase(at, 1);
        return out;
    }

    bool contains(const std::string &text, const char *what)
    {
        return text.find(what) != std::string::npos;
    }
}

int main(int argc, char **argv)
{
    if (argc == 2)
        return child(argv[1]);
    const auto dir = std::filesystem::temp_directory_path() /
                     ("dbziw-stdio-pump-exit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    int failures = 0;
    const std::string drained = runChild(argv[0], "drain", dir / "drain.log");
    if (!contains(drained, "[WATCHDOG STALL] report line 1\n") || !contains(drained, "[WATCHDOG STALL] last line\n"))
    {
        std::cerr << "FAIL: the report written before drain()+_Exit is missing:\n" << drained << '\n';
        ++failures;
    }
    const std::string direct = runChild(argv[0], "direct", dir / "direct.log");
    if (!contains(direct, "[GUEST CRASH] banner\n"))
    {
        std::cerr << "FAIL: the banner written with writeDirect before _Exit is missing:\n" << direct << '\n';
        ++failures;
    }
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    if (failures)
        return 1;
    std::cout << "stdio pump exit-path tests passed\n";
    return 0;
}
