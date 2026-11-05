#include "installation.h"
#include "install_location.h"
#include "install_state.h"
#include "setup_json.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cwctype>
#include <fstream>
#include <set>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#endif

namespace dbziw::setup
{
namespace fs = std::filesystem;
namespace
{
fs::path environment(const char *key)
{
#ifdef _WIN32
    auto nativeKey = pathFromText(key).wstring();
    const wchar_t *value = _wgetenv(nativeKey.c_str());
    return value && *value ? fs::path(value) : fs::path{};
#else
    const char *value = std::getenv(key);
    return value && *value ? pathFromText(value) : fs::path{};
#endif
}
fs::path normalBase(bool config)
{
#ifdef _WIN32
    auto base = environment("LOCALAPPDATA");
    return base.empty() || !base.is_absolute() ? fs::path{} : base / "DBZIW";
#else
    auto base = environment(config ? "XDG_CONFIG_HOME" : "XDG_DATA_HOME");
    if (base.empty() || !base.is_absolute())
    {
        auto home = environment("HOME");
        if (home.empty() || !home.is_absolute())
            return {};
        base = home / (config ? ".config" : ".local/share");
    }
    return base / "dbziw";
#endif
}
bool fail(const fs::path &path, const std::string &message, std::string &error)
{
    error = message + ": " + pathText(path);
    return false;
}
bool readJson(const fs::path &file, json::Value &value, std::string &error)
{
    std::error_code ec;
    if (!fs::is_regular_file(file, ec) || fs::file_size(file, ec) > 65536 || ec)
        return fail(file, "Cannot read installation metadata", error);
    try
    {
        std::ifstream input(file, std::ios::binary);
        if (!input)
            return fail(file, "Cannot open installation metadata", error);
        value = json::parse(std::string(std::istreambuf_iterator<char>(input), {}));
        const auto *version = value.find("version");
        if (!value.isObject() || !version || !version->isNumber() || version->number != 1)
            return fail(file, "Invalid installation metadata version", error);
    }
    catch (const std::exception &)
    {
        return fail(file, "Malformed installation metadata", error);
    }
    return true;
}
// installations.json: the normal-mode installations on this PC and the last one used.
struct InstallIndex
{
    std::vector<std::string> paths;
    std::string preferred;
};
bool readIndex(const fs::path &file, InstallIndex &index, std::string &error)
{
    json::Value value;
    if (!readJson(file, value, error))
        return false;
    const auto *paths = value.find("installations");
    if (!paths || !paths->isArray() || paths->items.size() > 128)
        return fail(file, "Invalid installation location list", error);
    index = {};
    for (const auto &path : paths->items)
    {
        if (!path.isString() || path.text.empty() || !pathFromText(path.text).is_absolute())
            return fail(file, "Installation locations must be absolute paths", error);
        index.paths.push_back(path.text);
    }
    if (const auto *preferred = value.find("preferred"); preferred && preferred->isString())
        index.preferred = preferred->text;
    return true;
}
bool writeIndex(const fs::path &file, const InstallIndex &index, std::string &error)
{
    std::string text = "{\n  \"version\": 1,\n  \"installations\": [";
    for (size_t i = 0; i < index.paths.size(); ++i)
        text += (i ? ",\n    " : "\n    ") + json::quote(index.paths[i]);
    text += "\n  ]";
    if (!index.preferred.empty())
        text += ",\n  \"preferred\": " + json::quote(index.preferred);
    return writeAtomicText(file, text + "\n}\n", error);
}
void setPaths(Installation &installation)
{
    if (installation.mode == SaveMode::Normal)
    {
        installation.userData = normalUserDataDirectory();
        installation.configDir = normalConfigDirectory();
    }
    else
    {
        installation.userData = installation.configDir = installation.root / "user-data";
    }
}
fs::path resolvedPath(const fs::path &path)
{
    std::error_code ec;
    auto result = fs::weakly_canonical(path, ec);
    return (ec ? fs::absolute(path).lexically_normal() : result).make_preferred();
}
bool samePath(const fs::path &a, const fs::path &b)
{
    if (a.empty() || b.empty())
        return a.empty() && b.empty();
#ifdef _WIN32
    return _wcsicmp(resolvedPath(a).c_str(), resolvedPath(b).c_str()) == 0;
#else
    return resolvedPath(a) == resolvedPath(b);
#endif
}
bool sameBytes(const fs::path &a, const fs::path &b)
{
    std::error_code ec;
    if (!fs::is_regular_file(a, ec) || !fs::is_regular_file(b, ec) ||
        fs::file_size(a, ec) != fs::file_size(b, ec) || ec)
        return false;
    std::ifstream left(a, std::ios::binary), right(b, std::ios::binary);
    char x[65536], y[65536];
    while (left && right)
    {
        left.read(x, sizeof(x));
        right.read(y, sizeof(y));
        if (left.gcount() != right.gcount() || !std::equal(x, x + left.gcount(), y))
            return false;
    }
    return left.eof() && right.eof();
}
bool hasRetailMarker(const fs::path &root)
{
    std::error_code ec;
    return fs::is_regular_file(root / "game/dbziw.verified", ec);
}
bool verified(const fs::path &root)
{
    return installState(root, kSupportedDiscs[0]) == InstallState::Installed;
}
bool saveDirectoryName(const fs::path &path)
{
    const auto name = pathText(path.filename());
    return name == "mc0" || name == "mc1";
}
bool findSaveDirectories(const fs::path &root, std::vector<fs::path> &paths, std::string &error)
{
    std::error_code ec;
    for (fs::directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec))
        if (saveDirectoryName(it->path()))
            paths.push_back(it->path());
    return !ec || ec == std::errc::no_such_file_or_directory ||
           fail(root, "Cannot enumerate saves: " + ec.message(), error);
}
struct Copy
{
    fs::path source, destination;
};
bool collect(const fs::path &source, const fs::path &destination, std::vector<Copy> &files,
             std::string &error)
{
    if (samePath(source, destination))
        return true;
    std::error_code ec;
    auto status = fs::symlink_status(source, ec);
    if (ec == std::errc::no_such_file_or_directory || !fs::exists(status))
        return true;
    if (ec)
        return fail(source, "Cannot inspect migration source", error);
    if (fs::is_symlink(status))
        return fail(source, "Migration refuses symbolic links", error);
    if (fs::is_regular_file(status))
    {
        files.push_back({source, destination});
        return true;
    }
    if (!fs::is_directory(status))
        return fail(source, "Unsupported migration source", error);
    const auto relative = resolvedPath(destination).lexically_relative(resolvedPath(source));
    if (!relative.empty() && *relative.begin() != "..")
        return fail(destination, "Migration destination is inside its source", error);
    for (fs::directory_iterator it(source, ec), end; it != end && !ec; it.increment(ec))
    {
        if (!collect(it->path(), destination / it->path().filename(), files, error))
            return false;
    }
    return !ec || fail(source, "Cannot enumerate migration source", error);
}
bool settingsFileName(const fs::path &path)
{
    const auto name = pathText(path.filename());
    return name == "dbziw.ini" || name == "imgui.ini";
}
bool collectUserFiles(const Installation &source, const Installation &destination, std::vector<Copy> &files,
                      std::string &error)
{
    if (!samePath(source.userData, destination.userData))
    {
        std::error_code ec;
        if (fs::exists(source.userData, ec))
        {
            for (fs::directory_iterator it(source.userData, ec), end; it != end && !ec; it.increment(ec))
            {
                if (settingsFileName(it->path()))
                    continue;
                if (!collect(it->path(), destination.userData / it->path().filename(), files, error))
                    return false;
            }
            if (ec)
                return fail(source.userData, "Cannot enumerate user data", error);
        }
    }
    for (const char *name : {"dbziw.ini", "imgui.ini"})
        if (!collect(source.configDir / name, destination.configDir / name, files, error))
            return false;
    return true;
}
bool copyFiles(const std::vector<Copy> &files, SaveConflict policy, std::string &error)
{
    // Decide all conflicts before creating files. A source remains a recovery copy throughout.
    std::error_code ec;
    for (const auto &file : files)
    {
        if (fs::exists(file.destination, ec) && !sameBytes(file.source, file.destination))
        {
            if (!fs::is_regular_file(file.destination, ec))
                return fail(file.destination, "Migration destination is not a file", error);
            if (policy == SaveConflict::Ask)
                return fail(file.destination, "Save conflict: choose current saves or destination saves",
                            error);
        }
        if (ec)
            return fail(file.destination, "Cannot inspect migration destination", error);
    }
    static std::atomic<unsigned long> sequence{0};
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                        std::to_string(++sequence);
    for (const auto &file : files)
    {
        if (sameBytes(file.source, file.destination))
            continue;
        const bool exists = fs::exists(file.destination, ec);
        if (exists && policy == SaveConflict::KeepDestination)
            continue;
        fs::create_directories(file.destination.parent_path(), ec);
        if (ec)
            return fail(file.destination, "Cannot create migration destination", error);
        auto temporary = file.destination;
        temporary += ".dbziw-copy-" + suffix;
        if (fs::exists(temporary, ec))
            return fail(temporary, "Migration staging file already exists", error);
        fs::copy_file(file.source, temporary, fs::copy_options::none, ec);
        if (ec || !sameBytes(file.source, temporary))
        {
            fs::remove(temporary, ec);
            return fail(file.destination, "Save copy verification failed", error);
        }
        if (exists)
        {
            auto backup = file.destination;
            backup += ".dbziw-backup-" + suffix;
            if (fs::exists(backup, ec))
            {
                fs::remove(temporary, ec);
                return fail(backup, "Save backup already exists", error);
            }
            fs::copy_file(file.destination, backup, fs::copy_options::none, ec);
            if (ec || !sameBytes(file.destination, backup))
            {
                fs::remove(temporary, ec);
                return fail(backup, "Cannot preserve destination save", error);
            }
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), file.destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return fail(file.destination, "Cannot commit copied save", error);
#else
        fs::rename(temporary, file.destination, ec);
        if (ec)
            return fail(file.destination, "Cannot commit copied save", error);
#endif
    }
    return true;
}
} // namespace
InstallationLock::~InstallationLock()
{
    if (handle_ == -1)
        return;
#ifdef _WIN32
    ReleaseMutex(reinterpret_cast<HANDLE>(handle_));
    CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
    flock(static_cast<int>(handle_), LOCK_UN);
    close(static_cast<int>(handle_));
#endif
}
bool InstallationLock::acquire(const fs::path &root, std::string &error)
{
#ifdef _WIN32
    auto name = resolvedPath(root).wstring();
    std::uint64_t hash = 14695981039346656037ULL;
    for (auto c : name)
    {
        hash ^= static_cast<std::uint32_t>(towlower(c));
        hash *= 1099511628211ULL;
    }
    const auto mutexName = L"Local\\DBZIW-storage-" + std::to_wstring(hash);
    HANDLE handle = CreateMutexW(nullptr, FALSE, mutexName.c_str());
    if (!handle)
        return fail(root, "Cannot lock installation", error);
    const auto state = WaitForSingleObject(handle, 0);
    if (state != WAIT_OBJECT_0 && state != WAIT_ABANDONED)
    {
        CloseHandle(handle);
        return fail(root, "Another installation action is active; close the game or manager", error);
    }
    handle_ = reinterpret_cast<std::intptr_t>(handle);
#else
    const int handle = open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (handle < 0)
        return fail(root, "Cannot lock installation directory", error);
    if (flock(handle, LOCK_EX | LOCK_NB) != 0)
    {
        close(handle);
        return fail(root, "Another installation action is active; close the game or manager", error);
    }
    handle_ = handle;
#endif
    return true;
}
const char *saveModeName(SaveMode mode)
{
    return mode == SaveMode::Normal ? "normal" : "portable";
}
void configureSaveMode(Installation &installation, SaveMode mode)
{
    installation.mode = mode;
    setPaths(installation);
}
fs::path existingInstallationRoot(const fs::path &selected)
{
    if (verified(selected))
        return resolvedPath(selected);
    for (const auto *child : {"DBZIW-Recomp/dbziw-data", "dbziw-data"})
        if (verified(selected / child))
            return resolvedPath(selected / child);
    return {};
}
fs::path newInstallationRoot(const fs::path &selected)
{
    const auto root = resolvedPath(selected);
    const auto name = root.filename();
    std::error_code ec;
#ifdef _WIN32
    const bool dedicated = _wcsicmp(name.c_str(), L"dbziw-data") == 0;
    const bool branded = _wcsicmp(name.c_str(), L"DBZIW-Recomp") == 0;
#else
    const bool dedicated = name == "dbziw-data";
    const bool branded = name == "DBZIW-Recomp";
#endif
    if (dedicated || fs::is_regular_file(root / "install.json", ec))
        return root;
    return (branded ? root : root / "DBZIW-Recomp") / "dbziw-data";
}
fs::path normalUserDataDirectory()
{
    const auto root = normalBase(false);
    return root.empty() ? root : root / "user-data";
}
fs::path normalConfigDirectory()
{
    const auto root = normalBase(true);
    return root.empty() ? root : root / "config";
}
bool loadInstallation(const fs::path &root, Installation &installation, std::string &error)
{
    error.clear();
    installation = {};
    installation.root = root;
    std::error_code ec;
    const auto metadata = root / "install.json";
    if (fs::exists(metadata, ec))
    {
        json::Value value;
        if (!readJson(metadata, value, error))
            return false;
        const auto *mode = value.find("save_mode");
        if (!mode || !mode->isString())
            return fail(metadata, "Missing save mode", error);
        if (mode->text == "normal")
            installation.mode = SaveMode::Normal;
        else if (mode->text == "portable")
            installation.mode = SaveMode::Portable;
        else
            return fail(metadata, "Unsupported save mode", error);
        installation.recorded = true;
    }
    else if (ec)
        return fail(metadata, "Cannot inspect installation metadata", error);
    else
    {
        // Saves retained after an uninstall of a portable copy: reinstalling finds them.
        std::vector<fs::path> retained;
        if (!findSaveDirectories(root / "user-data", retained, error))
            return false;
        if (!retained.empty())
            installation.mode = SaveMode::Portable;
    }
    setPaths(installation);
    if (installation.recorded && (installation.userData.empty() || installation.configDir.empty()))
        return fail(root, "Cannot determine standard user directories", error);
    return true;
}
bool saveInstallation(const Installation &installation, std::string &error)
{
    return writeAtomicText(installation.root / "install.json",
                           "{\n  \"version\": 1,\n  \"save_mode\": " + json::quote(saveModeName(installation.mode)) + "\n}\n", error);
}
bool rememberNormalInstallation(const Installation &installation, std::string &error, bool makePreferred)
{
    if (installation.mode != SaveMode::Normal || !verified(installation.root))
        return true;
    const auto configDirectory = normalConfigDirectory();
    if (configDirectory.empty())
        return fail(installation.root, "Cannot determine standard configuration directory", error);
    const auto file = configDirectory / "installations.json";
    std::error_code directoryError;
    fs::create_directories(configDirectory, directoryError);
    if (directoryError)
        return fail(configDirectory, "Cannot create installation index directory", error);
    InstallationLock indexLock;
    if (!indexLock.acquire(configDirectory, error))
        return false;
    InstallIndex index;
    std::error_code ec;
    if (fs::exists(file, ec) && !readIndex(file, index, error))
        return false;
    std::string stored;
    for (const auto &path : index.paths)
        if (samePath(pathFromText(path), installation.root))
        {
            stored = path;
            break;
        }
    if (!stored.empty())
    {
        if (!makePreferred || index.preferred == stored)
            return true;
    }
    else
    {
        if (index.paths.size() >= 128)
            return fail(file, "Installation location list is full", error);
        stored = pathText(resolvedPath(installation.root));
        index.paths.push_back(stored);
    }
    if (makePreferred)
        index.preferred = stored;
    return writeIndex(file, index, error);
}
bool prepareInstallation(Installation &installation, std::string &error)
{
    if (installation.userData.empty() || installation.configDir.empty())
        return fail(installation.root,
                    "Cannot determine standard user directories; select portable storage or restore the user "
                    "environment",
                    error);
    if (!validateInstallDirectory(installation.root, error) ||
        !validateInstallDirectory(installation.userData, error) ||
        !validateInstallDirectory(installation.configDir, error))
        return false;
    if (!rememberNormalInstallation(installation, error) || !saveInstallation(installation, error))
        return false;
    installation.recorded = true;
    return true;
}
bool discoverInstallations(const fs::path &launcher, std::vector<fs::path> &found, std::string &error,
                           fs::path *preferred)
{
    found.clear();
    error.clear();
    if (preferred)
        preferred->clear();
    auto add = [&](const fs::path &root) {
        if (!verified(root))
            return;
        for (const auto &existing : found)
            if (samePath(existing, root))
                return;
        found.push_back(resolvedPath(root));
    };
    for (const auto *child : {"dbziw-data", "DBZIW-Recomp/dbziw-data"})
    {
        add(launcher / child);
        if (!found.empty())
            return true;
    }
    const auto configDirectory = normalConfigDirectory();
    const auto file = configDirectory.empty() ? fs::path{} : configDirectory / "installations.json";
    std::error_code ec;
    if (!file.empty() && fs::exists(file, ec))
    {
        InstallIndex index;
        if (!readIndex(file, index, error))
            return false;
        for (const auto &text : index.paths)
        {
            const auto root = pathFromText(text);
            add(root);
            if (preferred && text == index.preferred)
            {
                Installation retained;
                std::string metadataError;
                const bool readyToReinstall = fs::is_directory(root, ec) &&
                    installState(root, kSupportedDiscs[0]) == InstallState::Missing &&
                    loadInstallation(root, retained, metadataError) && retained.recorded &&
                    retained.mode == SaveMode::Normal;
                if (verified(root) || readyToReinstall)
                    *preferred = resolvedPath(root);
                ec.clear();
            }
        }
    }
    // Bounded neighbourhood only: no whole-drive scan and no dependence on the working folder.
    size_t inspected = 0;
    for (const auto &parent : {launcher.parent_path(), launcher.parent_path().parent_path()})
    {
        if (parent.empty() || parent == parent.root_path())
            continue;
        for (fs::directory_iterator it(parent, fs::directory_options::skip_permission_denied, ec), end;
             it != end && !ec && inspected < 256; it.increment(ec))
        {
            if (!it->is_directory(ec) || it->is_symlink(ec))
                continue;
            ++inspected;
            for (const auto *child : {"DBZIW-Recomp/dbziw-data", "dbziw-data"})
                add(it->path() / child);
        }
        ec.clear();
    }
    return true;
}
bool installationGameRunning(const fs::path &root, std::string &error)
{
    error.clear();
#ifdef _WIN32
    const auto expected = resolvedPath(root / "build/dbziw.exe");
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        error = "Cannot check whether the game is running";
        return true;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool running = false;
    for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry))
    {
        if (_wcsicmp(entry.szExeFile, L"dbziw.exe") != 0 || entry.th32ProcessID == GetCurrentProcessId())
            continue;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (!process)
        {
            running = true;
            break;
        }
        wchar_t path[32768];
        DWORD length = 32768;
        if (QueryFullProcessImageNameW(process, 0, path, &length) && samePath(fs::path(path), expected))
            running = true;
        CloseHandle(process);
        if (running)
            break;
    }
    CloseHandle(snapshot);
#else
    const auto expected = resolvedPath(root / "build/dbziw");
    bool running = false;
    std::error_code ec;
    for (fs::directory_iterator it("/proc", fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec))
    {
        const auto name = pathText(it->path().filename());
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos)
            continue;
        std::error_code linkError;
        auto image = fs::read_symlink(it->path() / "exe", linkError);
        if (!linkError && samePath(image, expected))
        {
            running = true;
            break;
        }
    }
    if (ec)
    {
        error = "Cannot check whether the game is running";
        return true;
    }
#endif
    if (running)
        error = "Close the game before changing saves or importing this installation";
    return running;
}
bool switchSaveMode(Installation &installation, SaveMode mode, SaveConflict conflict, std::string &error)
{
    InstallationLock lock;
    if (!lock.acquire(installation.root, error) || installationGameRunning(installation.root, error))
        return false;
    Installation destination = installation;
    destination.mode = mode;
    setPaths(destination);
    if (destination.userData.empty() || destination.configDir.empty())
        return fail(destination.root, "Standard user directories unavailable", error);
    if (mode == installation.mode)
        return true;
    std::vector<Copy> files;
    if (!collectUserFiles(installation, destination, files, error) || !copyFiles(files, conflict, error))
        return false;
    if (!prepareInstallation(destination, error))
        return false;
    installation = destination;
    return true;
}
bool importInstallation(const fs::path &sourceRoot, Installation &destination, SaveConflict conflict,
                        std::string &error)
{
    if (samePath(sourceRoot, destination.root))
        return true;
    if (!verified(sourceRoot))
        return fail(sourceRoot, "Not a verified DBZIW installation", error);
    if (!validateInstallDirectory(destination.root, error))
        return false;
    InstallationLock sourceLock, destinationLock;
    if (!sourceLock.acquire(sourceRoot, error) || !destinationLock.acquire(destination.root, error) ||
        installationGameRunning(sourceRoot, error) || installationGameRunning(destination.root, error))
        return false;
    Installation source;
    if (!loadInstallation(sourceRoot, source, error))
        return false;
    std::vector<Copy> files;
    for (const char *name : {"game", "recomp", "build", "cache", "logs"})
        if (!collect(sourceRoot / name, destination.root / name, files, error))
            return false;
    if (!collectUserFiles(source, destination, files, error) || !copyFiles(files, conflict, error))
        return false;
    return prepareInstallation(destination, error);
}

bool canUninstallGameFiles(const fs::path &root)
{
    return root.is_absolute() && root != root.root_path() && hasRetailMarker(root);
}

fs::path uninstallFolder(const fs::path &root)
{
    const auto parent = root.parent_path();
    return root.filename() == "dbziw-data" && parent.filename() == "DBZIW-Recomp" ? parent : root;
}

namespace {
bool uninstallIndex(const Installation &installation, InstallIndex &index, bool &present, bool &shared, std::string &error)
{
    const auto config = normalConfigDirectory();
    const auto file = config / "installations.json";
    std::error_code ec;
    shared = false;
    present = false;
    if (config.empty() || !fs::exists(file, ec))
        return !ec || fail(file, "Cannot inspect the installation index: " + ec.message(), error);
    if (!readIndex(file, index, error))
        return false;
    present = true;
    for (const auto &entry : index.paths)
    {
        const auto other = pathFromText(entry);
        if (samePath(other, installation.root) || !verified(other))
            continue;
        Installation otherInstallation;
        if (!loadInstallation(other, otherInstallation, error))
            return false;
        shared |= otherInstallation.mode == SaveMode::Normal;
    }
    return true;
}

bool removablePath(const fs::path &path, std::string &error)
{
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory)
        return true;
    if (ec || fs::is_symlink(status))
        return fail(path, "Cannot remove a redirected or inaccessible path", error);
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return fail(path, "Cannot remove a redirected or inaccessible path", error);
    if (fs::is_directory(status)) {
        for (fs::recursive_directory_iterator it(path, ec), end; it != end && !ec; it.increment(ec)) {
            const auto child = GetFileAttributesW(it->path().c_str());
            if (child == INVALID_FILE_ATTRIBUTES || (child & FILE_ATTRIBUTE_REPARSE_POINT))
                return fail(it->path(), "Cannot remove a redirected or inaccessible path", error);
        }
    }
#endif
    return !ec || fail(path, "Cannot inspect files for removal: " + ec.message(), error);
}

bool safeRemovalParent(const fs::path &path, std::string &error)
{
    if (path.empty())
        return true;
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory)
        return true;
    if (ec || fs::is_symlink(status) || !fs::is_directory(status))
        return fail(path, "Cannot remove files through a redirected or inaccessible path", error);
#ifdef _WIN32
    if (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
        return fail(path, "Cannot remove files through a redirected or inaccessible path", error);
#endif
    return true;
}

#ifdef _WIN32
bool collectPackageFiles(const fs::path &payload, std::vector<fs::path> &files,
                         std::vector<fs::path> &launchers, std::set<fs::path, std::greater<>> &parents,
                         std::string &error)
{
    const auto manifest = payload / "package-files.txt";
    std::error_code ec;
    if (!removablePath(manifest, error) || fs::file_size(manifest, ec) > 16 * 1024 * 1024 || ec)
        return fail(manifest, "Invalid package inventory", error);
    std::ifstream in(manifest, std::ios::binary);
    if (!in)
        return fail(manifest, "Cannot read the package inventory", error);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto relative = pathFromText(line);
        if (line.empty() || line.find_first_of(":\\") != std::string::npos ||
            line.find('\0') != std::string::npos || relative.has_root_path() ||
            std::any_of(relative.begin(), relative.end(), [](const fs::path &part) {
                return part.empty() || part == "." || part == "..";
            }))
            return fail(manifest, "Invalid path in the package inventory", error);
        const auto first = *relative.begin();
        const std::string component = pathText(first);
        static const std::set<std::string> packaged{"app", "external", "recomp", "tools", "LICENSES",
                                                    "launcher", "BUILD_ID", "LICENSE"};
        if (!packaged.contains(component))
            return fail(manifest, "The package inventory includes an unmanaged path", error);
        const auto file = payload / relative;
        for (auto parent = file.parent_path(); parent != payload; parent = parent.parent_path()) {
            const bool added = parents.insert(parent).second;
            if (added && !safeRemovalParent(parent, error))
                return false;
        }
        const auto status = fs::symlink_status(file, ec);
        if (ec != std::errc::no_such_file_or_directory && (ec || !fs::is_regular_file(status)))
            return fail(file, "A packaged file was replaced by another kind of file", error);
        (first == "launcher" || relative == "BUILD_ID" ? launchers : files).push_back(file);
    }
    if (!in.eof())
        return fail(manifest, "Cannot read the package inventory", error);
    parents.insert(payload);
    launchers.push_back(manifest);
    return true;
}
#endif
} // namespace

bool uninstallSavesShared(const Installation &installation, bool &shared, std::string &error)
{
    shared = false;
    InstallIndex index;
    bool present = false;
    return installation.mode != SaveMode::Normal || uninstallIndex(installation, index, present, shared, error);
}

bool uninstallInstallation(const fs::path &selected, bool keepSaves, std::string &error)
{
    error.clear();
    if (!canUninstallGameFiles(selected))
        return fail(selected, "Cannot identify this folder as a DBZIW installation", error);
    const auto root = resolvedPath(selected);
    auto requested = selected.lexically_normal().make_preferred();
#ifdef _WIN32
    const bool redirected = _wcsicmp(requested.c_str(), root.c_str()) != 0;
#else
    const bool redirected = requested != root;
#endif
    if (redirected)
        return fail(selected, "Cannot uninstall through a redirected path", error);
    InstallationLock lock;
    if (!lock.acquire(root, error) || installationGameRunning(root, error))
        return false;
    if (!hasRetailMarker(root))
        return fail(root, "The installation changed before removal", error);

    Installation installation;
    if (!loadInstallation(root, installation, error))
        return false;
    const auto folder = uninstallFolder(root);
    const auto config = normalConfigDirectory();
    const auto indexPath = config / "installations.json";
    for (const auto &parent : {root / "game", root / "recomp", root / "user-data", config, installation.userData})
        if (!safeRemovalParent(parent, error))
            return false;
    InstallationLock indexLock;
    std::error_code ec;
    if (!config.empty() && fs::is_directory(config, ec) && !indexLock.acquire(config, error))
        return false;
    InstallIndex index;
    bool indexPresent = false, shared = false;
    if (!uninstallIndex(installation, index, indexPresent, shared, error))
        return false;
    if (!keepSaves && installation.mode == SaveMode::Normal && shared)
        return fail(installation.userData, "Another DBZIW installation shares these saves", error);

    std::vector<fs::path> targets{root / "build", root / "build-setup", root / "recomp", root / "builder", root / "cache",
                                  root / "logs", root / "user-data/dbziw.ini", root / "user-data/imgui.ini"};
    // The game's files: everything in game/ except the marker, which goes last, so a failed removal can be retried.
    for (fs::directory_iterator it(root / "game", ec), end; it != end && !ec; it.increment(ec))
        if (it->path().filename() != "dbziw.verified")
            targets.push_back(it->path());
    if (ec)
        return fail(root / "game", "Cannot list the game files: " + ec.message(), error);
    std::vector<fs::path> launchers;
    std::set<fs::path, std::greater<>> packageParents;
    // Packaged files only in the self-contained layout, and only those the package listed.
    if (folder != root) {
        targets.push_back(folder / "README.txt");
#ifdef _WIN32
        const auto payload = root / "package";
        if (fs::is_regular_file(payload / "BUILD_ID", ec) && fs::is_regular_file(payload / "launcher/DBZIW.exe", ec)) {
            if (!collectPackageFiles(payload, targets, launchers, packageParents, error))
                return false;
            launchers.push_back(folder / "DBZIW.exe");
        }
#else
        launchers.push_back(folder / "DBZIW.AppImage");
#endif
    }
    if (installation.mode == SaveMode::Normal && !shared)
        for (const char *name : {"dbziw.ini", "imgui.ini"})
            targets.push_back(installation.configDir / name);
    if (!keepSaves &&
        (!findSaveDirectories(root, targets, error) ||
         !findSaveDirectories(root / "user-data", targets, error) ||
         (installation.mode == SaveMode::Normal &&
          !findSaveDirectories(installation.userData, targets, error))))
        return false;

    for (const auto &target : targets)
        if (!removablePath(target, error))
            return false;
    for (const auto &target : launchers)
        if (!removablePath(target, error))
            return false;
    if (!removablePath(root / "game/dbziw.verified", error) || !removablePath(root / "install.json", error))
        return false;
    for (const auto &target : targets) {
        fs::remove_all(target, ec);
        if (ec)
            return fail(target, "Cannot remove DBZIW files: " + ec.message(), error);
    }
    if (indexPresent) {
        std::erase_if(index.paths, [&](const std::string &entry) { return samePath(pathFromText(entry), root); });
        if (!index.preferred.empty() && samePath(pathFromText(index.preferred), root))
            index.preferred.clear();
        if (index.paths.empty())
            fs::remove(indexPath, ec);
        else if (!writeIndex(indexPath, index, error))
            return false;
        if (ec)
            return fail(indexPath, "Cannot remove the installation record: " + ec.message(), error);
    }
    for (const auto &target : launchers) {
        fs::remove_all(target, ec);
        if (ec)
            return fail(target, "Cannot remove the installed launcher: " + ec.message(), error);
    }
    for (const auto &metadata : {root / "install.json", root / "game/dbziw.verified", root / "setup.lock"}) {
        fs::remove(metadata, ec);
        if (ec)
            return fail(metadata, "Cannot remove installation metadata: " + ec.message(), error);
    }
    packageParents.insert({root / "game", root / "user-data", root / "package", root / "cache", root, folder});
    if (installation.mode == SaveMode::Normal && !shared) {
        packageParents.insert(installation.configDir);
        if (!keepSaves)
            packageParents.insert(installation.userData);
    }
    for (const auto &directory : packageParents) {
        fs::remove(directory, ec);
        if (ec && ec != std::errc::directory_not_empty)
            return fail(directory, "Cannot remove the empty folder: " + ec.message(), error);
    }
    return true;
}
} // namespace dbziw::setup
