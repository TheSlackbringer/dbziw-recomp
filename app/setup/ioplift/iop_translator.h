#pragma once
// DW5 009 v4.5 (015): translates an IOP module's code (R3000A, as relocated in IOP RAM) into C++ for
// ps2x::iop::IopNativeFrame (ps2xIOP/include/ps2x/iop/iop_native.h). Pure: no file access.

#include <cstdint>
#include <string>
#include <vector>

namespace dbziw::ioplift
{
    struct ModuleText
    {
        std::string name;           // module name, e.g. "CRI_ADXI"
        uint32_t base = 0;          // IOP RAM address of the first byte of `bytes`
        std::vector<uint8_t> bytes; // the module's .text after relocation
        std::vector<uint32_t> dataWords; // addresses of words in .text that are data, not code (exportTableWords)
    };

    struct TranslationStats
    {
        uint32_t words = 0;
        uint32_t nativeWords = 0;   // instructions with a native form (the rest stop for the interpreter)
        uint32_t importStubs = 0;
    };

    // The addresses of the words of the module's library export tables. An IRX keeps these inside .text: magic
    // 0x41C00000, a next pointer (0 in the file), a version word whose high half holds flags, an 8-byte name, then function
    // pointers up to a zero word. They are data: loadcore's SetRebootTimeLibraryHandlingMode writes those flags while the
    // module starts (PADMAN does, mode 2), which is not a change of its code.
    [[nodiscard]] std::vector<uint32_t> exportTableWords(const ModuleText &module);
    // C identifier for a module name ("CRI_ADXI" -> "CRI_ADXI", "sio2man" -> "sio2man").
    [[nodiscard]] std::string moduleIdentifier(const std::string &name);
    // One .cpp per module, defining `uint32_t iopn_run_<identifier>(ps2x::iop::IopNativeFrame &)`.
    [[nodiscard]] std::string translateModule(const ModuleText &module, TranslationStats *stats = nullptr);
    // The registry .cpp: a static ps2x::iop::IopNativeRegistrar for the modules.
    [[nodiscard]] std::string registrySource(const std::vector<ModuleText> &modules);
}
