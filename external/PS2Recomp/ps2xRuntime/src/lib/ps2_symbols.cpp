#include "runtime/ps2_symbols.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>

GuestSymbolResolver &GuestSymbolResolver::instance()
{
    static GuestSymbolResolver s_instance;
    return s_instance;
}

void GuestSymbolResolver::ensureLoaded()
{
    if (m_loaded)
        return;
    m_loaded = true;

    // 1. Check environment variable PS2X_SYMBOLS_PATH
    const char *envPath = std::getenv("PS2X_SYMBOLS_PATH");
    if (envPath && *envPath)
    {
        if (loadFromCsv(envPath))
            return;
    }

    // 2. Check recomp/ghidra_functions.csv and relative paths
    const std::vector<std::filesystem::path> searchPaths = {
        "recomp/ghidra_functions.csv",
        "../recomp/ghidra_functions.csv",
        "ghidra_functions.csv"
    };

    for (const auto &p : searchPaths)
    {
        if (std::filesystem::exists(p))
        {
            if (loadFromCsv(p))
                return;
        }
    }
}

bool GuestSymbolResolver::loadFromCsv(const std::filesystem::path &csvPath)
{
    std::ifstream file(csvPath);
    if (!file.is_open())
        return false;

    std::string line;
    // Skip header line if present
    if (std::getline(file, line))
    {
        if (line.find("Name") == std::string::npos && line.find("Start") == std::string::npos)
        {
            // First line was not a header, reset
            file.clear();
            file.seekg(0);
        }
    }

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::stringstream ss(line);
        std::string name, startStr, endStr, sizeStr;

        if (!std::getline(ss, name, ','))
            continue;
        if (!std::getline(ss, startStr, ','))
            continue;
        if (!std::getline(ss, endStr, ','))
            continue;
        std::getline(ss, sizeStr, ',');

        auto parseHexOrDec = [](const std::string &s) -> uint32_t {
            if (s.empty()) return 0;
            if (s.size() > 2 && (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')))
                return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
            return static_cast<uint32_t>(std::stoul(s, nullptr, 10));
        };

        try
        {
            uint32_t start = parseHexOrDec(startStr);
            uint32_t end = parseHexOrDec(endStr);
            uint32_t size = sizeStr.empty() ? (end >= start ? end - start : 0) : parseHexOrDec(sizeStr);
            if (end == 0 && size > 0)
                end = start + size;

            m_symbols.push_back({name, start, end, size});
        }
        catch (...)
        {
            // Skip malformed lines
        }
    }

    std::sort(m_symbols.begin(), m_symbols.end(), [](const GuestSymbol &a, const GuestSymbol &b) {
        return a.start < b.start;
    });

    return !m_symbols.empty();
}

void GuestSymbolResolver::addSymbol(const std::string &name, uint32_t start, uint32_t end)
{
    m_symbols.push_back({name, start, end, end >= start ? end - start : 0});
    std::sort(m_symbols.begin(), m_symbols.end(), [](const GuestSymbol &a, const GuestSymbol &b) {
        return a.start < b.start;
    });
}

const GuestSymbol *GuestSymbolResolver::find(uint32_t address) const
{
    const_cast<GuestSymbolResolver *>(this)->ensureLoaded();
    if (m_symbols.empty())
        return nullptr;

    auto it = std::upper_bound(m_symbols.begin(), m_symbols.end(), address,
                               [](uint32_t addr, const GuestSymbol &sym) {
                                   return addr < sym.start;
                               });

    if (it != m_symbols.begin())
    {
        --it;
        if (address >= it->start && (it->end == 0 || address < it->end))
            return &(*it);
    }

    return nullptr;
}

std::string GuestSymbolResolver::resolve(uint32_t address) const
{
    const GuestSymbol *sym = find(address);
    if (sym)
    {
        uint32_t offset = address - sym->start;
        if (offset == 0)
            return sym->name;
        std::stringstream ss;
        ss << sym->name << "+0x" << std::hex << offset;
        return ss.str();
    }

    std::stringstream ss;
    ss << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << address;
    return ss.str();
}

void GuestSymbolResolver::clear()
{
    m_symbols.clear();
    m_loaded = false;
}
