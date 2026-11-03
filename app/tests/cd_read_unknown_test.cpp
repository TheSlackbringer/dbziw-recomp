// Authored regression (DBZIW 035): sceCdRead(lsn a0, sectors a1, buffer a2, mode a3) with a sector the runtime
// cannot place must fail (return 0) and leave EE memory as it was. The replacement used to retry with its arguments
// in other orders: here the buffer address is also a valid sector number, so the swapped guess read that sector into
// the address given as the sector number. A normal read still works. No game code or data.
#include "ps2_runtime.h"
#include "ps2_stubs.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {
void reg(R5900Context &c, unsigned n, uint32_t v) { c.r[n] = _mm_cvtsi32_si128(static_cast<int>(v)); }
uint32_t result(R5900Context &c) { return static_cast<uint32_t>(_mm_cvtsi128_si32(c.r[2])); }
void require(bool ok, const char *why)
{
    if (!ok)
    {
        std::cerr << "FAILED: " << why << '\n';
        throw std::runtime_error(why);
    }
}
bool untouched(const std::vector<uint8_t> &ee, uint32_t at)
{
    for (uint32_t i = 0; i < 2048u; ++i)
        if (ee[at + i] != 0xa5)
            return false;
    return true;
}
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("dbziw-cd-read-unknown-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        std::filesystem::create_directory(root);
        std::vector<uint8_t> sector(2048);
        for (size_t i = 0; i < sector.size(); ++i)
            sector[i] = static_cast<uint8_t>((i * 37 + 11) & 255);
        {
            std::ofstream f(root / "fixture.bin", std::ios::binary);
            f.write(reinterpret_cast<const char *>(sector.data()), static_cast<std::streamsize>(sector.size()));
        }
        PS2Runtime::IoPaths paths{};
        paths.cdRoot = root;
        PS2Runtime::setIoPaths(paths);
        PS2Runtime runtime;
        R5900Context c{};
        std::vector<uint8_t> ee(32 * 1024 * 1024, 0xa5);
        std::strcpy(reinterpret_cast<char *>(ee.data() + 0x3000), "cdrom0:\\fixture.bin;1");
        reg(c, 4, 0x4000);
        reg(c, 5, 0x3000);
        ps2_stubs::sceCdSearchFile(ee.data(), &c, &runtime);
        require(result(c) == 1, "search the fixture");
        uint32_t lsn = 0;
        std::memcpy(&lsn, ee.data() + 0x4000, 4);
        require(lsn < ee.size() - 2048u, "the fixture's sector number is also a usable EE address for this test");
        std::fill(ee.begin() + 0x4000, ee.begin() + 0x4100, 0xa5);

        // Unknown sector 0x600000 (no file, no image) into a buffer whose address equals the fixture's sector.
        constexpr uint32_t kUnknown = 0x600000u;
        reg(c, 4, kUnknown);
        reg(c, 5, 1);
        reg(c, 6, lsn);
        reg(c, 7, 0);
        ps2_stubs::sceCdRead(ee.data(), &c, &runtime);
        require(result(c) == 0, "an unknown sector fails the read");
        require(untouched(ee, kUnknown), "nothing written at the address given as the sector number");
        require(untouched(ee, lsn), "the buffer is left as it was");

        // The standard layout still reads.
        constexpr uint32_t kBuffer = 0x8000u;
        reg(c, 4, lsn);
        reg(c, 5, 1);
        reg(c, 6, kBuffer);
        ps2_stubs::sceCdRead(ee.data(), &c, &runtime);
        require(result(c) == 1, "a known sector reads");
        require(std::memcmp(ee.data() + kBuffer, sector.data(), sector.size()) == 0, "the sector lands in the buffer");

        std::filesystem::remove_all(root);
        std::cout << "sceCdRead unknown-sector tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
