// dbziw_vu1lift <SLES_553.47> <output dir>: writes the lifted VU1 code (dbziw_vu1_lifted_NN.cpp) from the player's own executable (issue 012;
// DW5 025 Step 6). DBZIW starts no VU0 microprograms (012 M0), so DW5's --vu0 mode is left out.
// DBZIW_VU1_STATIC ('all' default, 'none', or a comma list) selects static-timing segments.
//
// dbziw_vu1lift --find-range <census image .bin> <entry pc> <name> [disc file ...]: locates a VU1 program's reachable
// code in the player's own disc files (default game/SLES_553.47) and prints the entries to add to
// app/setup/vu1lift/vu1_inputs.h (kChunks/kRanges, kImageRegistrations, kTargets): offsets, sizes and a SHA256
// only, never instruction bytes. The image comes from a local PS2X_VU1_CENSUS run (never commit it). Each contiguous
// reachable run is matched greedily (longest prefix found) in the given files.
#include "vu1_translator.h"
#include "../sha256.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string_view>

namespace
{
    std::string readFile(const char *path)
    {
        std::ifstream in(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

    uint64_t fnv1a64(const std::string &data)
    {
        uint64_t h = 1469598103934665603ull;
        for (unsigned char b : data)
            h = (h ^ b) * 1099511628211ull;
        return h;
    }

    int findRange(int argc, char **argv)
    {
        if (argc < 5)
        {
            std::fprintf(stderr, "usage: %s --find-range <census image .bin> <entry pc> <name> [disc file ...]\n",
                         argv[0]);
            return 2;
        }
        const std::string image = readFile(argv[2]);
        if (image.size() != 16384)
        {
            std::fprintf(stderr, "dbziw_vu1lift: %s is not a 16 KiB VU1 image\n", argv[2]);
            return 1;
        }
        const uint32_t entry = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 0));
        const std::string name = argv[4];
        struct Source { std::string label; std::string blob; }; // label empty: the executable
        std::vector<Source> sources;
        std::vector<std::string> files;
        for (int i = 5; i < argc; ++i)
            files.push_back(argv[i]);
        if (files.empty())
            files.push_back("game/SLES_553.47");
        for (const auto &f : files)
        {
            const std::string base = std::filesystem::path(f).filename().string();
            sources.push_back({base == "SLES_553.47" ? std::string() : base, readFile(f.c_str())});
        }
        const std::vector<uint8_t> code(image.begin(), image.end());
        const std::vector<uint32_t> reachable = dbziw::vu1lift::findReachable(code, entry);
        std::vector<std::pair<uint32_t, uint32_t>> runs;
        for (uint32_t pc : reachable)
        {
            if (!runs.empty() && runs.back().second == pc)
                runs.back().second = pc + 8;
            else
                runs.push_back({pc, pc + 8});
        }
        struct Chunk { uint32_t pc, offset, size; std::string label; };
        std::vector<Chunk> chunks;
        for (const auto &[a, b] : runs)
            for (uint32_t pc = a; pc < b;)
            {
                bool found = false;
                Chunk best{};
                for (const auto &src : sources)
                {
                    int lo = 8, hi = int(b - pc);
                    while (lo <= hi)
                    {
                        int mid = (lo + hi) / 16 * 8;
                        if (mid == 0)
                            mid = 8;
                        const size_t at = src.blob.find(std::string_view(image).substr(pc, size_t(mid)));
                        if (at != std::string::npos)
                        {
                            if (!found || uint32_t(mid) > best.size)
                                best = {pc, uint32_t(at), uint32_t(mid), src.label};
                            found = true;
                            lo = mid + 8;
                        }
                        else
                            hi = mid - 8;
                    }
                }
                if (!found)
                {
                    std::fprintf(stderr, "dbziw_vu1lift: %s: bytes at VU pc 0x%04x not found in the disc files\n",
                                 name.c_str(), pc);
                    return 1;
                }
                chunks.push_back(best);
                pc += best.size;
            }
        std::string rebuilt(16384, '\0');
        for (const auto &c : chunks)
            for (const auto &src : sources)
                if (src.label == c.label)
                    rebuilt.replace(c.pc, c.size, src.blob, c.offset, c.size);
        const uint32_t end = reachable.back();
        const uint64_t imageHash = fnv1a64(image);
        char key[17];
        std::snprintf(key, sizeof(key), "%016llx", static_cast<unsigned long long>(imageHash));
        const std::string digest = dbziw::setup::sha256Hex(reinterpret_cast<const uint8_t *>(rebuilt.data()) + entry,
                                                           end + 8 - entry);
        std::printf("kChunks entry (number it after the last kChunksN in vu1_inputs.h):\n"
                    "inline constexpr Chunk kChunksN[] = {");
        for (size_t i = 0; i < chunks.size(); ++i)
            std::printf("%s{%uu, %uu, %uu, %s}", i ? ", " : "", chunks[i].pc, chunks[i].offset, chunks[i].size,
                        chunks[i].label.empty() ? "nullptr" : ("\"" + chunks[i].label + "\"").c_str());
        std::printf("};\nkRanges entry:\n    {\"%s\", %uu, %uu, \"%s\", kChunksN, %zuu, \"%s\"},\n", key, entry, end,
                    name.c_str(), chunks.size(), digest.c_str());
        std::printf("kImageRegistrations entry:\n    {0x%sull, %uu, \"%s\"},\n", key, entry, name.c_str());
        std::printf("kTargets entry:\n    {\"%s\", %uu, %uu, \"%s\"},\n", key, entry, end, name.c_str());
        return 0;
    }
}

int main(int argc, char **argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "--find-range") == 0)
        return findRange(argc, argv);
    if (argc != 3)
    {
        std::fprintf(stderr, "usage: %s <SLES_553.47> <output dir>\n"
                             "       %s --find-range <census image .bin> <entry pc> <name> [disc file ...]\n",
                     argv[0], argv[0]);
        return 2;
    }
    const char *env = std::getenv("DBZIW_VU1_STATIC");
    const std::string selection = env ? env : "all";
    auto staticTiming = [&](const std::string &name) {
        if (selection == "all")
            return true;
        std::stringstream list(selection);
        for (std::string item; std::getline(list, item, ',');)
            if (item == name)
                return true;
        return false;
    };
    std::vector<std::string> parts;
    std::string error;
    const auto print = [](const std::string &line) { std::printf("%s\n", line.c_str()); };
    if (!dbziw::vu1lift::generateLiftedVu1(argv[1], parts, error, print, staticTiming, dbziw::vu1lift::kTargetsPerPart))
    {
        std::fprintf(stderr, "dbziw_vu1lift: %s\n", error.c_str());
        return 1;
    }
    // Issue 012: <output dir>/dbziw_vu1_lifted_NN.cpp, one file per part; a file is rewritten only when its text
    // changed (keeps builds incremental), and parts a smaller program list no longer has are removed.
    const std::filesystem::path dir = argv[2];
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::set<std::string> written;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        char name[64];
        std::snprintf(name, sizeof(name), "dbziw_vu1_lifted_%02zu.cpp", i);
        const std::filesystem::path path = dir / name;
        written.insert(name);
        if (std::filesystem::exists(path) && readFile(path.string().c_str()) == parts[i])
            continue;
        std::ofstream out(path, std::ios::binary);
        out << parts[i];
        if (!out.flush())
        {
            std::fprintf(stderr, "dbziw_vu1lift: cannot write %s\n", path.string().c_str());
            return 1;
        }
    }
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec))
    {
        const std::string name = entry.path().filename().string();
        if (name.rfind("dbziw_vu1_lifted", 0) == 0 && entry.path().extension() == ".cpp" && !written.count(name))
            std::filesystem::remove(entry.path(), ec);
    }
    std::printf("Wrote %zu files to %s\n", parts.size(), dir.string().c_str());
    return 0;
}
