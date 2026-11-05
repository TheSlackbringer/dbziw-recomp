#include "install_location.h"
#include <filesystem>
#include <iostream>
int main()
{
    namespace fs = std::filesystem;
    std::string error;
    fs::path root;
    bool explicitChoice = false;
    const fs::path launcher = fs::temp_directory_path() / "dbziw-storage-default-regression";
    if (!dbziw::setup::resolveInstallLocation(launcher, {}, root, explicitChoice, error))
    {
        std::cerr << error;
        return 1;
    }
    const bool ok = root == (launcher / "dbziw-data");
    std::cout << (ok ? "PASS" : "FAIL") << ": new installation defaults to dbziw-data beneath launcher\n";
    return ok ? 0 : 1;
}
