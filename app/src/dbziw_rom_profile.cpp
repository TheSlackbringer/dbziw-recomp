// DBZIW (PAL SLES_553.47): rom0:ROMVER as a PAL console holds it (issue 037).
//
// The runtime's base profile is a US console ("0200AC20040614", 14 bytes, no terminator). Every PAL BIOS checked
// (SCPH-30003 to SCPH-77004) holds 16 bytes: version, region 'E', type 'C', the build date, then "\n\0". This is
// SCPH-70004's (v2.00, 2004-06-14: the base profile's version and date, region changed). The game reads it twice:
// 14 bytes for its language choice (FUN_00382070: byte 4 == 'T' takes a fixed language), and libgraph reads it byte
// by byte to its zero to compare the date with 2001-06-08 (FUN_00100558, only for video modes other than NTSC/PAL).
#include "runtime/ps2_rom_device.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace
{
    struct DbziwRomProfile
    {
        DbziwRomProfile()
        {
            static constexpr char kRomVersion[] = "0200EC20040614\n"; // sizeof keeps the terminating zero
            static_assert(sizeof(kRomVersion) == 16u);
            PS2RomProfile profile;
            profile.id = "pal-scph-70004";
            profile.provider = "dbziw";
            profile.matcher.elfName = "SLES_553.47";
            profile.files["ROMVER"] = std::vector<uint8_t>(kRomVersion, kRomVersion + sizeof(kRomVersion));
            PS2RomDevice::registerProfile(std::move(profile));
        }
    };
    const DbziwRomProfile g_dbziwRomProfile;
}
