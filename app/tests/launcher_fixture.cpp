// Test-only replacement for the window and the slow setup steps (DW5-E's wizard_destination_fixture.cpp): the launcher's real main,
// storage and pipeline run; a stand-in picks the folder, and stand-in generate, lift and build steps need no disc.
#include "install_location.h"
#include "install_state.h"
#include "wizard_ui.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

namespace dbziw::setup {
namespace fs = std::filesystem;

namespace {
fs::path environmentPath(const char *name)
{
#ifdef _WIN32
    const wchar_t *value = _wgetenv(pathFromText(name).c_str());
    return value ? fs::path(value) : fs::path{};
#else
    const char *value = std::getenv(name);
    return value ? pathFromText(value) : fs::path{};
#endif
}

SetupSteps fixtureSteps(const fs::path &fakeGame)
{
    SetupSteps steps;
    steps.generate = [](const SetupOptions &, StepContext &, std::string &) { return true; };
    steps.liftVu1 = [](const SetupOptions &, StepContext &, std::string &) { return true; };
    steps.build = [fakeGame](const SetupOptions &options, StepContext &context, fs::path &executable, std::string &error) {
        const auto build = options.effectiveBuildDir();
        std::error_code ec;
        fs::create_directories(build, ec);
#ifdef _WIN32
        executable = build / "dbziw.exe";
#else
        executable = build / "dbziw";
#endif
        fs::copy_file(fakeGame, executable, fs::copy_options::overwrite_existing, ec);
        if (ec)
            error = "cannot copy the fixture game: " + ec.message();
        std::ofstream(build / "fixture-root.txt", std::ios::binary) << pathText(options.root);
        context.progress(1.0, "Compiling 1 of 1 (100%)");
        return !ec;
    };
    return steps;
}

int runFixture(const SetupOptions &options)
{
    SetupRun run(options, fixtureSteps(environmentPath("DBZIW_FIXTURE_GAME")));
    run.start();
    const bool finished = run.wait();
    if (!finished)
        std::cerr << "setup failed: " << run.status().error << '\n';
    return finished ? 0 : 1;
}
} // namespace

std::string utf8(const fs::path &path) { return pathText(path); }
fs::path fromUtf8(const std::string &text) { return pathFromText(text); }

bool runWizardWindow(WizardConfig &config)
{
    InstallFolderSelection selection{config.launcherDir, config.base.effectiveDataDir(), config.explicitDestination, false,
                                     config.destinationError};
    if (const auto folder = environmentPath("DBZIW_FIXTURE_GUI_FOLDER"); !folder.empty())
    {
        selection.error.clear();
        selection.choose(folder);
    }
    if (!config.layoutError.empty())
    {
        std::cerr << "Setup error: " << config.layoutError << '\n';
        return false;
    }
    // As the real no-window run: without a disc there is nothing to install, so leave the folder uncreated.
    if (!config.manageOnly && config.importSource.empty() && selection.error.empty() && config.base.iso.empty() &&
        installState(selection.destination, kSupportedDiscs[0]) != InstallState::Installed)
    {
        std::cerr << "No disc image specified.\n";
        return false;
    }
    if (!prepareWizardSelection(config, selection))
    {
        std::cerr << "Setup destination/save-location error: " << selection.error << '\n';
        return false;
    }
    if (config.manageOnly)
        return true;
    config.base.dataDir = selection.destination;
    return runFixture(config.base) == 0;
}

int runWizardConsole(const SetupOptions &options) { return runFixture(options); }

} // namespace dbziw::setup
