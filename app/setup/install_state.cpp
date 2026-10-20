#include "install_state.h"

#include <cstdlib>
#include <fstream>

namespace dbziw::setup {
namespace fs = std::filesystem;

namespace {
constexpr const char *kMarkerHeader = "dbziw install 1";

fs::path envPath(const char *name)
{
    const char *value = std::getenv(name);
    if (!value || !*value)
        return {};
    return fs::path(std::u8string(reinterpret_cast<const char8_t *>(value)));
}

// <data>/game/<disc.folder>.verified: written only after every checksum of that disc matched.
fs::path markerPath(const fs::path &dataDir, const DiscSpec &disc)
{
    const std::string name = (disc.folder && *disc.folder) ? disc.folder : "dbziw";
    return dataDir / "game" / (name + ".verified");
}

// The first file of `disc` that is missing or has another size, or empty.
std::string firstProblem(const fs::path &folder, const DiscSpec &disc)
{
    std::error_code ec;
    for (const auto &file : disc.files)
    {
        const fs::path path = folder / fs::path(file.path);
        const auto size = fs::file_size(path, ec);
        if (ec)
            return std::string(file.path) + " is missing";
        if (size != file.size)
            return std::string(file.path) + " has another size";
    }
    return {};
}
} // namespace

fs::path playerDataDir()
{
    if (fs::path dir = envPath("DBZIW_DATA_DIR"); !dir.empty())
        return dir;
#ifdef _WIN32
    if (fs::path local = envPath("LOCALAPPDATA"); !local.empty())
        return local / "DBZIW";
#else
    if (fs::path xdg = envPath("XDG_DATA_HOME"); !xdg.empty())
        return xdg / "dbziw";
    if (fs::path home = envPath("HOME"); !home.empty())
        return home / ".local" / "share" / "dbziw";
#endif
    return {};
}

fs::path discFolder(const fs::path &dataDir, const DiscSpec &disc)
{
    if (disc.folder && *disc.folder && std::string(disc.folder) != "dbziw")
        return dataDir / "game" / disc.folder;
    return dataDir / "game";
}

InstallState installState(const fs::path &dataDir, const DiscSpec &disc, std::string *problem)
{
    std::ifstream marker(markerPath(dataDir, disc));
    std::string header, name;
    if (!marker || !std::getline(marker, header) || header != kMarkerHeader || !std::getline(marker, name) ||
        name != disc.name)
        return InstallState::Missing;
    const std::string found = firstProblem(discFolder(dataDir, disc), disc);
    if (found.empty())
        return InstallState::Installed;
    if (problem)
        *problem = found;
    return InstallState::Incomplete;
}

bool writeInstallMarker(const fs::path &dataDir, const DiscSpec &disc, std::string &error)
{
    if (const std::string found = firstProblem(discFolder(dataDir, disc), disc); !found.empty())
    {
        error = "cannot mark " + std::string(disc.name) + " as installed: " + found;
        return false;
    }
    std::error_code ec;
    fs::create_directories(markerPath(dataDir, disc).parent_path(), ec);
    std::ofstream out(markerPath(dataDir, disc), std::ios::trunc);
    out << kMarkerHeader << "\n" << disc.name << "\n" << disc.files.size() << " files\n";
    if (!out.flush())
    {
        error = "cannot write " + markerPath(dataDir, disc).string();
        return false;
    }
    return true;
}

} // namespace dbziw::setup
