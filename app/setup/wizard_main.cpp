// dbziw_setup — builds the game from your own retail disc.
#include "codegen.h"
#include "install_state.h"
#include "run_process.h"
#include "setup_layout.h"
#include "wizard_ui.h"

#include <cstdio>
#include <fstream>
#include <string>

void attachParentConsole();

using namespace dbziw::setup;
namespace fs = std::filesystem;

namespace {
#ifdef _WIN32
constexpr bool kWindows = true;
#else
constexpr bool kWindows = false;
#endif

struct Args
{
    fs::path iso, root;
    bool noWindow = false;
    bool rebuild = false;
    bool help = false;
};

std::string firstLine(const fs::path &path)
{
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    return line;
}

Args parseArgs(int argc, char **argv)
{
    Args a;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--iso" && hasValue)
            a.iso = fromUtf8(argv[++i]);
        else if (arg == "--root" && hasValue)
            a.root = fromUtf8(argv[++i]);
        else if (arg == "--no-window")
            a.noWindow = true;
        else if (arg == "--rebuild")
            a.rebuild = true;
        else if (arg == "--help" || arg == "-h")
            a.help = true;
    }
    return a;
}
} // namespace

int main(int argc, char **argv)
{
    const Args args = parseArgs(argc, argv);
    if (args.noWindow)
        attachParentConsole();

    if (args.help)
    {
        std::printf("usage: dbziw_setup --iso <DBZIW.iso> [--root <dir>] --no-window\n");
        return 0;
    }

    std::error_code ec;
    fs::path root = args.root;
    if (root.empty())
        root = findRoot(argc > 0 ? fs::absolute(fromUtf8(argv[0]), ec).parent_path() : fs::current_path(ec));
    if (root.empty())
        root = findRoot(fs::current_path(ec));

    SetupOptions options;
    std::string layoutError;
    SetupLayout layout;
    if (root.empty())
        layoutError = "cannot find the project folder (app/ and recomp/) next to this program";
    else if (findLayout(root, kWindows, layout, layoutError))
    {
        options.recompiler = layout.recompiler;
        options.tools = layout.tools;
    }
    options.root = root;

    const std::string release = root.empty() ? std::string() : firstLine(root / "BUILD_ID");
    fs::path game, stamp, data;
    if (!release.empty())
    {
        data = playerDataDir();
        if (data.empty())
            layoutError = "no player data folder (LOCALAPPDATA / HOME not set)";
        options.dataDir = data;
        game = options.effectiveBuildDir() / (kWindows ? "dbziw.exe" : "dbziw");
        stamp = options.effectiveBuildDir() / "built-from.txt";
    }
    const auto installed = [&] { return installState(data, kSupportedDiscs[0]) == InstallState::Installed; };
    const auto startGame = [&] {
        std::string error;
        if (!launchDetached(game, {}, data, error))
        {
            std::fprintf(stderr, "dbziw_setup: %s\n", error.c_str());
            return 1;
        }
        return 0;
    };
    const auto markBuilt = [&] { std::ofstream(stamp) << release << "\n"; };
    if (!release.empty() && layoutError.empty() && !args.rebuild && !args.noWindow && fs::exists(game) &&
        firstLine(stamp) == release && installed())
        return startGame();

    if (args.noWindow)
    {
        if (!layoutError.empty())
        {
            std::fprintf(stderr, "dbziw_setup: %s\n", layoutError.c_str());
            return 2;
        }
        if (args.iso.empty() && !(!release.empty() && installed()))
        {
            std::fprintf(stderr, "usage: dbziw_setup --iso <DBZIW.iso> [--root <dir>] --no-window\n");
            return 2;
        }
        options.iso = args.iso;
        const int rc = runWizardConsole(options);
        if (rc == 0 && !release.empty())
            markBuilt();
        return rc;
    }
    WizardConfig config{options, layoutError, args.iso};
    if (!release.empty())
    {
        config.discInstalled = installed();
        config.closeWhenFinished = true;
    }
    if (runWizardWindow(config) && !release.empty())
    {
        markBuilt();
        return startGame();
    }
    return 0;
}
