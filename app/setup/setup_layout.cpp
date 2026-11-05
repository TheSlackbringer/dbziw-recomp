#ifdef _WIN32
#include <cwchar>
#endif
#include "setup_layout.h"
#include "codegen.h"
#include "setup_cache.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace dbziw::setup {
namespace fs = std::filesystem;

bool prepareLinuxBuildRoot(const fs::path &root, const fs::path &data, fs::path &buildRoot, std::string &error)
{
    return prepareBuildRoot(root,data,buildRoot,error);
}

bool installReleaseLauncher(fs::path &root, const fs::path &data, std::string &error)
{
    const auto folder = data.parent_path();
#ifdef _WIN32
    if (_wcsicmp(data.filename().c_str(), L"dbziw-data") != 0 || _wcsicmp(folder.filename().c_str(), L"DBZIW-Recomp") != 0)
#else
    if (data.filename() != "dbziw-data" || folder.filename() != "DBZIW-Recomp")
#endif
        return true;
    std::error_code ec;
    const auto fail = [&](const fs::path &path) {
        error = "Cannot install launcher file " + path.string() + ": " + ec.message();
        return false;
    };
    const auto read = [](const fs::path &path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    // Staged beside the target and renamed, so a launcher is never half written.
    const auto copy = [&](const fs::path &source, const fs::path &target) {
        if (fs::is_regular_file(target, ec) && read(source) == read(target))
            return true;
        const auto temporary = folder / (".dbziw-launcher-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(target.parent_path(), ec);
        if (!fs::create_directory(temporary, ec))
        {
            if (!ec)
                ec = std::make_error_code(std::errc::file_exists);
            return fail(temporary);
        }
        const auto staged = temporary / target.filename();
        fs::copy_file(source, staged, fs::copy_options::none, ec);
        if (!ec)
            fs::rename(staged, target, ec);
        const bool copied = !ec || fail(target);
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        fs::remove(temporary, cleanup);
        return copied;
    };
#ifdef _WIN32
    const auto payload = data / "package";
    if (root.filename() != "package" || fs::equivalent(root, payload, ec))
    {
        ec.clear();
        return true;
    }
    ec.clear();
    fs::create_directories(payload, ec);
    if (ec)
        return fail(payload);
    for (const char *item : {"launcher", "app", "external", "recomp", "tools", "LICENSES"})
        if (!syncDirectory(root / item, payload / item, error, true))
            return false;
    for (const char *item : {"LICENSE", "package-files.txt", "BUILD_ID"})
        if (!copy(root / item, payload / item))
            return false;
    const auto download = root.parent_path().parent_path();
    if (!copy(download / "README.txt", folder / "README.txt") || !copy(download / "DBZIW.exe", folder / "DBZIW.exe"))
        return false;
    root = payload;
#else
    const char *image = std::getenv("APPIMAGE");
    if (!image || !*image)
        return true;
    const fs::path source(image);
    const auto launcher = folder / "DBZIW.AppImage";
    if (fs::equivalent(source, launcher, ec))
        return true;
    ec.clear();
    if (!copy(root / "README.txt", folder / "README.txt") || !copy(source, launcher))
        return false;
    fs::permissions(launcher, fs::perms::owner_exec, fs::perm_options::add, ec);
    if (ec)
        return fail(launcher);
#endif
    return true;
}

bool prepareBuildRoot(const fs::path &root,const fs::path &data,fs::path &buildRoot,std::string &error,
                      const std::function<bool(uint64_t,uint64_t)> &progress)
{
    buildRoot = data / "builder";
    for (const char *folder : {"app", "external", "recomp", "tools"})
        if (!syncDirectory(root / folder, buildRoot / folder, error, true, progress, data))
            return false;
    const auto read = [](const fs::path &path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    const std::string version = read(root / "BUILD_ID");
    if (version != read(buildRoot / "BUILD_ID"))
    {
        if(!atomicWrite(buildRoot/"BUILD_ID",version,error)) return false;
    }
    return true;
}

fs::path findRoot(const fs::path &start)
{
    std::error_code ec;
    fs::path dir = fs::absolute(start, ec).lexically_normal();
    while (!dir.empty())
    {
        if (fs::exists(dir / "app" / "CMakeLists.txt", ec) && fs::is_directory(dir / "recomp", ec))
            return dir;
        const fs::path parent = dir.parent_path();
        if (parent == dir)
            break;
        dir = parent;
    }
    return {};
}

bool findLayout(const fs::path &root, bool windows, SetupLayout &layout, std::string &error)
{
    const std::string exe = windows ? ".exe" : "";
    const fs::path tools = root / "tools";
    std::error_code ec;
    layout = {};
    layout.root = root;

    const fs::path bundled = tools / ("ps2_recomp" + exe);
    const fs::path developer = root / "external" / "PS2Recomp" / "build" / "ps2xRecomp" / ("ps2_recomp" + exe);
    layout.recompiler = fs::exists(bundled, ec) ? bundled : developer;
    if (!fs::exists(layout.recompiler, ec))
    {
        error = "the recompiler is missing (" + bundled.string() + ")";
        return false;
    }

    const fs::path cmake = tools / "cmake" / "bin" / ("cmake" + exe);
    layout.tools.cmake = fs::exists(cmake, ec) ? cmake : fs::path("cmake");
    const fs::path ninja = tools / ("ninja" + exe);
    if (fs::exists(ninja, ec))
        layout.tools.ninja = ninja;
    if (windows)
    {
        layout.tools.llvmMingwRoot = tools / "llvm-mingw";
        if (!fs::is_directory(layout.tools.llvmMingwRoot, ec))
        {
            error = "the bundled compiler is missing (" + layout.tools.llvmMingwRoot.string() + ")";
            return false;
        }
        if (fs::exists(tools / "ffmpeg.zip", ec))
            layout.tools.ffmpegArchive = tools / "ffmpeg.zip";
    }
    else if (fs::exists(root / "BUILD_ID", ec))
    {
        layout.tools.linuxToolchainRoot = tools / "clang";
        layout.tools.ffmpegRoot = tools / "ffmpeg";
        for (const fs::path &required : {cmake, ninja, tools / "clang" / "bin" / "clang",
                                        tools / "clang" / "bin" / "clang++",
                                        tools / "ffmpeg" / "include" / "libavcodec" / "avcodec.h"})
        {
            if (!fs::exists(required, ec))
            {
                error = "a bundled Linux build tool is missing (" + required.string() + ")";
                return false;
            }
        }
    }
    return true;
}

} // namespace dbziw::setup
