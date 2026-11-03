// Authored regression (DBZIW 037): DBZIW's ROM profile gives rom0:ROMVER the 16 bytes of a PAL console (region 'E',
// "\n\0" at the end), and only for SLES_553.47. The two checks below follow the game's own two readers. No game code
// or data.
#include "ps2_runtime.h"
#include "runtime/ps2_rom_device.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {
void require(bool ok, const char *what)
{
    if (!ok)
        throw std::runtime_error(what);
}
}

int main()
{
    try
    {
        PS2Runtime runtime;
        std::vector<uint8_t> bytes;
        std::string error;

        require(runtime.romDevice().configure({"SLES_553.47", 0u, 0u}, &error), "the DBZIW profile configures");
        require(runtime.romDevice().activeProvider() == "dbziw", "the DBZIW profile is the one selected");
        require(runtime.romDevice().readFile("rom0:ROMVER", bytes), "rom0:ROMVER exists");
        const std::string pal = "0200EC20040614\n";
        require(bytes.size() == 16u && std::string(bytes.begin(), bytes.begin() + 15) == pal && bytes[15] == 0u,
                "ROMVER is SCPH-70004's 16 bytes");

        // The language check reads 14 bytes and tests byte 4 for 'T'.
        require(bytes[4] == 'E', "region letter E (PAL)");
        // libgraph reads byte by byte to the zero, then parses the date that starts 9 bytes before it.
        size_t zero = 0;
        while (zero < bytes.size() && bytes[zero] != 0u)
            ++zero;
        require(zero == 15u, "the terminating zero is at byte 15");
        const long date = std::strtol(reinterpret_cast<const char *>(bytes.data()) + zero - 9u, nullptr, 10);
        require(date == 20040614L && date > 0x1315670L, "the date parses and is after 2001-06-08");

        require(runtime.romDevice().configure({"OTHER.ELF", 0u, 0u}, &error), "another game configures");
        require(runtime.romDevice().activeProvider().empty(), "no profile for another game");
        require(runtime.romDevice().readFile("rom0:ROMVER", bytes) && bytes.size() == 14u,
                "another game keeps the runtime's base ROMVER");
        std::cout << "DBZIW ROM profile tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
