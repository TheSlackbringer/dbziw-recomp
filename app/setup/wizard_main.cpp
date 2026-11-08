// dbziw_setup: the launcher the player starts every time, and the developer's way to build from a disc image.
// In a release package (a BUILD_ID file): a current build starts the game at once; otherwise the installer pages (007, DW5-E's flow).
//   dbziw_setup [--iso <disc>] [--data-dir <folder>] [--install | --no-window] [--rebuild]
//   dbziw_setup --manage [--save-mode normal|portable] [--save-conflict source|destination]
//   dbziw_setup --use-installation <folder> | --import-installation <folder> | --status | --check-ready
#include "codegen.h"
#include "install_location.h"
#include "install_state.h"
#include "installation.h"
#include "run_process.h"
#include "setup_cache.h"
#include "setup_json.h"
#include "setup_layout.h"
#include "wizard_ui.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace dbziw::setup;
namespace fs = std::filesystem;

void attachParentConsole();

namespace {
#ifdef _WIN32
constexpr bool kWindows = true;
#else
constexpr bool kWindows = false;
#endif

struct Args
{
    fs::path iso, root, dataDir, existing, importSource;
    std::string error;
    bool help = false, noWindow = false, install = false, rebuild = false, checkReady = false, manage = false, status = false;
    bool modeChosen = false;
    SaveMode mode = SaveMode::Normal;
    SaveConflict conflict = SaveConflict::Ask;
};

std::string firstLine(const fs::path &path)
{
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        line.pop_back();
    return line;
}

Args parseArgs(const std::vector<std::string> &argv)
{
    Args a;
    for (size_t i = 1; i < argv.size(); ++i)
    {
        const std::string &arg = argv[i];
        if (arg == "--iso" || arg == "--disc" || arg == "--root" || arg == "--data-dir" || arg == "--use-installation" ||
            arg == "--import-installation")
        {
            if (i + 1 == argv.size() || argv[i + 1].empty() || argv[i + 1].rfind("--", 0) == 0)
            {
                a.error = "Missing path after " + arg;
                break;
            }
            const fs::path path = pathFromText(argv[++i]);
            if (arg == "--root")
                a.root = path;
            else if (arg == "--data-dir")
                a.dataDir = path;
            else if (arg == "--use-installation")
                a.existing = path;
            else if (arg == "--import-installation")
                a.importSource = path;
            else
                a.iso = path;
        }
        else if (arg == "--save-mode" || arg == "--save-conflict")
        {
            if (i + 1 == argv.size())
            {
                a.error = "Missing value after " + arg;
                break;
            }
            const std::string value = argv[++i];
            if (arg == "--save-mode")
            {
                a.modeChosen = true;
                if (value == "normal")
                    a.mode = SaveMode::Normal;
                else if (value == "portable")
                    a.mode = SaveMode::Portable;
                else
                {
                    a.error = "Save mode must be normal or portable";
                    break;
                }
            }
            else if (value == "source")
                a.conflict = SaveConflict::UseSource;
            else if (value == "destination")
                a.conflict = SaveConflict::KeepDestination;
            else if (value != "ask")
            {
                a.error = "Save conflict choice must be ask, source or destination";
                break;
            }
        }
        else if (arg == "--manage")
            a.manage = true;
        else if (arg == "--status")
            a.status = true;
        else if (arg == "--no-window")
            a.noWindow = true;
        else if (arg == "--install")
            a.install = a.noWindow = true;
        else if (arg == "--check-ready")
            a.checkReady = true;
        else if (arg == "--rebuild")
            a.rebuild = true;
        else if (arg == "--help" || arg == "-h")
            a.help = true;
        else
        {
            a.error = "Unknown setup argument: " + arg;
            break;
        }
    }
    return a;
}

void setEnvironment(const char *name, const fs::path &value)
{
#ifdef _WIN32
    _wputenv_s(fs::path(name).c_str(), value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}
} // namespace

int main(int argc, char **argv)
{
    const Args args = parseArgs(nativeArguments(argc, argv));
    if (args.noWindow || args.help || args.status || args.checkReady || !args.error.empty())
        attachParentConsole();
    if (args.help)
    {
        std::printf("DBZIW setup: [--iso <disc>] [--data-dir <folder>] [--install | --no-window] [--rebuild]\n"
                    "The launcher, README and dbziw-data folder live together in one DBZIW-Recomp folder. Saves and settings: this PC or portable.\n"
                    "Overrides: --data-dir, DBZIW_DATA_DIR (one folder for everything, not saved), remembered choice, launcher folder.\n"
                    "Manage: --manage [--save-mode normal|portable] [--save-conflict source|destination]\n"
                    "Existing files: --use-installation <folder> or --import-installation <folder>. Inspect with --status, --check-ready.\n");
        return 0;
    }
    if (!args.error.empty())
    {
        std::fprintf(stderr, "%s\n", args.error.c_str());
        return 1;
    }

    std::error_code ec;
    fs::path root = args.root.empty() ? fs::path{} : fs::absolute(args.root, ec).lexically_normal();
    if (root.empty())
    {
        std::string discoveryError;
        root = findRoot(executableDirectory(discoveryError));
    }
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
    fs::path game, data, launcher;
    bool explicitDestination = false;
    std::string destinationError;
    Installation storage;
    std::vector<fs::path> discovered;
    fs::path importSource;

    if (!release.empty())
    {
        launcher = launcherDirectory(destinationError);
        std::string resolveError;
        if (!resolveInstallLocation(launcher, args.dataDir, data, explicitDestination, resolveError))
            destinationError = resolveError;
        else if (!args.existing.empty())
        {
            data = existingInstallationRoot(args.existing);
            if (data.empty())
                destinationError = "Selected folder is not a verified DBZIW installation: " + pathText(args.existing);
            else
                destinationError.clear();
        }
        else if (args.importSource.empty() && !explicitDestination &&
                 installState(data, kSupportedDiscs[0]) != InstallState::Installed && !fs::is_regular_file(data / "install.json", ec))
        {
            fs::path preferred;
            if (!discoverInstallations(launcher, discovered, resolveError, &preferred))
                destinationError = resolveError;
            else if (!preferred.empty())
            {
                data = preferred;
                destinationError.clear();
            }
            else if (discovered.size() == 1)
            {
                data = discovered.front();
                destinationError.clear();
            }
            else if (discovered.size() > 1)
                destinationError = "Several existing DBZIW installations were found. Choose one below.";
        }
        if (destinationError.empty() && !args.importSource.empty())
        {
            importSource = existingInstallationRoot(args.importSource);
            if (importSource.empty())
                destinationError = "Cannot import an unverified DBZIW installation: " + pathText(args.importSource);
            else if (!explicitDestination)
                data = launcher / "dbziw-data";
        }
        if (destinationError.empty() && !loadInstallation(data, storage, resolveError))
            destinationError = resolveError;
        if (destinationError.empty() && !args.status && !checkInstallDirectory(data, resolveError))
            destinationError = resolveError;
        options.dataDir = data;
        options.releaseId = release;
        options.usePersistentBuilder = true;
        options.reuseGenerated = true;
        options.forceGenerate = args.rebuild;
        game = options.effectiveBuildDir() / (kWindows ? "dbziw.exe" : "dbziw");
    }
    const auto installed = [&] { return !data.empty() && installState(data, kSupportedDiscs[0]) == InstallState::Installed; };

    if (args.status)
    {
        if (!destinationError.empty())
        {
            std::fprintf(stderr, "%s\n", destinationError.c_str());
            return 1;
        }
        std::printf("{\n  \"installation\": %s,\n  \"save_mode\": %s,\n  \"user_data\": %s,\n  \"config\": %s,\n  \"recorded\": %s\n}\n",
                    json::quote(pathText(data)).c_str(), json::quote(saveModeName(storage.mode)).c_str(),
                    json::quote(pathText(storage.userData)).c_str(), json::quote(pathText(storage.configDir)).c_str(),
                    storage.recorded ? "true" : "false");
        return 0;
    }
    const bool managing = args.manage || (args.modeChosen && installed());
    const auto startGame = [&] {
        InstallationLock lock;
        if (!lock.acquire(data, destinationError))
        {
            std::fprintf(stderr, "%s\n", destinationError.c_str());
            return 1;
        }
        if (!validateInstallDirectory(data, destinationError) || !loadInstallation(data, storage, destinationError) ||
            (!explicitDestination && (!validateInstallDirectory(storage.userData / "mc0", destinationError) ||
                                      !validateInstallDirectory(storage.userData / "mc1", destinationError) ||
                                      !validateInstallDirectory(storage.configDir, destinationError))))
        {
            std::fprintf(stderr, "DBZIW launcher error: %s\n", destinationError.c_str());
            return 1;
        }
        fs::create_directories(data / "logs", ec);
        if (ec)
        {
            std::fprintf(stderr, "Cannot create game logs at %s\n", pathText(data / "logs").c_str());
            return 1;
        }
        if (!explicitDestination && !rememberNormalInstallation(storage, destinationError, true))
        {
            std::fprintf(stderr, "%s\n", destinationError.c_str());
            return 1;
        }
        // An explicit, unrecorded folder holds everything for that run, saves and settings too; nothing is written to the profile.
        if (explicitDestination && !storage.recorded)
        {
            configureSaveMode(storage, SaveMode::Portable);
            if (!validateInstallDirectory(storage.userData / "mc0", destinationError) ||
                !validateInstallDirectory(storage.userData / "mc1", destinationError))
            {
                std::fprintf(stderr, "DBZIW launcher error: %s\n", destinationError.c_str());
                return 1;
            }
        }
        // Where the game keeps saves and settings: the user's folder, or with the game when portable.
        setEnvironment("PS2X_MC_ROOT", storage.userData / "mc0");
        setEnvironment("PS2X_SETTINGS_FILE", storage.configDir / "dbziw.ini");
        // DBZIW 019: players start at the game's own 60 Hz unless their settings file says video_mode=50 (no file is written).
        setEnvironment("PS2X_VIDEO_MODE_DEFAULT", "60");
        game = options.effectiveBuildDir() / (kWindows ? "dbziw.exe" : "dbziw");
        const fs::path gameFolder = discFolder(data, kSupportedDiscs[0]);
        const std::vector<std::string> gameArgs{"./SLES_553.47"};
#ifndef _WIN32
        // Keep the AppImage mount alive while the game uses its bundled libraries.
        const ProcessResult result = runProcess(game, gameArgs, gameFolder, data / "logs" / "game.log");
        if (result.started)
            return result.exitCode;
        std::fprintf(stderr, "DBZIW launcher error: %s\n", result.error.c_str());
        return 1;
#else
        std::string error;
        if (!launchDetached(game, gameArgs, gameFolder, error, data / "logs" / "game.log"))
        {
            std::fprintf(stderr, "DBZIW launcher error: %s\n", error.c_str());
            return 1;
        }
        return 0;
#endif
    };

    const bool ready = layoutError.empty() && destinationError.empty() && !args.install && !managing && !args.modeChosen && importSource.empty() &&
                       wizardBuildIsCurrent(options, data, release);
    if (args.checkReady)
    {
        std::printf("%s\n", ready ? "ready" : "setup required");
        return ready ? 0 : 3;
    }
    if (ready)
    {
        if (args.noWindow)
        {
            std::printf("SUCCESS: Game already built and ready at %s\n", pathText(game).c_str());
            return 0;
        }
        return startGame();
    }
    options.iso = args.iso;

    if (release.empty())
    {
        // The developer's tree: everything stays in the project.
        if (args.noWindow && !layoutError.empty())
        {
            std::fprintf(stderr, "dbziw_setup: %s\n", layoutError.c_str());
            return 2;
        }
        if (args.noWindow && args.iso.empty())
        {
            std::fprintf(stderr, "usage: dbziw_setup --iso <disc image> [--root <dir>] --no-window\n");
            return 2;
        }
        if (args.noWindow)
            return runWizardConsole(options);
    }

    WizardConfig config;
    config.base = options;
    config.launcherDir = launcher;
    config.destinationError = destinationError;
    config.explicitDestination = explicitDestination;
    config.layoutError = layoutError;
    config.releaseId = release;
    config.prefilledIso = args.iso;
    config.discInstalled = installed();
    config.noWindow = args.noWindow;
    config.manageOnly = managing;
    config.modeChosen = args.modeChosen;
    config.desiredMode = args.mode;
    config.conflict = args.conflict;
    config.importSource = importSource;
    config.discovered = discovered;
    config.closeWhenFinished = !release.empty();
    // An explicit folder holds everything for that run, saves and settings too.
    if (explicitDestination && !storage.recorded && !args.modeChosen)
    {
        config.modeChosen = true;
        config.desiredMode = SaveMode::Portable;
    }

    if (!runWizardWindow(config))
        return 1;
    if (config.manageOnly || release.empty())
        return 0;
    options = config.base;
    data = options.effectiveDataDir();
    if (!loadInstallation(data, storage, destinationError) ||
        (args.noWindow && !rememberNormalInstallation(storage, destinationError, true)))
    {
        std::fprintf(stderr, "%s\n", destinationError.c_str());
        return 1;
    }
    if (args.noWindow)
        return 0;
    game = options.effectiveBuildDir() / (kWindows ? "dbziw.exe" : "dbziw");
    return fs::exists(game, ec) ? startGame() : 0;
}
