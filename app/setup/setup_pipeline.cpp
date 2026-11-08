#include "setup_pipeline.h"
#include "codegen.h"
#include "install_state.h"
#include "setup_cache.h"
#include "setup_layout.h"
#include "vu1lift/vu1_translator.h"
#include "ioplift/iop_generate.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>

namespace dbziw::setup {
namespace fs = std::filesystem;

namespace {
constexpr size_t kLogLines = 200;

} // namespace

const char *stepName(Step step)
{
    switch (step)
    {
    case Step::Check: return "Check the disc";
    case Step::Extract: return "Copy the game files";
    case Step::Generate: return "Translate the game code";
    case Step::LiftVu1: return "Translate the VU1 code";
    case Step::Build: return "Compile the game";
    case Step::Done: return "Done";
    }
    return "?";
}

double overallProgress(const std::vector<Step> &plan, Step current, double fraction)
{
    const auto weight = [](Step s) {
        switch (s)
        {
        case Step::Check: return 1.0;
        case Step::Extract: return 13.0;
        case Step::Generate: return 9.0;
        case Step::LiftVu1: return 23.0;
        case Step::Build: return 166.0; // seconds of a timed clean setup (007-speed-final-20261104-212123)
        case Step::Done: return 0.0;
        }
        return 0.0;
    };
    double total = 0.0, done = 0.0;
    for (Step s : plan)
    {
        total += weight(s);
        if (int(s) < int(current))
            done += weight(s);
        else if (s == current)
            done += weight(s) * (fraction < 0.0 ? 0.0 : fraction > 1.0 ? 1.0 : fraction);
    }
    return total > 0.0 ? done / total : (current == Step::Done ? 1.0 : 0.0);
}

void StepContext::progress(double fraction, const std::string &detail)
{
    std::lock_guard lock(m_run.m_mutex);
    auto &st = m_run.m_status;
    st.fraction = fraction < 0.0 ? 0.0 : fraction > 1.0 ? 1.0 : fraction;
    st.detail = detail;
    st.overall = std::min(0.999999, std::max(st.overall, overallProgress(st.plan, st.step, st.fraction)));
}

void StepContext::log(const std::string &line)
{
    {
        std::lock_guard lock(m_run.m_mutex);
        auto &log = m_run.m_status.log;
        log.push_back(line);
        if (log.size() > kLogLines)
            log.erase(log.begin(), log.begin() + long(log.size() - kLogLines));
    }
    std::error_code ec;
    const fs::path dir = m_run.m_options.effectiveLogDir();
    fs::create_directories(dir, ec);
    std::ofstream(dir / "setup.log", std::ios::app) << line << "\n";
}

bool StepContext::cancelled() const { return m_run.m_cancel; }
const std::atomic<bool> *StepContext::cancelFlag() const { return &m_run.m_cancel; }
ps2recomp::timing::Session *StepContext::timing() const { return m_run.m_timing.get(); }
uint64_t StepContext::timingParent() const { return m_run.m_timingParent; }

SetupSteps defaultSetupSteps()
{
    SetupSteps s;
    s.generate = [](const SetupOptions &o, StepContext &c, std::string &error) {
        CodegenOptions options;options.reuse=o.reuseGenerated;options.force=o.forceGenerate;options.cancel=c.cancelFlag();
        int current=0;
        // Weight profiles by their declared map entries. Within a profile, count analysis/output work,
        // never elapsed time; unknown discovery work reports activity without advancing this estimate.
        std::vector<double> weights;double totalWeight=0,before=0,within=0;
        for(const auto &profile:kCodegenProfiles)
        {
            std::ifstream map(o.root/profile.ghidraCsv);std::string line;double rows=0;
            std::getline(map,line);
            while(std::getline(map,line)) if(!line.empty() && line!="\r") ++rows;
            weights.push_back(std::max(1.0,rows));totalWeight+=weights.back();
        }
        options.activity=[&](const CodegenProgress &p) {
            std::string label=p.stage;
            if(label=="analysis" || label=="recompile") label="analysing game functions";
            else if(label=="entry_discovery") label="finding additional game functions";
            else if(label=="continuation_discovery") label="following shared code paths";
            else if(label=="output") label="writing generated code";
            else if(label=="initialize" || label=="input_map") label="reading game code and function map";
            else if(label=="config") label="reading translation settings";
            else if(label=="translator") label="starting translator";
            std::string detail=p.profile+": "+label;
            if(p.total) detail+=" "+std::to_string(p.done)+"/"+std::to_string(p.total);
            if(p.total && p.stage=="analysis") within=std::max(within,0.5*double(p.done)/p.total);
            else if(p.total && p.stage=="output") within=std::max(within,0.5+0.5*double(p.done)/p.total);
            else if(p.stage=="verified cached output") within=1;
            c.progress((before+weights[current]*within)/totalWeight,detail);
        };
        return generateCode(o.root, o.recompiler, o.effectiveLogDir(), [&](const char *name, int index, int count) {
            current=index;within=0;before=0;
            for(int i=0;i<index;++i) before+=weights[i];
            c.progress(before/totalWeight,name);
            c.log(std::string("recompiling or verifying ") + name);
            return !c.cancelled();
        }, error, o.effectiveDataDir(), c.timing(), c.timingParent(),options);
    };
    s.liftVu1 = [](const SetupOptions &o, StepContext &c, std::string &error) {
        const auto elf=o.effectiveDataDir()/kCodegenProfiles[0].input;
        std::vector<CacheInput> runtimeInputs;
        std::string identityError;
        const bool known=vuRuntimeInputs(runtimeInputs,identityError) && std::string(vuProducerIdentity()).size()==64;
        {
            const std::string kind="vu1";
            const auto folder=o.effectiveDataDir()/"recomp"/kind;
            const auto record=o.effectiveDataDir()/"cache/generation"/(kind+".record");
            ps2recomp::timing::Stage stage(c.timing(),kind,c.timingParent());
            ps2recomp::timing::Stage validation(c.timing(),"cache_validation",stage.id(),kind);
            c.progress(0,kind+": checking generated output");
            auto inputs=runtimeInputs;
            for(const auto &file:vu1lift::inputFiles(elf)) inputs.push_back({"disc/"+file.filename().string(),file});
            // DBZIW 012 splits the output into files of kTargetsPerPart programs (DW5 042 used whole segments up to 512 KiB).
            const std::map<std::string,std::string> values={{"producer",vuProducerIdentity()},{"unit",kind},
                {"layout","parts-"+std::to_string(vu1lift::kTargetsPerPart)+"-1"},{"static-timing","all"}};
            std::string keyError,reason;
            const auto key=o.reuseGenerated && known?cacheKey(inputs,values,keyError):std::string();
            const bool hit=o.reuseGenerated && !o.forceGenerate && !key.empty() && cachedOutputs(record,key,folder,reason);
            if(!o.reuseGenerated) reason="reuse disabled";
            else if(o.forceGenerate) reason="explicit rebuild";
            else if(key.empty()) reason=keyError.empty()?identityError:keyError;
            validation.count("hit",hit);validation.field("reason",reason);
            validation.field("input_key",key.empty()?"unavailable":key);validation.finish();
            stage.field("input_key",key.empty()?"unavailable":key);stage.field("producer",vuProducerIdentity());
            if(c.cancelled()) {error="cancelled";return false;}
            if(hit) {c.log(kind+": "+reason);stage.finish("cache_hit");return true;}
            if(o.reuseGenerated) fs::remove(record);
            c.log(kind+": generating ("+(o.forceGenerate?"explicit rebuild":key.empty()?"producer identity unavailable":reason)+")");
            ps2recomp::timing::Stage generate(c.timing(),kind+"_generate",stage.id());
            std::vector<std::string> parts;
            const auto progress=[&](const std::string &line) {
                if(c.cancelled()) throw std::runtime_error("cancelled");
                c.log(line);
            };
            const auto counts=[&](size_t done,size_t total) {
                if(c.cancelled()) throw std::runtime_error("cancelled");
                c.progress(0.9*double(done)/double(total),kind+": translating "+std::to_string(done)+"/"+std::to_string(total)+" programs");
            };
            if(!vu1lift::generateLiftedVu1(elf,parts,error,progress,{},vu1lift::kTargetsPerPart,counts)) return false;
            generate.finish();
            const auto scratch=o.effectiveDataDir()/"build-setup"/kind;
            fs::remove_all(scratch);fs::create_directories(scratch);
            ps2recomp::timing::Stage write(c.timing(),kind+"_write",stage.id());
            for(size_t i=0;i<parts.size();++i)
            {
                if(c.cancelled()) {error="cancelled";return false;}
                char name[64];
                std::snprintf(name,sizeof(name),"dbziw_vu1_lifted_%02zu.cpp",i);
                if(!atomicWrite(scratch/name,parts[i],error)) return false;
            }
            if(!syncDirectory(scratch,folder,error,false,[&](uint64_t n,uint64_t total) {
                c.progress(0.95,kind+": synchronizing "+std::to_string(n)+"/"+std::to_string(total));return !c.cancelled();
            })) return false;
            fs::remove_all(scratch);write.finish();
            if(o.reuseGenerated && !key.empty())
            {
                ps2recomp::timing::Stage publish(c.timing(),"cache_publish",stage.id(),kind);
                if(cacheKey(inputs,values,keyError)!=key) {error="VU generation inputs changed";return false;}
                if(c.cancelled()) {error="cancelled";return false;}
                if(!publishCachedOutputs(record,key,folder,error)) return false;
                publish.finish();
            }
            stage.finish();
        }
        // DW5 009 v4.5 (015): the disc's IOP modules as C++ (well under a second, so no cache; files that did not change
        // are left alone by the synchronization).
        {
            ps2recomp::timing::Stage stage(c.timing(),"iop",c.timingParent());
            c.progress(0.95,"iop: translating the IOP modules");
            std::map<std::string,std::string> files;
            if(!dbziw::ioplift::generateIopModules(elf.parent_path()/"MODULES",files,error,
                                                   [&](const std::string &line){c.log("iop: "+line);})) return false;
            const auto folder=o.effectiveDataDir()/"recomp/iop";
            const auto scratch=o.effectiveDataDir()/"build-setup"/"iop";
            fs::remove_all(scratch);fs::create_directories(scratch);
            for(const auto &[name,bytes]:files)
            {
                if(c.cancelled()) {error="cancelled";return false;}
                if(!atomicWrite(scratch/name,bytes,error)) return false;
            }
            if(!syncDirectory(scratch,folder,error,false,[&](uint64_t,uint64_t){return !c.cancelled();})) return false;
            fs::remove_all(scratch);
            stage.finish();
        }
        return true;
    };
    s.build = [](const SetupOptions &o, StepContext &c, fs::path &executable, std::string &error) {
        return buildGame(o.root, o.effectiveBuildDir(), o.tools, o.effectiveLogDir() / "build.log",
                         [&,last=BuildProgress{}](const BuildProgress &p) mutable {
                             // DW5-E fac1943a: compiler output can hide Ninja's counter; the last one stays, so the bar never jumps back.
                             if (p.total > 0) last = p;
                             else if (last.total == 0) last.phase = p.phase;
                             c.progress(last.total > 0 ? double(last.done)/last.total : 0.0, buildStepDetail(last));
                         },
                         executable, error, c.cancelFlag(), o.dataDir.empty() ? fs::path() : o.dataDir / "recomp",
                         c.timing(), c.timingParent());
    };
    return s;
}

SetupRun::SetupRun(SetupOptions options, SetupSteps steps) : m_options(std::move(options)), m_steps(std::move(steps)) {}

SetupRun::~SetupRun()
{
    cancel();
    if (m_thread.joinable())
        m_thread.join();
}

void SetupRun::start()
{
    {
        std::lock_guard lock(m_mutex);
        m_status = {};
        m_status.running = true;
    }
    m_cancel = false;
    m_completed=false;
    m_started=std::chrono::steady_clock::now();
    m_thread = std::thread([this] { run(); });
}

bool SetupRun::wait()
{
    if (m_thread.joinable())
        m_thread.join();
    std::lock_guard lock(m_mutex);
    return m_status.finished;
}

SetupStatus SetupRun::status() const
{
    std::lock_guard lock(m_mutex);
    auto result=m_status;
    result.elapsedSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-m_started).count();
    return result;
}

void SetupRun::enter(Step step)
{
    {
        std::lock_guard lock(m_mutex);
        m_status.step = step;
        m_status.fraction = 0.0;
        m_status.detail.clear();
        m_status.overall = std::max(m_status.overall, overallProgress(m_status.plan, step, 0.0));
    }
    StepContext(*this).log(std::string("== ") + stepName(step));
}

bool SetupRun::fail(const std::string &error)
{
    std::lock_guard lock(m_mutex);
    m_status.running = false;
    if (m_cancel)
    {
        m_status.cancelled = true;
        return false;
    }
    m_status.failed = true;
    m_status.error = std::string(stepName(m_status.step)) + ": " + error;
    return false;
}

void SetupRun::run()
{
    try
    {
        std::string error;
        auto lease=SetupLease::acquire(m_options.effectiveDataDir(),error);
        if(!lease) {fail(error);return;}
        const std::string run=ps2recomp::timing::newRunId();
        if(!m_options.releaseId.empty()) m_options.logDir=m_options.effectiveLogDir()/run;
        const fs::path logs=m_options.effectiveLogDir();
        fs::create_directories(logs);
        m_timingFile.open(logs/("setup-timing-"+run+".jsonl"),std::ios::binary);
        if(!m_timingFile) {fail("cannot open setup timing log");return;}
        m_timing=std::make_unique<ps2recomp::timing::Session>(m_timingFile,run,"dbziw_setup",std::string(),[this] {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-m_started).count();
        });
        ps2recomp::timing::Stage total(m_timing.get(),"setup",0,{},0);
        m_timingParent=total.id();
        total.field("release",m_options.releaseId.empty()?"development":m_options.releaseId);
        total.field("vu_producer",vuProducerIdentity());
        bool identifyPackage=m_options.usePersistentBuilder;
        // 007: the launcher and package go into the chosen folder first, so the download is not needed afterwards.
        if(!m_options.releaseId.empty())
        {
            const auto downloaded=m_options.root;
            if(!installReleaseLauncher(m_options.root,m_options.effectiveDataDir(),error)) {total.finish("failed");fail(error);return;}
            if(m_options.root!=downloaded)
            {
                SetupLayout installed;
#ifdef _WIN32
                constexpr bool isWindows=true;
#else
                constexpr bool isWindows=false;
#endif
                if(!findLayout(m_options.root,isWindows,installed,error)) {total.finish("failed");fail(error);return;}
                m_options.recompiler=installed.recompiler;m_options.tools=installed.tools;
            }
            // An installed package already keeps one path; only a mounted or moved one needs the stable builder.
            std::error_code samePackage;
            if(fs::equivalent(m_options.root,m_options.effectiveDataDir()/"package",samePackage)) {m_options.usePersistentBuilder=false;identifyPackage=true;}
        }
        const auto marker=m_options.effectiveBuildDir()/"built-from.txt";
        std::string packageKey;
        const auto package=m_options.root;
        try
        {
            if(!m_options.releaseId.empty())
            {
                fs::remove(marker);fs::remove(m_options.effectiveBuildDir()/"ready.record");fs::remove(m_options.effectiveBuildDir()/"ready.stamp");
                if(identifyPackage)
                {
                    ps2recomp::timing::Stage identity(m_timing.get(),"package_identity",total.id());
                    StepContext(*this).progress(0,"checking package sources and tools");
                    packageKey=packageIdentity(package,error);
                    if(packageKey.empty()) {total.field("error",error);total.finish(m_cancel?"cancelled":"failed");fail(error);return;}
                    total.field("package_sha256",packageKey);
                    identity.finish();
                }
            }
            if(m_options.usePersistentBuilder)
            {
                ps2recomp::timing::Stage prepare(m_timing.get(),"builder_sync",total.id());
                fs::path stable;
                StepContext ctx(*this);
                ctx.progress(0,"checking builder sources and tools");
                if(!prepareBuildRoot(m_options.root,m_options.effectiveDataDir(),stable,error,[&](uint64_t n,uint64_t count) {
                    ctx.progress(0,"checking builder sources and tools "+std::to_string(n)+"/"+std::to_string(count));
                    return !m_cancel;
                })) {total.field("error",error);total.finish(m_cancel?"cancelled":"failed");fail(error);return;}
                SetupLayout layout;
#ifdef _WIN32
                constexpr bool windows=true;
#else
                constexpr bool windows=false;
#endif
                if(!findLayout(stable,windows,layout,error)) {total.field("error",error);total.finish("failed");fail(error);return;}
                m_options.root=stable;m_options.recompiler=layout.recompiler;m_options.tools=layout.tools;
                prepare.finish();
            }
            runSteps();
            if(m_completed && !m_cancel && !m_options.releaseId.empty())
            {
                if(!packageKey.empty())
                {
                    ps2recomp::timing::Stage verify(m_timing.get(),"package_verify",total.id());
                    if(packageIdentity(package,error)!=packageKey) {m_completed=false;fail("package changed during setup");}
                    else if(!atomicWrite(m_options.effectiveBuildDir()/"ready.record",packageKey+"\n",error) ||
                            !recordPackageStamp(package,m_options.effectiveBuildDir(),error)) {m_completed=false;fail(error);}
                    verify.finish(m_completed?"success":"failed");
                }
                if(m_completed && !atomicWrite(marker,m_options.releaseId+"\n",error)) {m_completed=false;fail(error);}
            }
        }
        catch(const std::exception &e) {m_completed=false;fail(e.what());}
        const bool complete=m_completed && !m_cancel;
        if(!complete && !m_options.releaseId.empty()) {std::error_code ec;fs::remove(marker,ec);}
        if(!complete) total.field("error",status().error);
        total.finish(complete?"success":m_cancel?"cancelled":"failed");
        if(!m_timing->healthy())
        {
            if(!m_options.releaseId.empty()) {std::error_code ec;fs::remove(marker,ec);}
            fail("writing setup timing evidence failed");return;
        }
        if(complete)
        {
            std::lock_guard lock(m_mutex);
            m_status.step=Step::Done;m_status.fraction=1;m_status.overall=1;
            m_status.running=false;m_status.finished=true;
        }
        else if(m_cancel) fail("cancelled");
    }
    catch(const std::exception &e) {fail(e.what());}
}

void SetupRun::runSteps()
{
    StepContext ctx(*this);
    const uint64_t setupTiming = m_timingParent;
    const auto &discs = m_options.discs;

    // Check: both images are recognised before anything is written.
    enter(Step::Check);
    ps2recomp::timing::Stage checkTime(ctx.timing(), "check", setupTiming);
    struct Image
    {
        fs::path path;
        const DiscSpec *disc;
        IsoImage iso;
    };
    std::vector<Image> images;
    const fs::path data = m_options.effectiveDataDir();
    const bool installed = !discs.empty() && installState(data, discs[0]) == InstallState::Installed;
    if (m_options.iso.empty() && installed)
        ctx.log(std::string("using the installed files of ") + discs[0].name);
    else
        images.push_back({m_options.iso, discs.empty() ? nullptr : &discs[0], {}});
    {
        std::vector<Step> plan;
        if (!images.empty())
            plan = {Step::Check, Step::Extract};
        if (!m_options.installOnly)
            plan.insert(plan.end(), {Step::Generate, Step::LiftVu1, Step::Build});
        std::lock_guard lock(m_mutex);
        m_status.plan = plan;
    }
    for (auto &image : images)
    {
        std::string error;
        if (!image.disc)
        {
            fail("no disc to check against");
            return;
        }
        if (image.path.empty() || !fs::exists(image.path))
        {
            fail("the disc image " + image.path.string() + " does not exist");
            return;
        }
        if (!image.iso.open(image.path, error))
        {
            fail(image.path.string() + " is not a disc image (" + error + ")");
            return;
        }
        const DiscSpec one[] = {*image.disc};
        if (!identifyDisc(image.iso, one, error))
        {
            fail(image.path.filename().string() + " is not " + image.disc->name + ": " + error);
            return;
        }
        ctx.log(image.path.filename().string() + ": " + image.disc->name);
    }

    checkTime.finish();
    // Extract, with checksums.
    enter(Step::Extract);
    ps2recomp::timing::Stage extractTime(ctx.timing(), "extract", setupTiming);
    uint64_t total = 0, before = 0;
    for (auto &image : images)
        for (const auto &file : image.disc->files)
            total += file.size;
    for (auto &image : images)
    {
        std::string error;
        uint64_t last = 0;
        const bool ok = extractDisc(image.iso, *image.disc, discFolder(data, *image.disc),
                                    [&](const ExtractProgress &p) {
                                        last = p.doneBytes;
                                        ctx.progress(total ? double(before + p.doneBytes) / total : 1.0, p.file);
                                        return !m_cancel;
                                    }, error);
        image.iso.close();
        if (!ok)
        {
            fail(error);
            return;
        }
        before += last;
        ctx.log(std::string("copied ") + image.disc->name + " to game/" + image.disc->folder);
        if (!writeInstallMarker(data, *image.disc, error))
        {
            fail(error);
            return;
        }
    }
    extractTime.finish();
    if(m_options.installOnly) {m_completed=true;return;}

    const auto runStep = [&](Step step, const SetupSteps::Fn &fn) {
        enter(step);
        ps2recomp::timing::Stage stepTime(ctx.timing(), step == Step::Generate ? "generate" : "vu", setupTiming);
        m_timingParent = stepTime.id();
        std::string error;
        if (!fn(m_options, ctx, error) || m_cancel)
        {
            stepTime.finish(m_cancel ? "cancelled" : "failed");
            return fail(error.empty() ? "stopped" : error);
        }
        stepTime.finish();
        ctx.progress(1.0, {});
        return true;
    };
    if (!runStep(Step::Generate, m_steps.generate) || !runStep(Step::LiftVu1, m_steps.liftVu1))
        return;

    enter(Step::Build);
    ps2recomp::timing::Stage buildTime(ctx.timing(), "native_build", setupTiming);
    m_timingParent = buildTime.id();
    fs::path executable;
    std::string error;
    if (!m_steps.build(m_options, ctx, executable, error) || m_cancel)
    {
        fail(error.empty() ? "stopped" : error);
        buildTime.finish(m_cancel ? "cancelled" : "failed");
        return;
    }
    buildTime.finish();

    {std::lock_guard lock(m_mutex);m_status.executable=executable;}
    m_completed=true;
}

} // namespace dbziw::setup
