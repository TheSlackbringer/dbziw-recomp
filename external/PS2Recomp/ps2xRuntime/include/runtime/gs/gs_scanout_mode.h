#pragma once

#include <cstdint>

// DBZIW 019: which TV system the scanout presents. On the console the BIOS's SetGsCrt programs SMODE1, whose CMOD
// field (bits 13-14) is 2 for NTSC and 3 for PAL (0 for VESA/DTV modes); the runtime's SetGsCrt sets it the same way
// (Kernel/Syscalls/System.cpp). The picture-height guess the scanout used before stays only for a request whose
// CMOD is neither, e.g. before any SetGsCrt.
namespace ps2x::gs
{
    [[nodiscard]] constexpr uint32_t smode1Cmod(uint64_t smode1) noexcept
    {
        return static_cast<uint32_t>((smode1 >> 13u) & 3u);
    }

    [[nodiscard]] constexpr bool scanoutIsPal(uint64_t smode1, bool heightSaysPal) noexcept
    {
        const uint32_t cmod = smode1Cmod(smode1);
        return cmod == 3u || (cmod != 2u && heightSaysPal);
    }
}
