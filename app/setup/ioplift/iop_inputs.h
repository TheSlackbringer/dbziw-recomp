#pragma once
// DW5 009 v4.5 (015): the IOP modules of the PAL disc (SLES_553.47) that dbziw_ioplift translates. Names, sizes,
// FNV-1a 64 hashes of the disc files and the IOP RAM address each one is loaded at (the game loads the same modules
// in the same order on every boot: 015 M1). No game data.

#include <cstdint>

namespace dbziw::ioplift
{
    struct IopModuleInput
    {
        const char *file;  // in the disc's MODULES folder
        const char *name;
        uint32_t fileSize;
        uint64_t fileHash; // FNV-1a 64 of the whole file
        uint32_t base;     // load address
    };

    inline constexpr IopModuleInput kIopModules[] = {
        {"SIO2MAN.IRX", "SIO2MAN", 5217u, 0x27F5CD2C84CD27CFull, 0x10000u},
        {"LIBSD.IRX", "LIBSD", 30085u, 0x4F06C33C857F97FAull, 0x10d00u},
        {"CRI_ADXI.IRX", "CRI_ADXI", 78565u, 0xBA5C01EE53C0A5EBull, 0x16a00u},
        {"SDRDRV.IRX", "SDRDRV", 9161u, 0xB23169607E57416Cull, 0x2d600u},
        {"MODHSYN.IRX", "MODHSYN", 63005u, 0x2BED58F73D56FEA6ull, 0x2f300u},
        {"MODSESQ.IRX", "MODSESQ", 13717u, 0xE5A70DE626ADB890ull, 0x3f900u},
        {"MODSEIN.IRX", "MODSEIN", 9393u, 0xC6EBBCB236E40CC3ull, 0x42500u},
        {"MODMIDI.IRX", "MODMIDI", 21941u, 0xA9774ECC5444FA5Eull, 0x44400u},
        {"AMSOUND.IRX", "AMSOUND", 25809u, 0x9D2FB49CA17EFE25ull, 0x48300u},
        {"MCMAN.IRX", "MCMAN", 103677u, 0x15CFC231D9C27FEEull, 0x5b600u},
        {"MCSERV.IRX", "MCSERV", 7713u, 0xF1FBF2EB3A277A15ull, 0x86500u},
        {"PADMAN.IRX", "PADMAN", 45925u, 0xBD159441567DDE8Eull, 0x89e00u},
    };
}
