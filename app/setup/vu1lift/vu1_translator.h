#pragma once
// VU1/VU0 translator (issues 011, 025): translates the VU microprograms in the player's own PAL executable into C++.
// The only translator since DW5 025 Step 6 (it was a port of the former Python tools/vu1_translator.py).
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace dbziw::vu1lift {
// Exact disc files referenced by the generation ranges (for semantic cache keys).
std::vector<std::filesystem::path> inputFiles(const std::filesystem::path &elf);

// Translates one VU1 program segment starting at entryPc in a 16 KiB microcode image (translate_segment).
std::string translateSegment(const std::vector<uint8_t> &code, uint32_t entryPc, const std::string &name,
                             bool staticTiming);

// Pairs reachable from entryPc (find_reachable, including the BAL/JR proof when the code has calls).
std::vector<uint32_t> findReachable(const std::vector<uint8_t> &code, uint32_t entryPc);

// The whole lifted VU1 file from the executable at `elf` (other disc files named in kSourceFiles are read from the
// same folder). `log` gets the
// "Translating ..." progress lines.
// staticTiming(name) mirrors DBZIW_VU1_STATIC (default: all segments).
// The output is split into source files of targetsPerPart programs each (0: one file), each with its own
// registration table (issue 012).
// Programs per generated file in tools/regen.sh and the setup (issue 012).
inline constexpr size_t kTargetsPerPart = 24;
bool generateLiftedVu1(const std::filesystem::path &elf, std::vector<std::string> &parts, std::string &error,
                       const std::function<void(const std::string &)> &log = {},
                       const std::function<bool(const std::string &)> &staticTiming = {},
                       size_t targetsPerPart = 0,
                       const std::function<void(size_t done, size_t total)> &counts = {});

// The 16 KiB VU1 code images the translated programs come from, rebuilt from the executable at `elf` (other disc
// files from the same folder) exactly as the translator reads them (load_disc_bins: executable, disc-file and range SHA-256
// checked), keyed by provenance key. Used by the vu1_lift test (DW5 036). false + `error` if a file is missing or
// does not match.
bool loadVu1Images(const std::filesystem::path &elf, std::map<std::string, std::vector<uint8_t>> &images,
                   std::string &error);

} // namespace dbziw::vu1lift
