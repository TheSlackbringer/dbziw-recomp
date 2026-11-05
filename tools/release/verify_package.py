#!/usr/bin/env python3
"""Check an offline builder's contents and run its tools without game data."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def validate_linux_icon(root):
    # appimagetool derives .DirIcon only from <Icon>.png or .xpm in the AppDir
    # root (not .svg) and refuses to build without one.
    icon = root / "dbziw.png"
    if not icon.is_file() or icon.read_bytes()[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("AppDir root needs the PNG icon dbziw.png for .DirIcon (an SVG is not accepted)")


def package_payload(root):
    root = Path(root)
    nested = root / "dbziw-data/package"
    return nested if (nested / "app/CMakeLists.txt").is_file() else root


def validate_package(root, windows):
    outer = Path(root)
    root = package_payload(outer)
    required = ["BUILD_ID", "app/CMakeLists.txt", "recomp/dbz_iw.toml", "external/PS2Recomp/ps2xRuntime/CMakeLists.txt",
                "app/deps/imgui-src/misc/fonts/Roboto-Medium.ttf"]
    if windows:
        required += [
            "launcher/DBZIW.exe", "launcher/dbziw-uninstall.exe", "package-files.txt", "tools/ps2_recomp.exe",
            "tools/cmake/bin/cmake.exe", "tools/ninja.exe", "tools/ffmpeg.zip", "tools/windows/llvm-mingw.cmake",
            "tools/llvm-mingw/bin/x86_64-w64-mingw32-clang.exe",
            "tools/llvm-mingw/bin/x86_64-w64-mingw32-clang++.exe",
            "tools/llvm-mingw/bin/llvm-ar.exe", "tools/llvm-mingw/bin/llvm-ranlib.exe",
        ]
    else:
        required += [
            "README.txt", "usr/bin/dbziw_setup", "usr/bin/dbziw-uninstall", "tools/ps2_recomp",
            "tools/cmake/bin/cmake", "tools/ninja", "tools/linux/clang.cmake",
            "tools/clang/bin/clang", "tools/clang/bin/clang++",
            "tools/clang/bin/llvm-ar", "tools/clang/bin/llvm-ranlib",
            "tools/clang/bin/ld.lld", "tools/ffmpeg/include/libavcodec/avcodec.h",
        ]
    missing = [name for name in required if not (root / name).is_file()]
    if windows:
        missing += [name for name in ("DBZIW.exe", "README.txt") if not (outer / name).is_file()]
    if missing:
        raise SystemExit("incomplete builder package; required files missing:\n  " +
                         "\n  ".join(missing))


def package_environment(root, windows):
    root = package_payload(root)
    tools = root / "tools"
    env = os.environ.copy()
    # The release compiler must not inherit target headers/libraries from the host.
    for variable in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH",
                     "PKG_CONFIG_PATH", "CMAKE_PREFIX_PATH", "PYTHONPATH", "PYTHONHOME"):
        env.pop(variable, None)
    compiler_root = tools / ("llvm-mingw" if windows else "clang")
    if windows:
        system = Path(os.environ["SystemRoot"])
        env["PATH"] = os.pathsep.join(map(str, [
            compiler_root / "bin", tools, system / "System32", system,
        ]))
    else:
        env["PATH"] = f"{compiler_root}/bin:/usr/bin:/bin"
        env["LD_LIBRARY_PATH"] = ":".join(map(str, [
            tools, root / "usr/lib",
        ]))
    return env


def smoke_staged_uninstaller(helper, scratch):
    staged = scratch / helper.name
    shutil.copy2(helper, staged)
    env = os.environ.copy()
    env.pop("LD_LIBRARY_PATH", None)
    result = subprocess.run([staged], cwd=scratch, env=env, timeout=10)
    if result.returncode != 2:
        raise SystemExit(f"staged uninstall helper did not start correctly: exit {result.returncode}")
    print("PASS: staged uninstall helper runs without packaged libraries", flush=True)


def smoke_package(root, windows):
    if windows != (os.name == "nt"):
        raise SystemExit("native tool smoke checks must run on the target operating system")
    outer = Path(root).resolve()
    root = package_payload(outer)
    tools = root / "tools"
    env = package_environment(root, windows)
    compiler_root = tools / ("llvm-mingw" if windows else "clang")
    suffix = ".exe" if windows else ""
    cmake = tools / f"cmake/bin/cmake{suffix}"

    def run(command, cwd=root):
        print("+", " ".join(map(str, command)), flush=True)
        subprocess.run(list(map(str, command)), cwd=cwd, env=env, check=True, timeout=300)

    run([root / ("launcher/DBZIW.exe" if windows else "usr/bin/dbziw_setup"), "--help"])
    # The recompiler has no --help: with no profile it prints its usage and exits 1, and that proves it loads with its libraries.
    usage = subprocess.run([str(tools / f"ps2_recomp{suffix}")], cwd=root, env=env, capture_output=True, text=True, timeout=60)
    if usage.returncode != 1 or "usage" not in (usage.stdout + usage.stderr).lower():
        raise SystemExit(f"the packaged recompiler did not start: exit {usage.returncode}")
    run([cmake, "--version"])
    run([tools / f"ninja{suffix}", "--version"])
    # Configure through the actual packaged toolchain, exercising its compiler,
    # headers, linker, CRT and runtime dependencies without any retail game files.
    with tempfile.TemporaryDirectory(prefix="dbziw-package-smoke-") as directory:
        scratch = Path(directory)
        helper = root / ("launcher/dbziw-uninstall.exe" if windows else "usr/bin/dbziw-uninstall")
        smoke_staged_uninstaller(helper, scratch)
        source, build = scratch / "source", scratch / "build"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            "project(package_smoke LANGUAGES C CXX)\n"
            "set(CMAKE_CXX_STANDARD 23)\n"
            "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
            "file(GLOB smoke_sources CONFIGURE_DEPENDS main.cpp)\n"
            "add_executable(package_smoke main.cpp)\n", encoding="utf-8")
        (source / "main.cpp").write_text(
            '#include <filesystem>\n#include <iostream>\n#include <vector>\n'
            'int main() { std::vector<int> v{1,2,3}; '
            'std::cout << "DBZIW package tools OK " << v.size() << "\\n"; '
            'return !std::filesystem::exists("."); }\n', encoding="utf-8")
        toolchain = tools / ("windows/llvm-mingw.cmake" if windows else "linux/clang.cmake")
        root_arg = "LLVM_MINGW_ROOT" if windows else "DBZIW_LINUX_TOOLCHAIN_ROOT"
        run([cmake, "-S", source, "-B", build, "-G", "Ninja",
             f"-DCMAKE_MAKE_PROGRAM={tools / ('ninja' + suffix)}",
             f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
             f"-D{root_arg}={compiler_root}", "-DCMAKE_BUILD_TYPE=Release"])
        run([cmake, "--build", build, "--target", "package_smoke"])
        run([build / ("package_smoke" + suffix)], cwd=scratch)
    print("PASS: packaged translator and offline build tools", flush=True)


def build_linux_sdk(root, build=None):
    """Rebuild the packaged launcher with its own exported compiler and sysroot."""
    if os.name == "nt":
        raise SystemExit("the Linux SDK rebuild check requires a Linux runner")
    root = Path(root).resolve()
    compiler = root / "tools/clang"
    env = package_environment(root, False)
    # Match a player launching DBZIW without development tools on PATH.
    env["PATH"] = "/usr/bin:/bin"
    build = Path(build or tempfile.mkdtemp(prefix="dbziw-sdk-build-")).resolve()
    if build.exists() and any(build.iterdir()):
        raise SystemExit(f"SDK verification requires a fresh build directory: {build}")
    build.mkdir(parents=True, exist_ok=True)
    cmake = root / "tools/cmake/bin/cmake"

    def run(command, cwd=build):
        print("+", " ".join(map(str, command)), flush=True)
        subprocess.run(list(map(str, command)), cwd=cwd, env=env, check=True, timeout=1800)

    print(f"Offline SDK rebuild evidence: {build}", flush=True)
    run([cmake, "-S", root / "app", "-B", build, "-G", "Ninja",
         f"-DCMAKE_MAKE_PROGRAM={root / 'tools/ninja'}",
         f"-DCMAKE_TOOLCHAIN_FILE={root / 'tools/linux/clang.cmake'}",
         f"-DDBZIW_LINUX_TOOLCHAIN_ROOT={compiler}",
         f"-DPS2X_FFMPEG_ROOT={root / 'tools/ffmpeg'}", "-DDBZIW_BUILD_GAME=OFF",
         "-DCMAKE_BUILD_TYPE=Release",
         "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"])
    run([cmake, "--build", build, "--target", "dbziw_setup", "dbziw_uninstall", "--parallel", str(os.cpu_count() or 1)])
    with tempfile.TemporaryDirectory(prefix="dbziw-offline-uninstall-") as directory:
        smoke_staged_uninstaller(build / "dbziw-uninstall", Path(directory))
    print("PASS: actual packaged Linux SDK rebuild", flush=True)
    # Keep the build tree/logs; do not remove build directories containing _deps.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--platform", choices=("windows", "linux"), required=True)
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--build-sdk", action="store_true",
                        help="rebuild the actual Linux SDK using packaged tools, without game data")
    parser.add_argument("--sdk-build-dir", type=Path,
                        help="fresh directory for retained SDK rebuild evidence")
    args = parser.parse_args()
    windows = args.platform == "windows"
    validate_package(args.root, windows)
    print("PASS: required builder package files", flush=True)
    if args.smoke:
        smoke_package(args.root, windows)
    if args.build_sdk:
        if windows:
            raise SystemExit("--build-sdk currently verifies Linux packages")
        build_linux_sdk(args.root, args.sdk_build_dir)


if __name__ == "__main__":
    main()
