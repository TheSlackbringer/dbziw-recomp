#pragma once
// The launcher's window and its no-window run: DBZIW's form of DW5-E's installer pages (007, on raylib + ImGui).
#include "installation.h"
#include "setup_pipeline.h"

#include <filesystem>
#include <string>
#include <vector>

namespace dbziw::setup {

std::string utf8(const std::filesystem::path &path);
std::filesystem::path fromUtf8(const std::string &text);

struct InstallFolderSelection;

struct WizardConfig
{
    SetupOptions base;
    std::filesystem::path launcherDir;
    std::string destinationError;
    bool explicitDestination = false;
    std::string layoutError;
    std::string releaseId;
    std::filesystem::path prefilledIso;
    bool discInstalled = false;
    bool closeWhenFinished = true;
    bool noWindow = false;
    bool manageOnly = false;
    bool launchExistingBuild = false;
    bool modeChosen = false;
    SaveMode desiredMode = SaveMode::Normal;
    SaveConflict conflict = SaveConflict::Ask;
    std::filesystem::path importSource;
    std::vector<std::filesystem::path> discovered;
};

bool prepareWizardInstallation(WizardConfig &config, const std::filesystem::path &destination, std::string &error);
bool prepareWizardSelection(WizardConfig &config, InstallFolderSelection &selection);
bool wizardBuildIsCurrent(const SetupOptions &options, const std::filesystem::path &destination, const std::string &release);

// Runs the launcher window until it closes; with no display, the console run instead. Returns true when the game can
// start or the requested management work finished. `config` holds the final destination afterwards.
bool runWizardWindow(WizardConfig &config);

// The run without a window: prints the steps to the console. Exit code 0 when it finished.
int runWizardConsole(const SetupOptions &options);

} // namespace dbziw::setup
