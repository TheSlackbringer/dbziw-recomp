#include "install_location.h"
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    std::string error;
    fs::path result;
    bool explicitChoice = false;
    const auto launcher = dbziw::setup::launcherDirectory(error);
    const fs::path expected = (fs::absolute(fs::u8path(argv[1])) / "dbziw-data").lexically_normal();
    const bool ok = dbziw::setup::resolveInstallLocation(launcher, {}, result, explicitChoice, error) &&
                    result.lexically_normal() == expected && !explicitChoice;
    std::cout << (ok ? "PASS" : "FAIL")
              << ": default installation is in dbziw-data beside the launcher, independent of working/user-data folders\n";
    return ok ? 0 : 1;
}
