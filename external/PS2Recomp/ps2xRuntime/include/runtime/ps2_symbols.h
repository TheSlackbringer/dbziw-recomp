#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

struct GuestSymbol
{
    std::string name;
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t size = 0;
};

class GuestSymbolResolver
{
public:
    static GuestSymbolResolver &instance();

    bool loadFromCsv(const std::filesystem::path &csvPath);
    void addSymbol(const std::string &name, uint32_t start, uint32_t end);
    [[nodiscard]] std::string resolve(uint32_t address) const;
    [[nodiscard]] const GuestSymbol *find(uint32_t address) const;
    [[nodiscard]] size_t count() const { return m_symbols.size(); }
    void clear();

private:
    GuestSymbolResolver() = default;
    void ensureLoaded();

    std::vector<GuestSymbol> m_symbols;
    bool m_loaded = false;
};
