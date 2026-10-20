#pragma once
// Setup wizard pipeline: from your own disc to a playable game.
// Steps: check the disc image -> extract -> generate game C++ -> compile.
// Runs on its own thread; the caller polls status() and may cancel().
#include "build_game.h"
#include "disc_setup.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace dbziw::setup {

enum class Step
{
    Check,    // recognise the disc image by file sizes
    Extract,  // copy the files with checksum verification
    Generate, // recompiler: main program
    Build,    // compile and link the game
    Done,
};
inline constexpr int kStepCount = int(Step::Done);
const char *stepName(Step step);

double overallProgress(const std::vector<Step> &plan, Step current, double fraction);

struct SetupOptions
{
    std::filesystem::path root;       // project / release root (contains app/, recomp/, game/)
    std::filesystem::path iso;        // Dragon Ball Z: Infinite World (PAL) ISO
    std::filesystem::path recompiler; // ps2_recomp(.exe)
    BuildTools tools;
    std::filesystem::path buildDir;   // empty: root/app/build
    std::filesystem::path logDir;     // empty: root/logs/setup
    std::span<const DiscSpec> discs = kSupportedDiscs;
    bool installOnly = false;         // Check + Extract only, then write install marker
    std::filesystem::path dataDir;    // player data folder

    std::filesystem::path effectiveDataDir() const { return dataDir.empty() ? root : dataDir; }
    std::filesystem::path effectiveBuildDir() const
    {
        return !buildDir.empty() ? buildDir : dataDir.empty() ? root / "app" / "build" : dataDir / "build";
    }
    std::filesystem::path effectiveLogDir() const
    {
        return !logDir.empty() ? logDir : dataDir.empty() ? root / "logs" / "setup" : dataDir / "logs";
    }
};

struct SetupStatus
{
    Step step = Step::Check;
    double fraction = 0.0;
    double overall = 0.0;
    std::vector<Step> plan;
    std::string detail;
    bool running = false;
    bool finished = false;
    bool failed = false;
    bool cancelled = false;
    std::string error;
    std::filesystem::path executable;
    std::vector<std::string> log;
};

class SetupRun;

class StepContext
{
public:
    explicit StepContext(SetupRun &run) : m_run(run) {}
    void progress(double fraction, const std::string &detail);
    void log(const std::string &line);
    bool cancelled() const;
    const std::atomic<bool> *cancelFlag() const;

private:
    SetupRun &m_run;
};

struct SetupSteps
{
    using Fn = std::function<bool(const SetupOptions &, StepContext &, std::string &error)>;
    Fn generate;
    std::function<bool(const SetupOptions &, StepContext &, std::filesystem::path &executable, std::string &error)>
        build;
};
SetupSteps defaultSetupSteps();

class SetupRun
{
public:
    explicit SetupRun(SetupOptions options, SetupSteps steps = defaultSetupSteps());
    ~SetupRun();
    SetupRun(const SetupRun &) = delete;
    SetupRun &operator=(const SetupRun &) = delete;

    void start();
    void cancel() { m_cancel = true; }
    bool wait();
    SetupStatus status() const;

private:
    friend class StepContext;
    void run();
    void enter(Step step);
    bool fail(const std::string &error);

    SetupOptions m_options;
    SetupSteps m_steps;
    std::atomic<bool> m_cancel{false};
    mutable std::mutex m_mutex;
    SetupStatus m_status;
    std::thread m_thread;
};

} // namespace dbziw::setup
