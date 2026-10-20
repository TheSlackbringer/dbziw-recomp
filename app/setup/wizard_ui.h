#pragma once
// The setup wizard's window and its no-window run, shared by dbziw_setup
// and the game itself on first launch.
#include "setup_pipeline.h"

#include <filesystem>
#include <memory>
#include <string>

namespace dbziw::setup {

std::string utf8(const std::filesystem::path &path);
std::filesystem::path fromUtf8(const std::string &text);

struct WizardConfig
{
    SetupOptions base;       // base.installOnly: the player install (check + extract)
    std::string layoutError; // shown instead of allowing a start
    std::filesystem::path iso; // prefilled field
    bool discInstalled = false;     // files are already in the data folder
    bool closeWhenFinished = false; // release launcher: close window when done
};

// Runs the wizard in its own window until the player closes it.
bool runWizardWindow(const WizardConfig &config);

// The same run without a window, printing the steps; returns 0 when it finished.
int runWizardConsole(const SetupOptions &options);

// "Insert disc" panel drawn inside an imgui frame
class DiscInstallPanel
{
public:
    DiscInstallPanel(std::filesystem::path dataDir, const DiscSpec &disc);
    ~DiscInstallPanel();
    bool draw();
    void dropped(const std::filesystem::path &image);
    bool running() const;

private:
    struct State;
    std::unique_ptr<State> m;
};

} // namespace dbziw::setup
