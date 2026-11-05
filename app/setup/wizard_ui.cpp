// DBZIW's launcher pages: DW5-E's installer (app/setup/wizard_ui.cpp at e4a7028c) on raylib + ImGui, with its layout, colours and wording.
// No <windows.h> here: it clashes with raylib.h (file_dialog.cpp and wizard_console.cpp hold the Windows parts).
#include "wizard_ui.h"
#include "disc_setup.h"
#include "file_dialog.h"
#include "install_location.h"
#include "install_state.h"
#include "uninstall_process.h"

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

namespace dbziw::setup {
namespace fs = std::filesystem;

std::string utf8(const fs::path &path)
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

fs::path fromUtf8(const std::string &text) { return fs::path(std::u8string(text.begin(), text.end())); }

namespace {
bool discImageValid(const fs::path &image, std::string &message)
{
    IsoImage iso;
    std::string error;
    if (!iso.open(image, error))
        return message = "This is not a disc image: " + error, false;
    const DiscSpec one[] = {kSupportedDiscs[0]};
    if (!identifyDisc(iso, one, error))
        return message = "This is not the supported PAL disc (SLES-55347).", false;
    return true;
}

// Work that must not hold up the window: preparing an installation, or scheduling its removal.
struct Preparation
{
    WizardConfig config;
    InstallFolderSelection selection;
    std::atomic<bool> done{false};
    bool ok = false;
    fs::path uninstallLog;
    std::thread worker;
    Preparation(const WizardConfig &source, const InstallFolderSelection &folder, bool uninstall, bool keepSaves)
        : config(source), selection(folder)
    {
        worker = std::thread([this, uninstall, keepSaves] {
            try
            {
                ok = uninstall ? scheduleUninstall(config.base.root, selection.destination, keepSaves, uninstallLog, selection.error)
                               : prepareWizardSelection(config, selection);
            }
            catch (const std::exception &error)
            {
                selection.error = error.what();
            }
            done.store(true, std::memory_order_release);
        });
    }
    ~Preparation()
    {
        if (worker.joinable())
            worker.join();
    }
};

fs::path visibleInstallFolder(const fs::path &path)
{
    return path.filename() == "dbziw-data" && path.parent_path().filename() == "DBZIW-Recomp" ? path.parent_path() : path;
}

const ImVec4 kErrorText(0.95f, 0.30f, 0.30f, 1.0f);
const ImVec4 kValidText(0.25f, 0.85f, 0.45f, 1.0f);
constexpr float kButtonHeight = 36.0f;

void applyStyle()
{
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.WindowPadding = ImVec2(20, 14);
    style.ItemSpacing = ImVec2(9, 9);
    style.FramePadding = ImVec2(12, 7);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.32f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.19f, 0.22f, 0.27f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.30f, 0.37f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.25f, 0.65f, 0.45f, 1.0f);
}

// Roboto Medium from the package's ImGui, or ImGui's own font when it cannot be read.
void loadFont(const fs::path &root)
{
    ImGuiIO &io = ImGui::GetIO();
    std::vector<fs::path> candidates{root / "app/deps/imgui-src/misc/fonts/Roboto-Medium.ttf"};
#ifdef DBZIW_IMGUI_FONT
    candidates.push_back(fromUtf8(DBZIW_IMGUI_FONT));
#endif
    for (const auto &path : candidates)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const auto size = file.tellg();
        if (size <= 0 || size > 1024 * 1024)
            continue;
        const auto bytes = static_cast<int>(size);
        void *data = ImGui::MemAlloc(size_t(bytes));
        file.seekg(0);
        if (data && file.read(static_cast<char *>(data), bytes))
        {
            io.FontDefault = io.Fonts->AddFontFromMemoryTTF(data, bytes, 16.0f);
            return;
        }
        ImGui::MemFree(data);
    }
}
} // namespace

int runWizardConsole(const SetupOptions &options)
{
    SetupRun run(options);
    run.start();
    Step shown = Step::Done;
    int shownTenth = -1;
    for (;;)
    {
        const SetupStatus st = run.status();
        const int steps = options.installOnly ? int(Step::Extract) + 1 : kStepCount;
        if (st.step != shown && st.step != Step::Done)
        {
            shown = st.step;
            shownTenth = -1;
            std::printf("[%d/%d] %s\n", int(st.step) + 1, steps, stepName(st.step));
        }
        const int tenth = int(st.overall * 20.0);
        if (tenth != shownTenth && st.step != Step::Done)
        {
            shownTenth = tenth;
            std::printf("      %3d %%  %s\n", tenth * 5, st.detail.c_str());
        }
        if (!st.running)
        {
            if (st.finished && options.installOnly)
                std::printf("installed in %s\n", utf8(options.root).c_str());
            else if (st.finished)
                std::printf("done: %s\n", utf8(st.executable).c_str());
            else if (st.cancelled)
                std::printf("cancelled\n");
            else
                std::printf("failed: %s\n(logs: %s)\n", st.error.c_str(), utf8(options.effectiveLogDir()).c_str());
            std::fflush(stdout);
            return st.finished ? 0 : 1;
        }
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

bool runWizardWindow(WizardConfig &config)
{
    InstallFolderSelection destination{config.launcherDir, config.base.effectiveDataDir(), config.explicitDestination, false,
                                       config.destinationError};
    const auto console = [&] {
        if (!config.layoutError.empty())
        {
            std::fprintf(stderr, "Setup error: %s\n", config.layoutError.c_str());
            return false;
        }
        // Without a disc there is nothing to install, so leave the folder uncreated.
        const bool hasFiles = installState(destination.destination, kSupportedDiscs[0]) == InstallState::Installed;
        if (!config.manageOnly && config.importSource.empty() && destination.error.empty() && config.base.iso.empty() && !hasFiles)
        {
            std::fprintf(stderr, "No disc image specified. To run in console mode: pass --iso <disc image>.\n");
            return false;
        }
        if (!prepareWizardSelection(config, destination))
        {
            std::fprintf(stderr, "Setup destination/save-location error: %s\n", destination.error.c_str());
            return false;
        }
        if (config.manageOnly)
        {
            std::printf("SUCCESS: save location updated without rebuilding the game.\n");
            return true;
        }
        return runWizardConsole(config.base) == 0;
    };
    if (config.noWindow)
        return console();

    // DBZIW-DEBUG: DBZIW_WIZARD_SHOT=<png> saves one frame of a hidden window and closes; DBZIW_WIZARD_PAGE opens a page directly (007's layout check).
    const char *shot = std::getenv("DBZIW_WIZARD_SHOT");
    const std::string testPage = [] { const char *v = std::getenv("DBZIW_WIZARD_PAGE"); return std::string(v ? v : ""); }();
    int frames = 0;

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | (shot && *shot ? FLAG_WINDOW_HIDDEN : 0));
    InitWindow(680, 440, "Dragon Ball Z: Infinite World - Setup");
    if (!IsWindowReady())
    {
        std::fprintf(stderr, "No display available, falling back to console setup.\n");
        return console();
    }
    SetWindowMinSize(360, 280);
    SetExitKey(KEY_NULL); // Esc must not close the installer
    SetTargetFPS(60);
    rlImGuiBeginInitImGui();
    ImGui::StyleColorsDark();
    loadFont(config.base.root);
    rlImGuiEndInitImGui();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // no window layout outside the installation
    applyStyle();

    char isoPath[4096] = {};
    if (!config.prefilledIso.empty())
        std::snprintf(isoPath, sizeof(isoPath), "%s", utf8(config.prefilledIso).c_str());

    char folderInput[4096] = {};
    std::string folderError, checkedFolder;
    fs::path folderChoice;
    bool createFolder = false, folderHasInstallation = false;
    enum class Page { Saves, Disc, Existing, Other, Manage, Uninstall, Uninstalled, Copied, Saved, PreparationFailed };
    const auto hasDisc = [&] { return installState(destination.destination, kSupportedDiscs[0]) == InstallState::Installed; };
    Page page = config.manageOnly                                         ? Page::Manage
                : !config.importSource.empty()                            ? Page::Saves
                : hasDisc()                                               ? Page::Existing
                : !config.discovered.empty() || !destination.error.empty() ? Page::Other
                                                                          : Page::Disc;
    Page returnFromOptions = page;
    bool choosingInstallDestination = false;
    const auto openOptions = [&](bool installDestination = false) {
        folderError.clear();
        checkedFolder.clear();
        folderChoice.clear();
        createFolder = false;
        folderHasInstallation = false;
        choosingInstallDestination = installDestination;
        auto path = page == Page::Disc && !installDestination
                        ? std::string{}
                        : utf8(installDestination ? visibleInstallFolder(destination.destination) : destination.destination);
        if (installDestination && !destination.explicitOverride)
        {
            path = utf8(suggestedInstallFolder());
        }
        std::snprintf(folderInput, sizeof(folderInput), "%s", path.c_str());
        returnFromOptions = page;
        page = Page::Other;
    };
    std::string actionError;
    fs::path preparationLog;
    Page pageBeforeFailure = page;
    std::string saveConfirmation;
    bool returnToExisting = false, copyFromManagement = false;
#ifdef _WIN32
    const char *copyLabel = "Copy installation beside this launcher";
#else
    const char *copyLabel = "Copy installation beside this AppImage";
#endif
    const auto selectDestination = [&](const fs::path &path) {
        if (!destination.choose(path))
            return false;
        actionError.clear();
        saveConfirmation.clear();
        return true;
    };
    std::unique_ptr<SetupRun> activeRun;
    std::unique_ptr<Preparation> preparation;
    bool closeAfterPreparation = false, keepUninstallSaves = true;
    fs::path uninstallLog;
    const auto beginPreparation = [&](bool uninstall = false) {
        preparation = std::make_unique<Preparation>(config, destination, uninstall, keepUninstallSaves);
        destination.frozen = true;
    };
    bool quit = false, succeeded = false;

    std::string validatedIso = "__uninit__", isoError;
    bool isoValid = false;

    if (testPage == "folder")
        openOptions(true);
    else if (testPage == "installed")
        openOptions(false);
    else if (testPage == "uninstall")
        page = Page::Uninstall;
    else if (testPage == "saves")
        page = Page::Saves;

    while (!quit)
    {
        if (WindowShouldClose())
        {
            if (activeRun)
                activeRun->cancel();
            if (preparation)
                closeAfterPreparation = true;
            else
                quit = true;
        }
        if (IsFileDropped())
        {
            FilePathList files = LoadDroppedFiles();
            if (files.count > 0 && !activeRun && !preparation && page == Page::Disc)
                std::snprintf(isoPath, sizeof(isoPath), "%s", files.paths[0]);
            UnloadDroppedFiles(files);
        }

        if (preparation && preparation->done.load(std::memory_order_acquire))
        {
            const bool ok = preparation->ok;
            config = std::move(preparation->config);
            destination = std::move(preparation->selection);
            uninstallLog = std::move(preparation->uninstallLog);
            preparation.reset();
            if (ok && page == Page::Uninstall)
            {
                config.manageOnly = true;
                succeeded = true;
                page = Page::Uninstalled;
                quit = closeAfterPreparation;
            }
            else if (closeAfterPreparation)
                quit = true;
            else if (!ok)
            {
                actionError = std::move(destination.error);
                destination.error.clear();
                if (actionError.empty())
                    actionError = "Setup could not complete this action.";
                std::fprintf(stderr, "Setup: %s\n", actionError.c_str());
                const auto root = config.importSource.empty() ? destination.destination : config.importSource;
                preparationLog.clear();
                if (!root.empty())
                {
                    const auto logPath = root / "logs" / "installation-management.log";
                    std::ofstream log(logPath, std::ios::app);
                    if (log << actionError << '\n' << std::flush)
                        preparationLog = logPath;
                }
                pageBeforeFailure = page;
                page = Page::PreparationFailed;
            }
            else if (config.manageOnly)
            {
                if (copyFromManagement)
                {
                    succeeded = true;
                    page = Page::Copied;
                }
                else
                {
                    saveConfirmation = config.desiredMode == SaveMode::Portable
                                           ? "Save location updated. Saves and settings now stay with the game."
                                           : "Save location updated. Saves and settings now use your user profile.";
                    config.modeChosen = false;
                    config.conflict = SaveConflict::Ask;
                    actionError.clear();
                    if (returnToExisting)
                    {
                        config.manageOnly = false;
                        returnToExisting = false;
                        page = Page::Existing;
                    }
                    else
                    {
                        succeeded = true;
                        page = Page::Saved;
                    }
                }
            }
            else if (config.importSource.empty() && config.base.iso.empty() &&
                     wizardBuildIsCurrent(config.base, destination.destination, config.releaseId))
            {
                config.launchExistingBuild = true;
                succeeded = true;
                quit = true;
            }
            else
            {
                config.importSource.clear();
                actionError.clear();
                activeRun = std::make_unique<SetupRun>(config.base);
                activeRun->start();
            }
        }
        destination.frozen = bool(activeRun) || bool(preparation);

        BeginDrawing();
        ClearBackground(Color{25, 28, 33, 255});
        rlImGuiBegin();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(float(GetScreenWidth()), float(GetScreenHeight())));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("SetupWindow", nullptr, flags))
        {
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            const float footerHeight = kButtonHeight + 2 * ImGui::GetStyle().ItemSpacing.y + 1;
            const float bodyHeight = ImGui::GetContentRegionAvail().y - footerHeight;
            const bool preparing = bool(preparation);
            ImGui::PushID(activeRun ? -2 : static_cast<int>(page));
            ImGui::BeginChild("PageBody", ImVec2(0, bodyHeight));
            if (preparing && closeAfterPreparation)
                ImGui::TextWrapped("Closing when the current operation finishes.");
            ImGui::BeginDisabled(preparing);

            struct FooterResult { bool back = false; bool primary = false; };
            bool footerDrawn = false;
            const auto footer = [&](const char *back = nullptr, const char *primary = nullptr, bool primaryEnabled = true,
                                    bool destructive = false) {
                ImGui::EndDisabled();
                ImGui::EndChild();
                footerDrawn = true;
                ImGui::Separator();
                FooterResult result;
                const float available = ImGui::GetContentRegionAvail().x;
                const bool paired = back && primary;
                const float width = std::min(180.0f, paired ? (available - gap) / 2 : available);
                const float total = paired ? 2 * width + gap : width;
                if (total > 0 && available > total)
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - total);
                if (back)
                {
                    ImGui::BeginDisabled(preparing);
                    result.back = ImGui::Button(back, ImVec2(width, kButtonHeight));
                    ImGui::EndDisabled();
                }
                if (primary)
                {
                    if (back)
                        ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, destructive ? ImVec4(0.62f, 0.21f, 0.24f, 1.0f) : ImVec4(0.22f, 0.43f, 0.66f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, destructive ? ImVec4(0.74f, 0.26f, 0.30f, 1.0f) : ImVec4(0.28f, 0.52f, 0.76f, 1.0f));
                    ImGui::BeginDisabled(preparing || !primaryEnabled);
                    result.primary = ImGui::Button(primary, ImVec2(width, kButtonHeight));
                    ImGui::EndDisabled();
                    ImGui::PopStyleColor(2);
                }
                return result;
            };
            const auto showError = [](const std::string &error) {
                if (error.empty())
                    return;
                ImGui::PushStyleColor(ImGuiCol_Text, kErrorText);
                ImGui::TextWrapped("%s", error.c_str());
                ImGui::PopStyleColor();
            };
            const auto chooseMode = [&](SaveMode initial, const fs::path &previewRoot = {}) {
                const SaveMode desired = config.modeChosen ? config.desiredMode : initial;
                if (ImGui::RadioButton("On this PC (recommended)", desired == SaveMode::Normal))
                {
                    config.desiredMode = SaveMode::Normal;
                    config.modeChosen = true;
                    actionError.clear();
                    config.conflict = SaveConflict::Ask;
                }
                if (ImGui::RadioButton("With the game (portable)", desired == SaveMode::Portable))
                {
                    config.desiredMode = SaveMode::Portable;
                    config.modeChosen = true;
                    actionError.clear();
                    config.conflict = SaveConflict::Ask;
                }
                Installation preview;
                preview.root = previewRoot.empty() ? destination.destination : previewRoot;
                configureSaveMode(preview, config.modeChosen ? config.desiredMode : initial);
                ImGui::TextWrapped("Saves: %s", utf8(preview.userData).c_str());
                if (preview.configDir != preview.userData)
                    ImGui::TextWrapped("Settings: %s", utf8(preview.configDir).c_str());
            };
            const auto chooseConflict = [&] {
                if (actionError.find("conflict") == std::string::npos && config.conflict == SaveConflict::Ask)
                    return;
                ImGui::TextWrapped("Both locations contain progress. Choose which files to use.");
                if (ImGui::RadioButton("Use current saves and back up destination saves", config.conflict == SaveConflict::UseSource))
                    config.conflict = SaveConflict::UseSource;
                if (ImGui::RadioButton("Keep destination saves", config.conflict == SaveConflict::KeepDestination))
                    config.conflict = SaveConflict::KeepDestination;
            };
            const auto folderButton = [&](const char *id, const char *tooltip) {
                const bool pressed = ImGui::Button(id, ImVec2(kButtonHeight, kButtonHeight));
                // Not while the system's picker has the focus, and gone once the mouse leaves.
                if (IsWindowFocused() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                    ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                    ImGui::SetTooltip("%s", tooltip);
                const auto origin = ImGui::GetItemRectMin();
                const float x = origin.x + 9.0f, y = origin.y + 11.0f;
                const ImVec2 outline[] = {{x, y + 14}, {x, y}, {x + 7, y}, {x + 10, y + 3}, {x + 18, y + 3}, {x + 18, y + 14}};
                ImGui::GetWindowDrawList()->AddPolyline(outline, 6, ImGui::GetColorU32(ImGuiCol_Text), ImDrawFlags_Closed, 1.5f);
                return pressed;
            };
            if (!config.layoutError.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, kErrorText);
                ImGui::TextWrapped("Setup Error: %s", config.layoutError.c_str());
                ImGui::PopStyleColor();
            }
            else if (!activeRun && page == Page::PreparationFailed)
            {
                ImGui::TextUnformatted(pageBeforeFailure == Page::Manage ? "Could not save the new location" : "Setup could not continue");
                showError(actionError);
                if (!preparationLog.empty())
                    ImGui::TextWrapped("Details saved to %s", utf8(preparationLog).c_str());
                ImGui::TextWrapped("Go back to review your choice and try again.");
                if (footer("Back").back)
                    page = pageBeforeFailure;
            }
            else if (!activeRun && page == Page::Uninstall)
            {
                ImGui::TextUnformatted("Uninstall DBZIW?");
                ImGui::TextWrapped("Installation folder: %s", utf8(uninstallFolder(destination.destination)).c_str());
                ImGui::TextWrapped("This removes the installed launcher, game files and settings.");
                Installation installed;
                bool shared = false;
                std::string error;
                const bool ready = loadInstallation(destination.destination, installed, error) &&
                                   uninstallSavesShared(installed, shared, error);
                if (shared)
                    keepUninstallSaves = true;
                ImGui::BeginDisabled(shared || !ready);
                ImGui::Checkbox("Keep my saves", &keepUninstallSaves);
                ImGui::EndDisabled();
                if (shared)
                    ImGui::TextWrapped("Another DBZIW installation uses these saves.");
                if (keepUninstallSaves)
                    ImGui::TextWrapped("Saves will remain in: %s", utf8(installed.userData).c_str());
                else
                    showError("Your saves will be permanently deleted.");
                showError(error);
                const auto actions = footer("Back", preparing ? "Preparing..." : keepUninstallSaves ? "Uninstall" : "Uninstall and delete saves",
                                            ready && destination.error.empty(), true);
                if (actions.back)
                    page = Page::Manage;
                if (actions.primary)
                    beginPreparation(true);
            }
            else if (!activeRun && page == Page::Uninstalled)
            {
                ImGui::TextUnformatted("Ready to uninstall");
                ImGui::TextWrapped("Removal will finish after this window closes.");
                ImGui::TextWrapped("If removal fails, details will be saved to %s.", utf8(uninstallLog).c_str());
                if (footer(nullptr, "Close").primary)
                    quit = true;
            }
            else if (!activeRun && (page == Page::Copied || page == Page::Saved))
            {
                if (page == Page::Copied)
                {
                    ImGui::TextUnformatted("Installation copied");
                    ImGui::TextWrapped("%s", utf8(destination.destination).c_str());
                    ImGui::TextWrapped("Open this launcher to play from the new copy. Your original installation is still in place.");
                }
                else
                {
                    ImGui::TextWrapped("%s", saveConfirmation.c_str());
                    ImGui::TextWrapped("Open the launcher when you are ready to update or play.");
                }
                if (footer(nullptr, "Done").primary)
                    quit = true;
            }
            else if (!activeRun && page == Page::Manage)
            {
                ImGui::TextUnformatted("Manage installation");
                ImGui::TextWrapped("%s", utf8(destination.destination).c_str());
                Installation current;
                std::string metadataError;
                const bool loaded = destination.error.empty() && loadInstallation(destination.destination, current, metadataError);
                const bool canManage = loaded && hasDisc();
                if (canManage)
                {
                    ImGui::TextWrapped("Current saves: %s", utf8(current.userData).c_str());
                    chooseMode(current.mode);
                    chooseConflict();
                    ImGui::TextWrapped("Close the game before switching. Your original saves are kept.");
                }
                else if (loaded)
                    ImGui::TextWrapped("Choose an installed copy of DBZIW to manage its saves.");
                showError(destination.error);
                showError(metadataError);
                showError(actionError);
                const auto copyDestination = config.launcherDir / "dbziw-data";
                if (canManage && !destination.explicitOverride && !config.launcherDir.empty() && destination.destination != copyDestination &&
                    installState(copyDestination, kSupportedDiscs[0]) != InstallState::Installed)
                {
                    ImGui::Separator();
                    ImGui::TextWrapped("Taking DBZIW to another drive or PC? Make a second copy of this installation.");
                    if (ImGui::Button(copyLabel, ImVec2(0, kButtonHeight)))
                    {
                        const auto source = destination.destination;
                        if (selectDestination(copyDestination))
                        {
                            config.importSource = source;
                            config.modeChosen = false;
                            config.conflict = SaveConflict::Ask;
                            copyFromManagement = true;
                            page = Page::Saves;
                        }
                    }
                }
                if (!canManage && !destination.explicitOverride && ImGui::SmallButton("Choose installation..."))
                    openOptions();
                if (canUninstallGameFiles(destination.destination))
                {
                    ImGui::Separator();
                    if (ImGui::Button("Uninstall DBZIW...", ImVec2(0, kButtonHeight)))
                    {
                        keepUninstallSaves = true;
                        page = Page::Uninstall;
                    }
                }
                const auto actions = footer(returnToExisting ? "Back" : nullptr, preparing ? "Saving..." : "Save change",
                                            canManage && config.modeChosen && config.desiredMode != current.mode);
                if (actions.back)
                {
                    config.manageOnly = false;
                    config.modeChosen = false;
                    config.conflict = SaveConflict::Ask;
                    actionError.clear();
                    returnToExisting = false;
                    page = Page::Existing;
                }
                if (actions.primary)
                    beginPreparation();
            }
            else if (!activeRun && page == Page::Saves)
            {
                ImGui::TextDisabled("Copy installation");
                ImGui::TextUnformatted("Where should the copy keep its saves?");
                chooseMode(SaveMode::Normal);
                if (copyFromManagement)
                    ImGui::TextWrapped("Choose Portable if you want saves and settings to travel with the copy.");
                showError(actionError);
                ImGui::Spacing();
                const auto actions = footer(copyFromManagement ? "Back" : nullptr, "Continue");
                if (actions.back && copyFromManagement && selectDestination(config.importSource))
                {
                    config.importSource.clear();
                    config.modeChosen = false;
                    config.conflict = SaveConflict::Ask;
                    copyFromManagement = false;
                    page = Page::Manage;
                }
                if (actions.primary)
                    page = Page::Existing;
            }
            else if (!activeRun && page == Page::Existing)
            {
                const bool importing = !config.importSource.empty();
                ImGui::TextUnformatted(importing ? copyLabel : "We found your DBZIW installation");
                if (importing)
                {
                    ImGui::TextWrapped("From: %s", utf8(config.importSource).c_str());
                    ImGui::TextWrapped("To: %s", utf8(destination.destination).c_str());
                    ImGui::TextWrapped("This creates a second copy. Your current installation stays where it is. No ISO needed.");
                    chooseConflict();
                }
                else
                {
                    if (!saveConfirmation.empty())
                        ImGui::TextWrapped("%s", saveConfirmation.c_str());
                    ImGui::TextWrapped("Update if needed and play from this folder. No ISO needed.");
                    const auto path = utf8(destination.destination);
                    const ImVec2 padding(12, 9);
                    const bool canChangeFolder = !destination.explicitOverride;
                    const float reserved = canChangeFolder ? kButtonHeight + gap : 0.0f;
                    const float wrapWidth = ImGui::GetContentRegionAvail().x - 2 * padding.x - reserved - 2;
                    const float contentHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::CalcTextSize(path.c_str(), nullptr, false, wrapWidth).y;
                    const float panelHeight = std::max(contentHeight, canChangeFolder ? kButtonHeight : 0.0f) + 2 * padding.y + 2;
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.17f, 0.21f, 1.0f));
                    ImGui::BeginChild("DetectedFolder", ImVec2(0, panelHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
                    const float folderButtonX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - kButtonHeight;
                    ImGui::BeginGroup();
                    ImGui::TextDisabled("Installation folder");
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
                    ImGui::TextUnformatted(path.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndGroup();
                    if (canChangeFolder)
                    {
                        ImGui::SameLine(folderButtonX);
                        if (folderButton("##ChangeInstallationFolder", "Change installation folder"))
                            openOptions();
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar(2);
                }
                showError(destination.error);
                showError(actionError);
                ImGui::Spacing();
                if (!importing && ImGui::Button("Manage installation", ImVec2(0, kButtonHeight)))
                {
                    config.manageOnly = true;
                    returnToExisting = true;
                    actionError.clear();
                    page = Page::Manage;
                }
                const char *primary = copyFromManagement ? "Copy installation"
                                      : importing        ? "Copy & Play"
                                      : wizardBuildIsCurrent(config.base, destination.destination, config.releaseId) ? "Play"
                                                                                                                     : "Update & Play";
                if (preparing)
                    primary = importing ? "Copying..." : "Preparing...";
                const auto actions = footer(importing ? "Back" : nullptr, primary, destination.error.empty());
                if (actions.back)
                    page = Page::Saves;
                if (actions.primary)
                {
                    config.base.iso.clear();
                    beginPreparation();
                }
            }
            else if (!activeRun && page == Page::Other)
            {
                const bool newDestination = choosingInstallDestination;
                if (newDestination)
                    ImGui::TextDisabled("Step 2 of 2");
                ImGui::TextUnformatted(newDestination ? "Choose an installation folder" : "Choose an installed DBZIW folder");
                ImGui::TextWrapped("%s", newDestination ? "Choose where DBZIW should install its game files."
                                                        : "Enter or browse to a folder where DBZIW is already installed.");
                showError(destination.error);
                ImGui::BeginDisabled(destination.explicitOverride);
                ImGui::TextDisabled("Installation folder");
                ImGui::SetNextItemWidth(-kButtonHeight - gap);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, (kButtonHeight - ImGui::GetFontSize()) / 2));
                ImGui::InputText("##InstallationFolderPath", folderInput, sizeof(folderInput));
                ImGui::PopStyleVar();
                ImGui::SameLine();
                if (folderButton("##BrowseInstallationFolder", "Browse for a folder"))
                {
                    const fs::path picked = pickFolder("Choose a folder");
                    if (!picked.empty())
                    {
                        std::snprintf(folderInput, sizeof(folderInput), "%s", utf8(picked).c_str());
                        checkedFolder.clear();
                    }
                }
                ImGui::EndDisabled();

                const auto checkFolder = [&] {
                    checkedFolder = folderInput;
                    folderChoice.clear();
                    folderError.clear();
                    createFolder = false;
                    folderHasInstallation = false;
                    if (checkedFolder.empty())
                        return;
                    auto path = fromUtf8(checkedFolder);
                    if (!path.is_absolute())
                        path = config.launcherDir / path;
                    std::error_code ec;
                    const auto status = fs::status(path, ec);
                    if (ec && ec != std::errc::no_such_file_or_directory)
                        folderError = "Cannot read this folder: " + utf8(path);
                    else if (fs::exists(status) && !fs::is_directory(status))
                        folderError = "This path is a file. Choose a folder.";
                    else if (auto existing = existingInstallationRoot(path); !existing.empty())
                    {
                        folderChoice = existing;
                        folderHasInstallation = true;
                    }
                    else if (newDestination && installState(path, kSupportedDiscs[0]) != InstallState::Installed)
                    {
                        const auto target = destination.explicitOverride ? destination.destination : newInstallationRoot(path);
                        ec.clear();
                        const auto targetStatus = fs::status(target, ec);
                        std::string tooLong;
                        if ((ec && ec != std::errc::no_such_file_or_directory) || (fs::exists(targetStatus) && !fs::is_directory(targetStatus)) ||
                            installState(target, kSupportedDiscs[0]) == InstallState::Installed)
                            folderError = "Cannot install to: " + utf8(target);
                        else if (!installPathFits(target, tooLong))
                            folderError = tooLong;
                        else
                        {
                            folderChoice = target;
                            createFolder = !fs::exists(visibleInstallFolder(target), ec);
                        }
                    }
                    else if (!fs::exists(status))
                        folderError = "This folder does not exist: " + utf8(path);
                    else
                        folderError = "This folder exists, but no verified DBZIW installation was found.";
                };
                if (checkedFolder != folderInput)
                    checkFolder();
                showError(folderError);
                if (newDestination && !folderChoice.empty())
                    ImGui::TextWrapped("Install to: %s", utf8(visibleInstallFolder(folderChoice)).c_str());
                if (createFolder)
                    ImGui::TextWrapped("This folder does not exist yet. DBZIW will create it when you start installation.");
                if (newDestination && folderHasInstallation)
                    ImGui::TextWrapped("DBZIW is already installed here. You can use this copy without the ISO.");
                if (newDestination && !folderHasInstallation)
                {
                    ImGui::Spacing();
                    ImGui::TextUnformatted("Saves and settings");
                    Installation selected;
                    std::string modeError;
                    const auto defaultMode = !folderChoice.empty() && loadInstallation(folderChoice, selected, modeError) ? selected.mode : SaveMode::Normal;
                    chooseMode(defaultMode, folderChoice);
                }

                const auto chooseExisting = [&](const fs::path &existing) {
                    if (!selectDestination(existing))
                        return;
                    config.importSource.clear();
                    config.modeChosen = false;
                    config.conflict = SaveConflict::Ask;
                    page = config.manageOnly ? Page::Manage : Page::Existing;
                };
                if (!newDestination)
                {
                    ImGui::BeginDisabled(destination.explicitOverride);
                    for (size_t i = 0; i < config.discovered.size(); ++i)
                    {
                        if (config.discovered[i] == destination.destination && returnFromOptions == Page::Existing)
                            continue;
                        ImGui::PushID(static_cast<int>(i));
                        ImGui::TextWrapped("%s", utf8(config.discovered[i]).c_str());
                        if (ImGui::Button("Use this installation", ImVec2(0, kButtonHeight)))
                        {
                            const auto existing = existingInstallationRoot(config.discovered[i]);
                            if (!existing.empty())
                                chooseExisting(existing);
                            else
                                folderError = "This installation is no longer available.";
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndDisabled();
                }
                const char *folderAction = newDestination ? (folderHasInstallation ? "Use installed copy" : "Install & Play") : "Use this folder";
                const auto actions = footer(returnFromOptions != Page::Other ? "Back" : nullptr, preparing ? "Preparing..." : folderAction,
                                            !folderChoice.empty() && (!destination.explicitOverride || (newDestination && !folderHasInstallation)));
                if (actions.back)
                    page = returnFromOptions;
                if (actions.primary)
                {
                    const bool alreadyAcknowledged = createFolder;
                    checkFolder();
                    if (!folderChoice.empty() && (!createFolder || alreadyAcknowledged))
                    {
                        if (folderHasInstallation)
                            chooseExisting(folderChoice);
                        else if (newDestination && (destination.explicitOverride || selectDestination(folderChoice)))
                            beginPreparation();
                    }
                }
            }
            else if (!activeRun)
            {
                const std::string currentIso = isoPath;
                if (currentIso != validatedIso)
                {
                    validatedIso = currentIso;
                    isoError.clear();
                    isoValid = false;
                    if (!currentIso.empty())
                    {
                        std::error_code ec;
                        const fs::path image = fromUtf8(currentIso);
                        if (!fs::exists(image, ec) || !fs::is_regular_file(image, ec))
                            isoError = "File not found: " + currentIso;
                        else
                            isoValid = discImageValid(image, isoError);
                    }
                }

                if (!config.launcherDir.empty())
                    ImGui::TextDisabled("Step 1 of 2");
                ImGui::TextUnformatted("Install from ISO");
                ImGui::TextWrapped("Choose your legally owned European PAL disc image (.iso).");
                ImGui::Spacing();
                ImGui::TextUnformatted("Disc Image (.iso):");
                const float browseWidth = ImGui::CalcTextSize("Browse...").x + 2 * ImGui::GetStyle().FramePadding.x;
                ImGui::SetNextItemWidth(-browseWidth - gap);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, (kButtonHeight - ImGui::GetFontSize()) / 2));
                ImGui::InputText("##IsoPath", isoPath, sizeof(isoPath));
                ImGui::PopStyleVar();
                ImGui::SameLine();
                if (ImGui::Button("Browse...", ImVec2(browseWidth, kButtonHeight)))
                {
                    const fs::path chosen = pickDiscImage("Choose your disc image");
                    if (!chosen.empty())
                        std::snprintf(isoPath, sizeof(isoPath), "%s", utf8(chosen).c_str());
                }
                if (!validatedIso.empty())
                {
                    if (!isoValid)
                        ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f), "Error: %s", isoError.c_str());
                    else
                        ImGui::TextColored(kValidText, "Valid Dragon Ball Z: Infinite World disc image detected.");
                }
                else
                    ImGui::TextDisabled("(Drag and drop your disc image onto this window)");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextDisabled("Estimated time on first launch: about 4 minutes on a fast PC, longer on older ones.");
                ImGui::Spacing();
                showError(destination.error);
                showError(actionError);
                if (!config.launcherDir.empty() && !destination.explicitOverride &&
                    ImGui::Button("Find an existing installation...", ImVec2(0, kButtonHeight)))
                    openOptions();
                ImGui::Spacing();
                const bool chooseFolder = !config.launcherDir.empty();
                const bool canStart = isoValid && currentIso == isoPath && !hasDisc() &&
                                      (chooseFolder || (destination.error.empty() && !destination.destination.empty()));
                const auto actions = footer(nullptr, preparing ? "Preparing..." : chooseFolder ? "Continue" : "Install & Play", canStart);
                if (actions.primary)
                {
                    config.base.iso = fromUtf8(validatedIso);
                    if (chooseFolder)
                        openOptions(true);
                    else
                        beginPreparation();
                }
            }
            else
            {
                const SetupStatus st = activeRun->status();
                ImGui::Text("Phase: %s", stepName(st.step));
                ImGui::TextWrapped("Current step: %s", st.detail.c_str());
                ImGui::Spacing();
                const float progress = st.finished ? 1.0f : std::min(float(st.overall), 0.999999f);
                char overlay[64];
                std::snprintf(overlay, sizeof(overlay), "%.0f%%", progress * 100.0f);
                ImGui::ProgressBar(progress, ImVec2(-1, 26), overlay);
                ImGui::Spacing();
                if (st.running)
                {
                    ImGui::TextDisabled("Elapsed: %d:%02d", int(st.elapsedSeconds) / 60, int(st.elapsedSeconds) % 60);
                    if (footer("Cancel").back)
                        activeRun->cancel();
                }
                else if (st.failed)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
                    ImGui::TextWrapped("Failed: %s", st.error.c_str());
                    ImGui::PopStyleColor();
                    ImGui::TextWrapped("Logs: %s", utf8(config.base.effectiveLogDir()).c_str());
                    ImGui::Spacing();
                    if (footer("Back").back)
                    {
                        activeRun.reset();
                        if (hasDisc())
                            page = Page::Existing;
                    }
                }
                else if (st.cancelled)
                {
                    ImGui::TextWrapped("Setup cancelled. You can retry with your installed files or choose a disc image.");
                    if (footer("Back").back)
                    {
                        activeRun.reset();
                        if (hasDisc())
                            page = Page::Existing;
                    }
                }
                else if (st.finished)
                {
                    ImGui::TextColored(kValidText, "Build finished successfully! Starting game...");
                    succeeded = true;
                    if (config.closeWhenFinished)
                        quit = true;
                }
            }
            if (!footerDrawn)
                footer();
            ImGui::PopID();
        }
        ImGui::End();
        rlImGuiEnd();
        EndDrawing();
        if (shot && *shot && ++frames == 10)
        {
            Image image = LoadImageFromScreen();
            ExportImage(image, shot);
            UnloadImage(image);
            quit = true;
        }
    }
    activeRun.reset();
    preparation.reset();
    rlImGuiShutdown();
    CloseWindow();
    return succeeded;
}

} // namespace dbziw::setup
