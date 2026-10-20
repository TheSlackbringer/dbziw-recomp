#include "wizard_ui.h"
#include "codegen.h"
#include "disc_setup.h"
#include "file_dialog.h"
#include "run_process.h"

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
const DiscSpec *probe(const fs::path &image, std::string &message)
{
    IsoImage iso;
    std::string error;
    if (image.empty())
        return message.clear(), nullptr;
    if (!iso.open(image, error))
        return message = "not a disc image: " + error, nullptr;
    for (const auto &disc : kSupportedDiscs)
    {
        const DiscSpec one[] = {disc};
        if (identifyDisc(iso, one, error))
            return message = disc.name, &disc;
    }
    return message = "not a supported disc (only the PAL SLES_553.47 disc works)", nullptr;
}

fs::path gameFolder(const fs::path &root) { return root / "game"; }

bool openFolder(const fs::path &dir, std::string &error)
{
#ifdef _WIN32
    return launchDetached("explorer.exe", {utf8(dir)}, {}, error);
#else
    return launchDetached("xdg-open", {utf8(dir)}, {}, error);
#endif
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

namespace {

struct DiscField
{
    const char *label;
    char path[4096] = "";
    std::string message;
    const DiscSpec *disc = nullptr;
    bool installed = false;
    bool replace = false;

    void set(const fs::path &p)
    {
        std::snprintf(path, sizeof(path), "%s", utf8(p).c_str());
        recheck();
    }
    void recheck() { disc = probe(fromUtf8(path), message); }
};

struct Wizard
{
    SetupOptions base;
    std::string layoutError;
    DiscField isoField{"Dragon Ball Z: Infinite World (PAL) disc image"};
    std::unique_ptr<SetupRun> run;
    std::chrono::steady_clock::time_point started;
    std::string actionError;

    bool canStart() const
    {
        const bool isoOk = isoField.disc == &kSupportedDiscs[0] || (isoField.installed && isoField.path[0] == '\0');
        return layoutError.empty() && isoOk;
    }

    void start()
    {
        SetupOptions o = base;
        o.iso = fromUtf8(isoField.path);
        run = std::make_unique<SetupRun>(o);
        run->start();
        started = std::chrono::steady_clock::now();
        actionError.clear();
    }

    void dropped(const fs::path &p)
    {
        isoField.set(p);
    }

    void discRow(DiscField &f, bool locked)
    {
        ImGui::TextUnformatted(f.label);
        ImGui::PushID(f.label);
        if (f.installed && !f.replace && f.path[0] == '\0')
        {
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1), "Installed and verified - no disc image needed.");
            if (!locked)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Use another image..."))
                    f.replace = true;
            }
            ImGui::PopID();
            ImGui::Spacing();
            return;
        }
        ImGui::BeginDisabled(locked);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
        if (ImGui::InputText("##path", f.path, sizeof(f.path)) || ImGui::IsItemDeactivatedAfterEdit())
            f.recheck();
        ImGui::SameLine();
        ImGui::BeginDisabled(!fileDialogAvailable());
        if (ImGui::Button("Browse...", ImVec2(100.0f, 0.0f)))
        {
            const fs::path chosen = pickDiscImage(f.label);
            if (!chosen.empty())
                f.set(chosen);
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (f.installed && !locked)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Keep the installed files"))
            {
                f.path[0] = '\0';
                f.message.clear();
                f.disc = nullptr;
                f.replace = false;
            }
        }
        if (!f.message.empty())
        {
            const bool good = f.disc != nullptr;
            ImGui::TextColored(good ? ImVec4(0.45f, 0.85f, 0.45f, 1) : ImVec4(1.0f, 0.45f, 0.4f, 1), "%s %s",
                               good ? "OK:" : "Not usable:", f.message.c_str());
        }
        ImGui::PopID();
        ImGui::Spacing();
    }

    void steps(const SetupStatus &st, double seconds)
    {
        int index = 0;
        for (size_t i = 0; i < st.plan.size(); ++i)
            if (st.plan[i] == st.step)
                index = int(i) + 1;
        const int count = int(st.plan.size());
        if (st.finished)
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1), "Done.");
        else if (st.failed)
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1), "Stopped at: %s", stepName(st.step));
        else if (index > 0)
            ImGui::Text("Step %d of %d: %s", index, count, stepName(st.step));
        else
            ImGui::TextUnformatted(stepName(st.step));
        char label[64];
        std::snprintf(label, sizeof(label), "%d %%", int(st.overall * 100.0));
        ImGui::ProgressBar(float(st.overall), ImVec2(-1.0f, 28.0f), label);
        if (!st.detail.empty() && !st.finished)
            ImGui::TextDisabled("%s", st.detail.c_str());
        if (st.running && st.overall > 0.15 && seconds > 30.0)
        {
            const int left = int(seconds * (1.0 - st.overall) / st.overall / 60.0 + 0.5);
            ImGui::TextDisabled(left <= 1 ? "About a minute left" : "About %d minutes left", left);
        }
    }

    void frame()
    {
        const SetupStatus st = run ? run->status() : SetupStatus{};
        const bool running = run && st.running;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(float(GetScreenWidth()), float(GetScreenHeight())));
        ImGui::Begin("setup", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted(base.installOnly ? "Dragon Ball Z: Infinite World - your disc, once"
                                                : "Dragon Ball Z: Infinite World - build the game from your own disc");
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("The game files come from your own retail disc image. Nothing is downloaded and nothing leaves "
                           "this PC. Drop a disc image here or browse.");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
        if (!layoutError.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1), "This package is incomplete: %s", layoutError.c_str());

        if (isoField.installed && !run && !base.installOnly)
        {
            ImGui::TextWrapped("Your game files are already installed. Start builds the game for this version.");
            ImGui::Spacing();
        }
        discRow(isoField, running);

        if (!run)
        {
            ImGui::BeginDisabled(!canStart());
            if (ImGui::Button(base.installOnly ? "Install" : "Start", ImVec2(140.0f, 0.0f)))
                start();
            ImGui::EndDisabled();
            ImGui::SameLine();
            const bool copies = isoField.path[0] != '\0';
            ImGui::TextDisabled(base.installOnly ? "Copies and checks the game files from your disc (1-2 minutes)."
                                : copies         ? "Copies the game files, translates and compiles them (10-30 minutes)."
                                                 : "Translates and compiles the game (10-30 minutes).");
        }
        else
        {
            ImGui::Separator();
            const auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
            steps(st, double(secs));
            if (running)
            {
                ImGui::Text("Running for %lld:%02lld", (long long)(secs / 60), (long long)(secs % 60));
                if (ImGui::Button("Cancel", ImVec2(140.0f, 0.0f)))
                    run->cancel();
            }
            else if (st.finished)
            {
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1), "Ready. The game is in %s",
                                   utf8(gameFolder(base.root)).c_str());
                if (ImGui::Button("Play", ImVec2(140.0f, 0.0f)))
                    launchDetached(st.executable, {"./" + utf8(fs::path(kCodegenProfiles[0].input).filename())},
                                   gameFolder(base.root), actionError);
            }
            else
            {
                if (st.cancelled)
                    ImGui::TextUnformatted("Cancelled.");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1), "%s", st.error.c_str());
                if (ImGui::Button("Try again", ImVec2(140.0f, 0.0f)))
                    run.reset();
            }
            if (!running)
            {
                ImGui::SameLine();
                if (ImGui::Button("Open the log folder", ImVec2(180.0f, 0.0f)))
                    openFolder(base.effectiveLogDir(), actionError);
            }
            if (!actionError.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1), "%s", actionError.c_str());
            ImGui::Spacing();
            ImGui::BeginChild("log", ImVec2(0, 0), ImGuiChildFlags_Borders);
            for (const auto &line : st.log)
                ImGui::TextUnformatted(line.c_str());
            if (running)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
        }
        ImGui::End();
    }
};

} // namespace

bool runWizardWindow(const WizardConfig &config)
{
    Wizard w;
    w.base = config.base;
    w.layoutError = config.layoutError;
    w.isoField.installed = config.discInstalled;
    if (!config.iso.empty())
        w.isoField.set(config.iso);
    bool finished = false;
    const char *shot = std::getenv("DBZIW_WIZARD_SHOT");
    int frames = 0;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(820, 560, "Dragon Ball Z: Infinite World setup");
    SetWindowMinSize(640, 480);
    SetTargetFPS(30);
    rlImGuiSetup(true);
    ImGui::GetIO().IniFilename = nullptr;
    while (!WindowShouldClose())
    {
        if (w.run && w.run->status().finished)
        {
            finished = true;
            if (w.base.installOnly || config.closeWhenFinished)
                break;
        }
        if (IsFileDropped())
        {
            FilePathList files = LoadDroppedFiles();
            for (unsigned i = 0; i < files.count; ++i)
                w.dropped(fromUtf8(files.paths[i]));
            UnloadDroppedFiles(files);
        }
        BeginDrawing();
        ClearBackground(Color{24, 26, 30, 255});
        rlImGuiBegin();
        w.frame();
        rlImGuiEnd();
        EndDrawing();
        if (shot && *shot && ++frames == 30)
        {
            Image image = LoadImageFromScreen();
            ExportImage(image, shot);
            UnloadImage(image);
        }
    }
    w.run.reset();
    rlImGuiShutdown();
    CloseWindow();
    return finished;
}

struct DiscInstallPanel::State
{
    fs::path dataDir;
    const DiscSpec *disc;
    DiscField field{"Disc image"};
    std::unique_ptr<SetupRun> run;
};

DiscInstallPanel::DiscInstallPanel(fs::path dataDir, const DiscSpec &disc) : m(std::make_unique<State>())
{
    m->dataDir = std::move(dataDir);
    m->disc = &disc;
    m->field.label = disc.name;
}

DiscInstallPanel::~DiscInstallPanel() = default;

bool DiscInstallPanel::running() const { return m->run && m->run->status().running; }

void DiscInstallPanel::dropped(const fs::path &image)
{
    if (!running())
        m->field.set(image);
}

bool DiscInstallPanel::draw()
{
    const SetupStatus st = m->run ? m->run->status() : SetupStatus{};
    if (st.finished)
    {
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1), "%s is ready.", m->disc->name);
        return true;
    }
    const bool busy = m->run && st.running;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Choose your own disc image (.iso). Its files are checked and copied once. "
                       "You can also drop the image on the game window.");
    ImGui::PopStyleColor();
    ImGui::BeginDisabled(busy);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
    if (ImGui::InputText("##disc", m->field.path, sizeof(m->field.path)) || ImGui::IsItemDeactivatedAfterEdit())
        m->field.recheck();
    ImGui::SameLine();
    ImGui::BeginDisabled(!fileDialogAvailable());
    if (ImGui::Button("Browse...", ImVec2(100.0f, 0.0f)))
    {
        const fs::path chosen = pickDiscImage(m->disc->name);
        if (!chosen.empty())
            m->field.set(chosen);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    const bool right = m->field.disc && std::string(m->field.disc->name) == m->disc->name;
    if (!m->field.message.empty())
    {
        if (right)
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1), "OK: %s", m->field.message.c_str());
        else if (m->field.disc)
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1), "This is %s, not %s.", m->field.message.c_str(),
                               m->disc->name);
        else
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1), "Not usable: %s", m->field.message.c_str());
    }
    if (busy)
    {
        ImGui::ProgressBar(float(st.overall), ImVec2(-1.0f, 0.0f), st.detail.c_str());
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            m->run->cancel();
        return false;
    }
    if (st.failed)
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1), "%s", st.error.c_str());
    else if (st.cancelled)
        ImGui::TextUnformatted("Cancelled.");
    ImGui::BeginDisabled(!right);
    if (ImGui::Button("Install", ImVec2(120.0f, 0.0f)))
    {
        SetupOptions o;
        o.root = m->dataDir;
        o.installOnly = true;
        o.logDir = m->dataDir / "logs";
        o.iso = fromUtf8(m->field.path);
        o.discs = std::span<const DiscSpec>(m->disc, 1);
        m->run = std::make_unique<SetupRun>(o);
        m->run->start();
    }
    ImGui::EndDisabled();
    return false;
}

} // namespace dbziw::setup
