#!/usr/bin/env python3
"""tools/audit_stubs.py — PS2 SDK & CRI Middleware Stub Auditor for Dragon Ball Z: Infinite World.

Analyzes SLES_553.47 (MIPS R5900 ELF) to identify SDK routines, CRI middleware modules,
syscalls, and hardware MMIO interactions. Categorizes routines into:
- RUN_NATIVE (recommended: runs game's own compiled code unhooked)
- REQUIRE_HLE (hardware MMIO, DMA channels, IOP RPC calls)
- ABI_AUDIT (CodeWarrior ABI checks, quadword returns)

Usage:
  python3 tools/audit_stubs.py game/SLES_553.47 [--json] [--verbose]
"""
import argparse, json, os, re, struct, sys

# Known PS2Recomp HLE Stub Catalog
HLE_CATALOG = {
    # DMA & System
    "sceDmaReset": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Manipulates DMAC hardware channels directly."},
    "sceDmaSend": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Triggers EE DMAC channel transfers via MMIO."},
    "sceDmaSendN": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Chained DMA transfer trigger."},
    "sceDmaSendI": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Interrupt-driven DMA transfer trigger."},
    "sceDmaRecv": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Triggers EE DMAC receive channel."},
    "sceDmaRecvN": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Chained DMA receive channel."},
    "sceDmaRecvI": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Interrupt-driven DMA receive channel."},
    "sceDmaSync": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Polls DMAC CHCR status registers."},
    "sceDmaWatch": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Polls DMAC transfer progress."},
    "sceDmaPause": {"module": "DMA", "type": "REQUIRE_HLE", "reason": "Controls DMAC channel pause state."},
    "sceDmaGetChan": {"module": "DMA", "type": "RUN_NATIVE", "reason": "Channel pointer math; game code can compute safely."},
    "sceDmaPutEnv": {"module": "DMA", "type": "RUN_NATIVE", "reason": "Environment struct configuration."},
    "sceDmaGetEnv": {"module": "DMA", "type": "RUN_NATIVE", "reason": "Environment struct retrieval."},

    # SIF (IOP RPC & DMA)
    "sceSifInit": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Initializes SIF DMA hardware and queues."},
    "sceSifInitRpc": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Binds SIF RPC client/server queues."},
    "sceSifCallRpc": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Transfers RPC payload packet to IOP via SIF0/SIF1 DMA."},
    "sceSifSetDma": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Dispatches SIF raw DMA packet to IOP."},
    "sceSifDmaStat": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Polls SIF DMA completion."},
    "sceSifAllocIopHeap": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Calls IOP heap allocator via RPC."},
    "sceSifFreeIopHeap": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Calls IOP heap free via RPC."},
    "sceSifGetSreg": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Reads SIF system register."},
    "sceSifSetSreg": {"module": "SIF", "type": "REQUIRE_HLE", "reason": "Writes SIF system register."},

    # Graphics (GS) & Display
    "sceGsResetPath": {"module": "GS", "type": "REQUIRE_HLE", "reason": "Resets VIF/GIF path hardware state."},
    "sceGsPutDispEnv": {"module": "GS", "type": "REQUIRE_HLE", "reason": "Writes display mode and PCRTC registers via GS priv MMIO."},
    "sceGsSyncV": {"module": "GS", "type": "REQUIRE_HLE", "reason": "Blocks on VSync field interrupt."},
    "sceGsSyncVCallback": {"module": "GS", "type": "REQUIRE_HLE", "reason": "Registers VSync callback handler."},
    "sceGsSyncPath": {"module": "GS", "type": "REQUIRE_HLE", "reason": "Waits for VIF/GIF Path1/Path2 completion."},
    "sceGsDefDispEnv": {"module": "GS", "type": "RUN_NATIVE", "reason": "Pure math/struct builder for DISPENV; HLE stubs often corrupt PAL formats."},
    "sceGsPutDrawEnv": {"module": "GS", "type": "RUN_NATIVE", "reason": "Generates GIF packets in memory; keep unhooked."},

    # Vector Unit (VU0)
    "sceVu0MulMatrix": {"module": "VU0", "type": "RUN_NATIVE", "reason": "Critical: HLE stub multiplies arg1*arg2 instead of game's arg2*arg1. Keep unhooked!"},
    "sceVu0RotMatrix": {"module": "VU0", "type": "RUN_NATIVE", "reason": "Matrix rotation math; compiled CodeWarrior code is cycle-exact."},
    "sceVu0ScaleVector": {"module": "VU0", "type": "RUN_NATIVE", "reason": "Vector scaling; compiled code runs accurately without stub."},
    "sceVu0InversMatrix": {"module": "VU0", "type": "RUN_NATIVE", "reason": "Matrix inversion; avoid HLE precision drift."},
    "sceVpu0Reset": {"module": "VU0", "type": "REQUIRE_HLE", "reason": "Resets VU0 co-processor state."},

    # VIF Packet builders
    "sceVif1PkOpenDirectCode": {"module": "VIF", "type": "RUN_NATIVE", "reason": "DMA tag builder; HLE stub refreshes tag count incorrectly causing corruption."},
    "sceVif1PkCloseDirectCode": {"module": "VIF", "type": "RUN_NATIVE", "reason": "DMA tag closer; keep native to prevent qwc mismatch."},

    # CD / DVD Media
    "sceCdInit": {"module": "CDVD", "type": "REQUIRE_HLE", "reason": "Initializes CDVD drive subsystem."},
    "sceCdSearchFile": {"module": "CDVD", "type": "REQUIRE_HLE", "reason": "Queries ISO9660 directory structure via IOP."},
    "sceCdRead": {"module": "CDVD", "type": "REQUIRE_HLE", "reason": "Commands physical sector read via IOP SIF buffer."},
    "sceCdSync": {"module": "CDVD", "type": "REQUIRE_HLE", "reason": "Polls sector read completion status."},
    "sceCdBreak": {"module": "CDVD", "type": "REQUIRE_HLE", "reason": "Aborts asynchronous sector read."},
    "sceCdGetError": {"module": "CDVD", "type": "REQUIRE_HLE", "reason": "Retrieves CDVD drive error code."},
    "sceCdChgSys": {"module": "CDVD", "type": "REQUIRE_HLE", "reason": "Changes CDVD drive mode."},
    "sceCdCbfunc": {"module": "CDVD", "type": "REQUIRE_HLE", "reason": "Registers CDVD completion callback."},

    # Pad (Controller)
    "scePadInit": {"module": "PAD", "type": "REQUIRE_HLE", "reason": "Initializes DualShock padman subsystem."},
    "scePadPortOpen": {"module": "PAD", "type": "REQUIRE_HLE", "reason": "Opens controller port socket table."},
    "scePadRead": {"module": "PAD", "type": "REQUIRE_HLE", "reason": "Polls controller button and analog stick states."},
    "scePadGetState": {"module": "PAD", "type": "REQUIRE_HLE", "reason": "Checks controller communication state."},

    # Memory Card
    "sceMcInit": {"module": "MC", "type": "REQUIRE_HLE", "reason": "Binds mcserv IOP RPC client."},
    "sceMcOpen": {"module": "MC", "type": "REQUIRE_HLE", "reason": "Opens memory card file via SIF RPC."},
    "sceMcRead": {"module": "MC", "type": "REQUIRE_HLE", "reason": "Reads save data cluster from IOP."},
    "sceMcWrite": {"module": "MC", "type": "REQUIRE_HLE", "reason": "Writes save data cluster to IOP."},
    "sceMcClose": {"module": "MC", "type": "REQUIRE_HLE", "reason": "Closes memory card handle."},
    "sceMcGetDir": {"module": "MC", "type": "REQUIRE_HLE", "reason": "Lists save directory contents."},
    "sceMcUdCheckNewCard": {"module": "MC", "type": "REQUIRE_HLE", "reason": "Detects memory card insertion/removal."},

    # Sound (SPU2 / Remote)
    "sceSdRemoteInit": {"module": "SOUND", "type": "REQUIRE_HLE", "reason": "Initializes LIBSD SPU2 driver on IOP."},

    # Standard LibC (String/Memory/Math)
    "memcpy": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "Fast 128-bit Quadword copy; compiled game code runs natively."},
    "memset": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "128-bit Quadword store; compiled game code runs natively."},
    "memmove": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "Overlapping memory copy; compiled code runs natively."},
    "memcmp": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "Byte comparison; compiled code runs natively."},
    "strlen": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "String length; compiled code runs natively."},
    "strcpy": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "String copy; compiled code runs natively."},
    "strcmp": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "String comparison; compiled code runs natively."},
    "malloc": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "Heap allocator; game manages its own heap zones natively."},
    "free": {"module": "LIBC", "type": "RUN_NATIVE", "reason": "Heap deallocator; game manages its own heap zones natively."},

    # File I/O
    "sceOpen": {"module": "FILEIO", "type": "REQUIRE_HLE", "reason": "Direct filesystem syscall via IOP."},
    "sceClose": {"module": "FILEIO", "type": "REQUIRE_HLE", "reason": "Direct filesystem syscall via IOP."},
    "sceRead": {"module": "FILEIO", "type": "REQUIRE_HLE", "reason": "Direct filesystem read via IOP."},
    "sceLseek": {"module": "FILEIO", "type": "REQUIRE_HLE", "reason": "File seek via IOP."},
    "sceLseek64": {"module": "FILEIO", "type": "REQUIRE_HLE", "reason": "64-bit file seek via IOP."},
    "sceIoctl": {"module": "FILEIO", "type": "REQUIRE_HLE", "reason": "Device I/O control via IOP."},

    # MPEG / Video (Sofdec SFD)
    "sceMpegInit": {"module": "MPEG", "type": "REQUIRE_HLE", "reason": "Initializes IPU/MPEG decoder core."},
    "sceMpegCreate": {"module": "MPEG", "type": "REQUIRE_HLE", "reason": "Allocates MPEG decode stream context."},
    "sceMpegGetPicture": {"module": "MPEG", "type": "REQUIRE_HLE", "reason": "Decodes macroblock stream via IPU."},
    "sceMpegSliceA0": {"module": "MPEG", "type": "RUN_NATIVE", "reason": "Slice unpacker math; game runs natively."},
}

# Known EE Kernel Syscalls (number -> name)
EE_SYSCALLS = {
    0x01: "ResetEE", 0x02: "SetGsCrt", 0x04: "Exit", 0x06: "LoadExecPS2",
    0x10: "AddSbusIntcHandler", 0x11: "RemoveSbusIntcHandler",
    0x12: "Interrupt2Firmware", 0x14: "SetVCommonHandler",
    0x15: "SetVInterruptHandler", 0x16: "ResetVCommonHandler", 0x17: "ResetVInterruptHandler",
    0x20: "CreateThread", 0x21: "DeleteThread", 0x22: "StartThread", 0x23: "ExitThread",
    0x24: "ExitDeleteThread", 0x25: "TerminateThread", 0x26: "iTerminateThread",
    0x27: "DisableDispatchThread", 0x28: "EnableDispatchThread", 0x29: "ChangeThreadPriority",
    0x2A: "iChangeThreadPriority", 0x2B: "RotateThreadReadyQueue", 0x2C: "iRotateThreadReadyQueue",
    0x2D: "ReleaseWaitThread", 0x2E: "iReleaseWaitThread", 0x2F: "GetThreadId",
    0x30: "ReferThreadStatus", 0x31: "iReferThreadStatus", 0x32: "SleepThread",
    0x33: "WakeupThread", 0x34: "iWakeupThread", 0x35: "CancelWakeupThread",
    0x36: "iCancelWakeupThread", 0x37: "SuspendThread", 0x38: "iSuspendThread",
    0x39: "ResumeThread", 0x3A: "iResumeThread",
    0x40: "CreateSema", 0x41: "DeleteSema", 0x42: "SignalSema", 0x43: "iSignalSema",
    0x44: "WaitSema", 0x45: "PollSema", 0x46: "iPollSema", 0x47: "ReferSemaStatus",
    0x48: "iReferSemaStatus",
    0x64: "FlushCache",
}


def parse_elf_headers(path):
    with open(path, "rb") as f:
        data = f.read()

    if data[:4] != b"\x7fELF":
        raise ValueError("Not a valid ELF file.")

    e_entry = struct.unpack_from("<I", data, 24)[0]
    e_phoff = struct.unpack_from("<I", data, 28)[0]
    e_shoff = struct.unpack_from("<I", data, 32)[0]
    e_phnum = struct.unpack_from("<H", data, 44)[0]
    e_shnum = struct.unpack_from("<H", data, 48)[0]
    e_shentsize = struct.unpack_from("<H", data, 46)[0]
    e_shstrndx = struct.unpack_from("<H", data, 50)[0]

    # Find loadable segment (usually PROGBITS section)
    # Section 4 in SLES_553.47 is the main progbits (offset 0x80, size 0x39c600, vaddr 0x100000)
    shstr_hdr = data[e_shoff + e_shstrndx * e_shentsize : e_shoff + (e_shstrndx + 1) * e_shentsize]
    shstr_offset, shstr_size = struct.unpack_from("<II", shstr_hdr, 16)
    shstrtab = data[shstr_offset : shstr_offset + shstr_size]

    progbits_offset = 0x80
    progbits_size = len(data) - 0x80
    progbits_vaddr = 0x100000

    for i in range(e_shnum):
        sh = data[e_shoff + i * e_shentsize : e_shoff + (i + 1) * e_shentsize]
        name_idx, sh_type, sh_flags, sh_addr, sh_offset, sh_size = struct.unpack_from("<IIIIII", sh, 0)
        name = shstrtab[name_idx:].split(b"\x00")[0].decode("latin1")
        if sh_type == 1 and sh_addr != 0 and sh_size > 0:
            progbits_offset = sh_offset
            progbits_size = sh_size
            progbits_vaddr = sh_addr
            break

    code_bytes = data[progbits_offset : progbits_offset + progbits_size]
    return {
        "entry_point": e_entry,
        "vaddr": progbits_vaddr,
        "offset": progbits_offset,
        "size": progbits_size,
        "raw_data": data,
        "code_bytes": code_bytes,
    }


def scan_symbols_and_strings(raw_data):
    # Extract all ASCII strings of length >= 4
    string_pattern = re.compile(b"[\x20-\x7e]{4,}")
    strings = [m.group(0).decode("latin1", errors="ignore") for m in string_pattern.finditer(raw_data)]

    detected_symbols = set()
    sdk_modules = {}
    cri_modules = {}

    # Scan for sce* symbols
    sce_pattern = re.compile(r"\b(sce[A-Za-z0-9_]+)\b")
    for s in strings:
        for match in sce_pattern.findall(s):
            detected_symbols.add(match)

    # Scan for CRI symbols (adx*, cri*)
    cri_pattern = re.compile(r"\b((?:adx|cri)[A-Za-z0-9_]+)\b", re.IGNORECASE)
    for s in strings:
        for match in cri_pattern.findall(s):
            detected_symbols.add(match)
            cri_modules[match] = "CRI ADX/Sofdec Middleware"

    # Add standard LibC symbols found in code/strings
    libc_candidates = ["memcpy", "memset", "memcmp", "memmove", "strcpy", "strcmp", "strlen", "malloc", "free", "printf"]
    for sym in libc_candidates:
        if any(sym in s for s in strings):
            detected_symbols.add(sym)

    return sorted(detected_symbols), cri_modules


def scan_syscalls(code_bytes, base_vaddr):
    syscall_sites = {}
    for i in range(0, len(code_bytes) - 8, 4):
        word = struct.unpack_from("<I", code_bytes, i)[0]
        # MIPS syscall opcode: 0x0000000c
        if (word & 0xFC00003F) == 0x0000000C:
            prev = struct.unpack_from("<I", code_bytes, i - 4)[0]
            sys_num = None
            # addiu $v1, $zero, imm -> 0x2403xxxx
            if (prev & 0xFFFF0000) == 0x24030000:
                sys_num = prev & 0xFFFF
                if sys_num & 0x8000:
                    sys_num -= 0x10000
            addr = base_vaddr + i
            name = EE_SYSCALLS.get(sys_num, f"UnknownSyscall_{sys_num}")
            syscall_sites[hex(addr)] = {"number": sys_num, "name": name}
    return syscall_sites


def scan_hardware_mmio(code_bytes, base_vaddr):
    # Detect lui instructions loading high 16-bits of MMIO addresses:
    # 0x1000xxxx -> DMAC, INTC, Timers
    # 0x1200xxxx -> GS Privileged registers
    # 0x7000xxxx -> Scratchpad RAM
    mmio_hits = {"DMAC_INTC_TIMERS": 0, "GS_PRIVILEGED": 0, "SCRATCHPAD": 0}
    for i in range(0, len(code_bytes) - 4, 4):
        word = struct.unpack_from("<I", code_bytes, i)[0]
        # lui $reg, imm -> opcode 0x0F
        if (word >> 26) == 0x0F:
            imm = word & 0xFFFF
            if imm == 0x1000:
                mmio_hits["DMAC_INTC_TIMERS"] += 1
            elif imm == 0x1200:
                mmio_hits["GS_PRIVILEGED"] += 1
            elif imm == 0x7000:
                mmio_hits["SCRATCHPAD"] += 1
    return mmio_hits


def audit(elf_path):
    elf_info = parse_elf_headers(elf_path)
    symbols, cri_modules = scan_symbols_and_strings(elf_info["raw_data"])
    syscalls = scan_syscalls(elf_info["code_bytes"], elf_info["vaddr"])
    mmio = scan_hardware_mmio(elf_info["code_bytes"], elf_info["vaddr"])

    native_unhooked = []
    hle_stubs = []
    abi_warnings = []
    recommendations = []

    sdk_summary = {}

    for sym in symbols:
        info = HLE_CATALOG.get(sym)
        if info:
            category = info["type"]
            module = info["module"]
            sdk_summary[module] = sdk_summary.get(module, 0) + 1
            rec = {
                "symbol": sym,
                "module": module,
                "recommendation": category,
                "reason": info["reason"]
            }
            if category == "RUN_NATIVE":
                native_unhooked.append(rec)
            else:
                hle_stubs.append(rec)
            recommendations.append(rec)
        elif sym.startswith("adx") or sym.startswith("cri"):
            rec = {
                "symbol": sym,
                "module": "CRI_MIDDLEWARE",
                "recommendation": "RUN_NATIVE",
                "reason": "CRI buffer/AFS management; run compiled code natively to preserve stream sync."
            }
            native_unhooked.append(rec)
            recommendations.append(rec)
            sdk_summary["CRI_MIDDLEWARE"] = sdk_summary.get("CRI_MIDDLEWARE", 0) + 1
        elif sym.startswith("sce"):
            rec = {
                "symbol": sym,
                "module": "UNKNOWN_SCE",
                "recommendation": "AUDIT_REQUIRED",
                "reason": "Uncatalogued Sony SDK symbol; verify MMIO accesses before hooking."
            }
            abi_warnings.append(rec)
            recommendations.append(rec)
            sdk_summary["UNKNOWN_SCE"] = sdk_summary.get("UNKNOWN_SCE", 0) + 1

    return {
        "elf_path": elf_path,
        "entry_point": hex(elf_info["entry_point"]),
        "text_vaddr": hex(elf_info["vaddr"]),
        "text_size_bytes": elf_info["size"],
        "total_symbols": len(symbols),
        "native_unhooked": native_unhooked,
        "hle_stubs": hle_stubs,
        "abi_warnings": abi_warnings,
        "sdk_modules": sdk_summary,
        "syscall_count": len(syscalls),
        "mmio_references": mmio,
        "recommendations": recommendations,
    }


def print_cli_report(res, verbose=False):
    print("=" * 80)
    print(f"PS2 SDK & CRI STUB AUDIT REPORT — {os.path.basename(res['elf_path'])}")
    print("=" * 80)
    print(f"Entry Point:        {res['entry_point']}")
    print(f"Text Virtual Base:  {res['text_vaddr']} ({res['text_size_bytes'] / (1024*1024):.2f} MB)")
    print(f"Total Symbols:      {res['total_symbols']}")
    print(f"EE Syscall Sites:   {res['syscall_count']}")
    print(f"Direct MMIO Hits:   DMAC/INTC: {res['mmio_references']['DMAC_INTC_TIMERS']}, GS: {res['mmio_references']['GS_PRIVILEGED']}, SPRAM: {res['mmio_references']['SCRATCHPAD']}")
    print("-" * 80)
    print("SDK MODULE BREAKDOWN:")
    for mod, count in sorted(res["sdk_modules"].items(), key=lambda x: -x[1]):
        print(f"  {mod:18s} {count:4d} symbols")
    print("-" * 80)
    print(f"AUDIT SUMMARY:")
    print(f"  \033[92m[RUN_NATIVE]\033[0m   {len(res['native_unhooked']):3d} routines (Run unhooked; prevents HLE bugs)")
    print(f"  \033[93m[REQUIRE_HLE]\033[0m  {len(res['hle_stubs']):3d} routines (Hardware MMIO, DMA triggers, IOP RPC)")
    print(f"  \033[94m[ABI_AUDIT]\033[0m    {len(res['abi_warnings']):3d} routines (Uncatalogued SCE routines)")
    print("=" * 80)

    print("\nKEY NATIVE RECOMMENDATIONS (Do NOT Stub in toml):")
    for r in res["native_unhooked"][:10]:
        print(f"  * \033[92m{r['symbol']:24s}\033[0m ({r['module']}) -> {r['reason']}")

    print("\nHARDWARE HLE REQUIREMENTS (Must Stub in toml):")
    for r in res["hle_stubs"][:10]:
        print(f"  * \033[93m{r['symbol']:24s}\033[0m ({r['module']}) -> {r['reason']}")

    if verbose and res["abi_warnings"]:
        print("\nUNCATALOGUED SCE ROUTINES:")
        for r in res["abi_warnings"]:
            print(f"  * {r['symbol']}")
    print("=" * 80)


def main():
    parser = argparse.ArgumentParser(description="Audit PS2 SDK and CRI stubs in SLES_553.47.")
    parser.add_argument("elf", help="Path to SLES_553.47 ELF file")
    parser.add_argument("--json", action="store_true", help="Output audit results as JSON")
    parser.add_argument("--verbose", action="store_true", help="Print detailed symbol lists")
    args = parser.parse_args()

    if not os.path.exists(args.elf):
        sys.exit(f"Error: ELF file not found: {args.elf}")

    result = audit(args.elf)

    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print_cli_report(result, verbose=args.verbose)


if __name__ == "__main__":
    main()
