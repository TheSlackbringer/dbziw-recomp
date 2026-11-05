#include "install_location.h"
#include "install_state.h"
#include "setup_cache.h"
#include "wizard_ui.h"

namespace dbziw::setup
{
namespace fs = std::filesystem;

bool wizardBuildIsCurrent(const SetupOptions &options, const fs::path &destination, const std::string &release)
{
    if (release.empty() || options.forceGenerate ||
        installState(destination, kSupportedDiscs[0]) != InstallState::Installed)
        return false;
    SetupOptions selected = options;
    selected.dataDir = destination;
    const auto build = selected.effectiveBuildDir();
#ifdef _WIN32
    const auto executable = build / "dbziw.exe";
#else
    const auto executable = build / "dbziw";
#endif
    std::error_code ec;
    if (!fs::is_regular_file(executable, ec))
        return false;
    std::string error;
    return buildReady(options.root, build, release, error);
}

bool prepareWizardSelection(WizardConfig &config, InstallFolderSelection &selection)
{
    if (!selection.error.empty())
        return false;
    if (selection.destination.empty())
    {
        selection.error = "Choose an installation folder.";
        return false;
    }
    if (!installPathFits(selection.destination, selection.error))
        return false;
    if (!prepareWizardInstallation(config, selection.destination, selection.error))
        return false;
    if (!(config.manageOnly && config.importSource.empty()))
        selection.frozen = true;
    return true;
}

bool prepareWizardInstallation(WizardConfig &config, const fs::path &destination, std::string &error)
{
    Installation installation;
    if (!loadInstallation(destination, installation, error))
        return false;
    if (installationGameRunning(destination, error))
        return false;
    bool modeSaved = false;
    if (config.modeChosen && config.desiredMode != installation.mode)
    {
        if (installation.recorded || installState(destination, kSupportedDiscs[0]) == InstallState::Installed)
        {
            if (!switchSaveMode(installation, config.desiredMode, config.conflict, error))
                return false;
            modeSaved = true;
        }
        else
            configureSaveMode(installation, config.desiredMode);
    }
    if (!config.importSource.empty())
    {
        if (!importInstallation(config.importSource, installation, config.conflict, error))
            return false;
    }
    else if (!modeSaved && !prepareInstallation(installation, error))
        return false;
    config.base.dataDir = destination;
    return true;
}
} // namespace dbziw::setup
