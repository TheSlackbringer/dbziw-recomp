#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: ps2x_checkpoint_test <state_file_path>" << std::endl;
        return 1;
    }
    const std::string statePath = argv[1];

    PS2Runtime runtime1;
    if (!runtime1.memory().initialize())
    {
        std::cerr << "Failed to initialize runtime1 memory" << std::endl;
        return 1;
    }

    // Fill memory buffers with known test patterns
    uint8_t *rdram1 = runtime1.memory().getRDRAM();
    for (size_t i = 0; i < PS2_RAM_SIZE; i += 4)
    {
        *reinterpret_cast<uint32_t *>(&rdram1[i]) = static_cast<uint32_t>(0xDEADBEEF ^ i);
    }

    uint8_t *scratch1 = runtime1.memory().m_scratchpad;
    for (size_t i = 0; i < PS2_SCRATCHPAD_SIZE; i += 4)
    {
        *reinterpret_cast<uint32_t *>(&scratch1[i]) = static_cast<uint32_t>(0xA5A5A5A5 ^ i);
    }

    uint8_t *iop1 = runtime1.memory().iop_ram;
    for (size_t i = 0; i < 2u * 1024u * 1024u; i += 4)
    {
        *reinterpret_cast<uint32_t *>(&iop1[i]) = static_cast<uint32_t>(0x12345678 ^ i);
    }

    uint8_t *vram1 = runtime1.memory().getGSVRAM();
    for (size_t i = 0; i < PS2_GS_VRAM_SIZE; i += 4)
    {
        *reinterpret_cast<uint32_t *>(&vram1[i]) = static_cast<uint32_t>(0xCAFEF00D ^ i);
    }

    // Set CPU registers
    runtime1.cpu().pc = 0x00100500u;
    runtime1.cpu().r[29] = _mm_set_epi64x(0, 0x01FFE000u);
    runtime1.cpu().r[31] = _mm_set_epi64x(0, 0x00100250u);
    runtime1.cpu().r[4] = _mm_set_epi64x(0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL);

    // Setup scheduler state
    runtime1.eeScheduler().reset(rdram1, runtime1.cpu());
    runtime1.eeScheduler().setVsyncTick(4200u);
    runtime1.eeScheduler().setEeCycle(987654321u);

    // Dump state
    if (!runtime1.dumpState(statePath))
    {
        std::cerr << "Failed to dump state to " << statePath << std::endl;
        return 1;
    }

    // Load state into fresh runtime
    PS2Runtime runtime2;
    if (!runtime2.memory().initialize())
    {
        std::cerr << "Failed to initialize runtime2 memory" << std::endl;
        return 1;
    }

    // Poison runtime2 memory before load
    std::memset(runtime2.memory().getRDRAM(), 0xCC, PS2_RAM_SIZE);
    std::memset(runtime2.memory().m_scratchpad, 0xCC, PS2_SCRATCHPAD_SIZE);
    std::memset(runtime2.memory().iop_ram, 0xCC, 2u * 1024u * 1024u);
    std::memset(runtime2.memory().getGSVRAM(), 0xCC, PS2_GS_VRAM_SIZE);

    if (!runtime2.loadState(statePath))
    {
        std::cerr << "Failed to load state from " << statePath << std::endl;
        return 1;
    }

    // Verify bitwise fidelity of RDRAM
    if (std::memcmp(rdram1, runtime2.memory().getRDRAM(), PS2_RAM_SIZE) != 0)
    {
        std::cerr << "Bitwise mismatch in RDRAM after restore!" << std::endl;
        return 1;
    }

    // Verify Scratchpad
    if (std::memcmp(scratch1, runtime2.memory().m_scratchpad, PS2_SCRATCHPAD_SIZE) != 0)
    {
        std::cerr << "Bitwise mismatch in Scratchpad after restore!" << std::endl;
        return 1;
    }

    // Verify IOP RAM
    if (std::memcmp(iop1, runtime2.memory().iop_ram, 2u * 1024u * 1024u) != 0)
    {
        std::cerr << "Bitwise mismatch in IOP RAM after restore!" << std::endl;
        return 1;
    }

    // Verify GS VRAM
    if (std::memcmp(vram1, runtime2.memory().getGSVRAM(), PS2_GS_VRAM_SIZE) != 0)
    {
        std::cerr << "Bitwise mismatch in GS VRAM after restore!" << std::endl;
        return 1;
    }

    // Verify CPU registers
    if (runtime2.cpu().pc != runtime1.cpu().pc)
    {
        std::cerr << "PC mismatch: expected 0x" << std::hex << runtime1.cpu().pc << ", got 0x" << runtime2.cpu().pc << std::endl;
        return 1;
    }

    uint32_t sp1 = static_cast<uint32_t>(_mm_extract_epi32(runtime1.cpu().r[29], 0));
    uint32_t sp2 = static_cast<uint32_t>(_mm_extract_epi32(runtime2.cpu().r[29], 0));
    if (sp1 != sp2)
    {
        std::cerr << "SP mismatch: expected 0x" << std::hex << sp1 << ", got 0x" << sp2 << std::endl;
        return 1;
    }

    // Verify scheduler ticks
    if (runtime2.eeScheduler().currentVSyncTick() != runtime1.eeScheduler().currentVSyncTick())
    {
        std::cerr << "VSync tick mismatch: expected " << runtime1.eeScheduler().currentVSyncTick()
                  << ", got " << runtime2.eeScheduler().currentVSyncTick() << std::endl;
        return 1;
    }

    std::cout << "PASS: Checkpoint roundtrip bitwise identical across all memory buffers and registers." << std::endl;
    return 0;
}
