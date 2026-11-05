#!/usr/bin/env python3
"""Release packages (DW5 011 v5): the *builder* — the launcher, the recompiler, the source code, the compiler and
the tools — never the compiled game (PS2Recomp is GPL-3.0; the player's PC builds the game from their own disc).
The package holds nothing from the game, so any PC (and later CI) can make it; publishing is the owner's.

  tools/release/make_release.py windows --build app/build-release-win --recomp-build build-recomp-win [--test]
  tools/release/build_linux.sh [--test]  # offline inputs: see tools/release/LINUX.md

Checklist enforced here:
  * a clean working tree (--test skips this and names the package "...-test"; never publish a test package);
  * the package holds only our files and freely licensed tools; the scan refuses a game binary, generated or lifted
    code, disc files, an ISO, and file names Windows does not allow;
  * BUILD_ID in the package (the player's build shows it); SHA-256 next to the zip.
Not checked here (do it before publishing): `run.py full` green on this commit, the owner's OK.
"""
import argparse, hashlib, os, re, shutil, subprocess, sys, zipfile
from pathlib import Path

import zsync_file

REPO = Path(__file__).resolve().parents[2]
# DW5 044: AppImage updaters look for this file pattern in the latest full GitHub release.
UPDATE_INFORMATION = 'gh-releases-zsync|TheSlackbringer|dbziw-recomp|latest|DBZIW-RECOMP-*x86_64.AppImage.zsync'
LLVM_MINGW_HOST = Path(os.environ.get('LLVM_MINGW_ROOT',
                                      Path.home() / '.local/opt/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64'))
WIN_TOOLS = Path(os.environ.get('DBZIW_WIN_TOOLS', Path.home() / '.local/opt/windows-builder-tools'))
LLVM_MINGW_WIN_ZIP = WIN_TOOLS / 'llvm-mingw-20261006-ucrt-x86_64.zip'
CMAKE_WIN_ZIP = WIN_TOOLS / 'cmake-4.4.4-windows-x86_64.zip'
NINJA_WIN_ZIP = WIN_TOOLS / 'ninja-win.zip'
FFMPEG_MIN_WINDOWS = Path(os.environ.get('DBZIW_FFMPEG_MIN', Path.home() / '.local/opt/ffmpeg-min-windows'))

README_WINDOWS = """Dragon Ball Z: Infinite World - PC port
======================================

A native Windows port made by static recompilation: the game's PS2 code is translated to C++
and compiled for your PC. It is not an emulator.

THIS DOWNLOAD CONTAINS NOTHING FROM THE GAME - no game files and no game code. It is a builder:
on the first start it makes the game on your PC from your own disc. You need:
  * Dragon Ball Z: Infinite World, PAL (SLES-55347), as a disc image (.iso)
You do not need to install anything else: the compiler and all tools are in this folder.

Play
  1. Unzip this folder anywhere (keep the folder together).
  2. Start DBZIW.exe. Windows may say "Windows protected your PC" because the program is not
     signed: choose "More info", then "Run anyway".
  3. First start: choose your disc image, press Continue, choose the installation folder and
     press "Install & Play". Your disc is checked and copied, and the game's code is translated
     and compiled for your PC. This happens once and takes a few minutes (longer on older PCs).
     Then the game starts.
  4. Every later start of DBZIW.exe in the installed folder starts the game at once.

Your files
  The installer suggests C:\\Games\\DBZIW-Recomp. You can choose another folder; DBZIW-Recomp is
  created inside it, with its own DBZIW.exe and this README. You may delete the unzipped
  download afterwards.
  The dbziw-data folder holds the extracted game files, the compiled game, generated code,
  cache, logs, tools and sources. Start the game from the installed DBZIW.exe.

Saves and settings
  The installation folder page offers On this PC (recommended) or With the game (portable).
  On this PC uses %LOCALAPPDATA%\\DBZIW\\user-data for saves and %LOCALAPPDATA%\\DBZIW\\config for
  settings and a small record of where the game is installed. Portable uses
  dbziw-data\\user-data for both. The choice is kept in dbziw-data\\install.json and survives updates.
  Hold Shift when opening DBZIW.exe, or run DBZIW.exe --manage, to change it. Close the game
  first. Saves and settings are copied and checked; the originals are kept. If both places
  hold progress, you choose which saves to use.

Updating and other installations
  Unzip the new version and start its DBZIW.exe: it finds your installation, updates it with
  "Update & Play" and keeps your saves. No disc image is needed again.
  Manage installation can make a second copy for another drive or PC (Copy installation beside
  this launcher; choose Portable to take saves and settings along) and can uninstall the game
  (you choose whether to keep your saves).
  Console: --data-dir <folder>, DBZIW_DATA_DIR, --use-installation <folder>,
  --manage --save-mode normal|portable, --save-conflict source|destination,
  --import-installation <folder>, --status. Path overrides last for one start only.

Keys
  F2  build ID and game tick (for bug reports)     F3  frame rate overlay
  F4  output resolution (1x / 2x / 4x)             F5  anti-aliasing

Source code: https://github.com/TheSlackbringer/dbziw-recomp (GPL-3.0), version {build}.
Licenses of everything included are in dbziw-data\\package\\LICENSES; the compiler and CMake also keep
theirs in their own folders under tools.
This is a fan project, not affiliated with or endorsed by Bandai Namco, Dimps or Sony.
"""

README_LINUX = """Dragon Ball Z: Infinite World - Linux builder
=============================================

Make the AppImage executable in your file manager and open it. Choose your PAL disc image
(SLES-55347) and the installation folder. The first start builds the game from your own disc. The
compiler and all build tools are included; no downloads are needed.

Your files
  The installer suggests ~/Games/DBZIW-Recomp. You can choose another folder; DBZIW-Recomp is
  created inside it, with its own DBZIW.AppImage and this README. You may delete the downloaded
  AppImage afterwards. Later starts of the installed AppImage open the game directly.
  The dbziw-data folder holds the extracted game files, the compiled game, generated code, cache
  and logs, outside the AppImage's temporary mount.

Saves and settings
  The installation folder page offers On this PC (recommended) or With the game (portable).
  On this PC uses $XDG_DATA_HOME/dbziw/user-data for saves and $XDG_CONFIG_HOME/dbziw/config for settings
  and a small record of where the game is installed. Portable uses dbziw-data/user-data for both.
  The choice is kept in dbziw-data/install.json and survives updates.
  Run the AppImage with --manage to change it. Close the game first. Saves and settings are copied
  and checked; the originals are kept. If both places hold progress, you choose which saves to use.

Updating and other installations
  Open the new AppImage: it finds your installation, updates it with "Update & Play" and keeps your
  saves. No disc image is needed again.
  Manage installation can make a second copy for another drive or PC (Copy installation beside this
  AppImage; choose Portable to take saves and settings along) and can uninstall the game (you choose
  whether to keep your saves).
  Console: --data-dir <folder>, DBZIW_DATA_DIR, --use-installation <folder>,
  --manage --save-mode normal|portable, --save-conflict source|destination,
  --import-installation <folder>, --status. Path overrides last for one start only.

Requires x86-64 with AVX2, Linux glibc 2.31 or newer and a Vulkan graphics driver.
Source: https://github.com/TheSlackbringer/dbziw-recomp, build {build}, GPL-3.0.
The download contains our sources and freely licensed tools, with no game data or game binary.
"""


TEST_PACKAGE = False


def fail(msg):
    sys.exit(f'release: {msg}')


def git(*args, cwd=REPO):
    # Packaging reads host-owned checkouts in a container. Trust only this checkout
    # for this invocation; never change the user's global Git trust settings.
    return subprocess.run(['git', '-c', f'safe.directory={cwd.resolve()}', '-C', str(cwd), *args],
                          env={**os.environ, 'GIT_OPTIONAL_LOCKS': '0'},
                          capture_output=True, text=True, check=True).stdout


def disc_file_names():
    text = (REPO / 'app/setup/disc_manifest.h').read_text(encoding='utf-8')
    return {Path(m).name.upper() for m in re.findall(r'\{"([^"]+)",\s*\d+u', text)}


def version_for(test):
    if test:
        return git('describe', '--always').strip() + '-test'
    if git('status', '--porcelain', '--untracked-files=no').strip():
        fail('the working tree has changes; commit or use --test')
    return git('describe', '--tags', '--always').strip()


def copy_tracked(src_repo, rel_paths, dest, skip=()):
    """Copies the files git tracks under rel_paths of src_repo (working-tree contents) to dest."""
    files = git('ls-files', '-z', '--recurse-submodules', '--', *rel_paths, cwd=src_repo).split('\0')
    if TEST_PACKAGE and src_repo == REPO:  # uncommitted new files are part of what a test package tests
        files += git('ls-files', '-z', '--others', '--exclude-standard', '--', *rel_paths, cwd=src_repo).split('\0')
    n = 0
    for f in filter(None, files):
        if any(f.startswith(s) for s in skip) or (src_repo / f).is_dir():  # a submodule that is not checked out
            continue
        target = dest / f
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src_repo / f, target)
        n += 1
    return n


def dll_closure(exe, search):
    readobj = LLVM_MINGW_HOST / 'bin' / 'llvm-readobj'
    found, todo = {}, [exe]
    while todo:
        out = subprocess.run([str(readobj), '--coff-imports', str(todo.pop())], capture_output=True, text=True).stdout
        for line in out.splitlines():
            line = line.strip()
            if not (line.startswith('Name:') and line.lower().endswith('.dll')):
                continue
            name = line.split(':', 1)[1].strip()
            if name.lower() in found:
                continue
            for d in search:
                hit = next((p for p in d.glob('*') if p.name.lower() == name.lower()), None)
                if hit:
                    found[name.lower()] = hit
                    todo.append(hit)
                    break
    return sorted(found.values(), key=lambda p: p.name.lower())


def dep_source(build, dep):
    """A dependency's source checkout: the build folder's, else DBZIW_DEP_SOURCES (a build that reuses another copy)."""
    source = build / '_deps' / f'{dep}-src'
    if not source.exists() and os.environ.get('DBZIW_DEP_SOURCES'):
        source = Path(os.environ['DBZIW_DEP_SOURCES']) / f'{dep}-src'
    return source


def unzip_into(archive, dest, strip_top=True):
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            parts = Path(info.filename).parts
            rel = Path(*parts[1:]) if strip_top and len(parts) > 1 else Path(*parts)
            if not rel.parts:
                continue
            target = dest / rel
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with z.open(info) as src, open(target, 'wb') as out:
                shutil.copyfileobj(src, out)


def apache_text():
    text = (LLVM_MINGW_HOST / 'LICENSE.TXT').read_text(encoding='utf-8', errors='replace')
    start = text.index('Apache License')
    end = text.index('---- LLVM Exceptions')
    return text[start:end].rstrip() + '\n'


# Text only the compiled game contains (its movie code, app/src/dbziw_movies.cpp): finds the game under any name.
GAME_MARKER = b'[sofdec] sceMpegCreate'


def scan(stage, windows=True):
    """Nothing from the game may be in a release package."""
    forbidden_names = disc_file_names() | {'DBZIW.ELF'}
    bad = []
    for p in stage.rglob('*'):
        rel = p.relative_to(stage).as_posix()
        name = p.name
        if windows and any(c in name for c in '<>:"|?*\\'):
            bad.append(f'{rel} (a character Windows does not allow in file names)')
        if p.is_dir():
            continue
        low = rel.lower()
        if (name.upper() in forbidden_names or low.endswith(('.iso', '.elf', '.irx')) or
                re.search(r'(^|/)recomp/(.*/)?output/', low) or
                re.search(r'(^|/)recomp/(.*/)?(vu0|vu1|iop)/', low) or
                re.match(r'fun_[0-9a-f]+_0x[0-9a-f]+\.cpp$', name.lower()) or
                re.search(r'vu[01]_lifted', low)):
            bad.append(rel)
        else:
            with p.open('rb') as stream:
                header = stream.read(4)
                if header[:2] == b'MZ' or header == b'\x7fELF':
                    previous = header
                    while block := stream.read(1024 * 1024):
                        if GAME_MARKER in previous + block:
                            bad.append(f'{rel} (the compiled game)')
                            break
                        previous = block[-len(GAME_MARKER):]
    if bad:
        fail('not allowed in a release package: ' + ', '.join(sorted(bad)[:20]))


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def windows(build, recomp_build, test):
    launcher = build / 'dbziw_setup.exe'
    bootstrap = build / 'DBZIW-bootstrap.exe'
    uninstall = build / 'dbziw-uninstall.exe'
    recompiler = recomp_build / 'ps2xRecomp' / 'ps2_recomp.exe'
    for path in (launcher, bootstrap, uninstall, recompiler, LLVM_MINGW_WIN_ZIP, CMAKE_WIN_ZIP, NINJA_WIN_ZIP,
                 Path(f'{FFMPEG_MIN_WINDOWS}.zip')):
        if not path.exists():
            fail(f'{path} missing')
    version = version_for(test)
    name = f'DBZIW-{version}-windows-x64'
    dist = REPO / 'dist'
    stage = dist / name
    shutil.rmtree(stage, ignore_errors=True)
    stage.mkdir(parents=True)

    # Only the small starter and the README sit outside dbziw-data (DW5-E): the real launcher and its DLLs stay in the package.
    payload = stage / 'dbziw-data' / 'package'
    (payload / 'launcher').mkdir(parents=True)
    shutil.copy2(bootstrap, stage / 'DBZIW.exe')
    shutil.copy2(launcher, payload / 'launcher/DBZIW.exe')
    shutil.copy2(uninstall, payload / 'launcher/dbziw-uninstall.exe')
    host_dlls = [build, LLVM_MINGW_HOST / 'x86_64-w64-mingw32/bin']
    if dll_closure(bootstrap, host_dlls):
        fail('the Windows starter must depend only on system DLLs')
    if dll_closure(uninstall, host_dlls):
        fail('the Windows uninstall helper must depend only on system DLLs')
    for dll in dll_closure(launcher, host_dlls):
        shutil.copy2(dll, payload / 'launcher' / dll.name)
    (payload / 'BUILD_ID').write_bytes(f'{version}\n'.encode())

    # Our source (only what git tracks: never anything game-derived) and the libraries' sources.
    n = copy_tracked(REPO, ['app', 'external/PS2Recomp', 'recomp', 'tools/windows', 'LICENSE'], payload,
                     skip=('external/PS2Recomp/android/', 'external/PS2Recomp/vita/', 'external/PS2Recomp/ps2xStudio/'))
    n += copy_tracked(REPO / 'app/deps/parallel-gs', ['.'], payload / 'app/deps/parallel-gs')
    raylib, imgui, rlimgui = (dep_source(build, dep) for dep in ('raylib', 'imgui', 'rlimgui'))
    n += copy_tracked(raylib, ['.'], payload / 'app/deps/raylib-src', skip=('examples/', 'projects/'))
    n += copy_tracked(imgui, ['.'], payload / 'app/deps/imgui-src', skip=('examples/', 'docs/'))
    n += copy_tracked(rlimgui, ['.'], payload / 'app/deps/rlimgui-src', skip=('examples/',))

    # Tools: recompiler, compiler, CMake, Ninja, FFmpeg.
    tools = payload / 'tools'
    shutil.copy2(recompiler, tools / 'ps2_recomp.exe')
    for dll in dll_closure(recompiler, host_dlls):
        shutil.copy2(dll, tools / dll.name)
    unzip_into(LLVM_MINGW_WIN_ZIP, tools / 'llvm-mingw')
    unzip_into(CMAKE_WIN_ZIP, tools / 'cmake')
    unzip_into(NINJA_WIN_ZIP, tools, strip_top=False)
    shutil.copy2(f'{FFMPEG_MIN_WINDOWS}.zip', tools / 'ffmpeg.zip')

    # Player README and licenses.
    (stage / 'README.txt').write_bytes(README_WINDOWS.format(build=version).replace('\n', '\r\n').encode('utf-8'))
    lic = payload / 'LICENSES'
    lic.mkdir()
    pgs = REPO / 'app/deps/parallel-gs'
    for target, source in {
        'dbziw (GPL-3.0).txt': REPO / 'LICENSE',
        'PS2Recomp (GPL-3.0).txt': REPO / 'external/PS2Recomp/LICENSE',
        'paraLLEl-GS (LGPL-3.0).txt': pgs / 'COPYING.LGPLv3',
        'Granite (MIT).txt': pgs / 'Granite/LICENSE',
        'volk (MIT).txt': pgs / 'Granite/third_party/volk/LICENSE.md',
        'raylib (zlib).txt': raylib / 'LICENSE',
        'GLFW (zlib).txt': raylib / 'src/external/glfw/LICENSE.md',
        'Dear ImGui (MIT).txt': imgui / 'LICENSE.txt',
        'rlImGui (zlib).txt': rlimgui / 'LICENSE',
        'llvm-mingw (Apache-2.0 WITH LLVM-exception and others).txt': LLVM_MINGW_HOST / 'LICENSE.TXT',
        'FFmpeg (LGPL-2.1).txt': FFMPEG_MIN_WINDOWS / 'licenses/ffmpeg/COPYING.LGPLv2.1',
    }.items():
        if not source.exists():
            fail(f'license file missing: {source}')
        shutil.copy2(source, lic / target)
    (lic / 'Ninja (Apache-2.0).txt').write_text('Ninja - https://ninja-build.org - Copyright Google Inc. and contributors.\n'
                                                'Licensed under the Apache License, Version 2.0:\n\n' + apache_text())
    (lic / 'CMake (BSD-3-Clause).txt').write_text('CMake - https://cmake.org - BSD-3-Clause. Its license and the '
                                                  'licenses of its parts are in tools/cmake/doc/cmake.\n')
    # What an uninstall may remove from the installed folder: exactly the files the package brought (DW5-E 566b5a2b).
    (payload / 'package-files.txt').write_bytes(
        ''.join(f'{path.relative_to(payload).as_posix()}\n' for path in sorted(payload.rglob('*')) if path.is_file()).encode('utf-8'))

    scan(stage)
    archive = dist / f'{name}.zip'
    archive.unlink(missing_ok=True)
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(stage.rglob('*')):
            if p.is_file():
                z.write(p, Path(name) / p.relative_to(stage))
    (dist / f'{name}.zip.sha256').write_text(f'{sha256(archive)}  {archive.name}\n')
    files = sum(1 for p in stage.rglob('*') if p.is_file())
    print(f'{archive} ({archive.stat().st_size / 2**20:.0f} MB, {files} files, {n} source files)')
    return archive


def linux_package_name(version):
    # DW5 044: named after the project; no "linux", since every AppImage is for Linux.
    return f'DBZIW-RECOMP-{version}-x86_64'


def linux(build, recomp_build, sdk, ffmpeg, appimage_tools, test):
    """Build an AppDir containing the setup launcher, sources and an offline SDK."""
    launcher = build / 'dbziw_setup'
    uninstall = build / 'dbziw-uninstall'
    recompiler = recomp_build / 'ps2xRecomp/ps2_recomp'
    for path in (launcher, uninstall, recompiler, sdk / 'bin/clang++', ffmpeg / 'include/libavcodec/avcodec.h',
                 appimage_tools / 'linuxdeploy-x86_64.AppImage', appimage_tools / 'appimagetool-x86_64.AppImage'):
        if not path.exists():
            fail(f'{path} missing')
    version = version_for(test)
    name = linux_package_name(version)
    dist = REPO / 'dist'
    stage = dist / f'{name}.AppDir'
    if stage.exists():
        shutil.rmtree(stage)
    (stage / 'usr/bin').mkdir(parents=True)
    (stage / 'usr/lib').mkdir()
    copy_tracked(REPO, ['app', 'external/PS2Recomp', 'recomp', 'tools/linux', 'LICENSE'], stage,
                 skip=('external/PS2Recomp/android/', 'external/PS2Recomp/vita/', 'external/PS2Recomp/ps2xStudio/'))
    dep_root = Path(os.environ.get('DBZIW_PACKAGE_DEP_ROOT', REPO / 'app/deps'))
    pgs = dep_root / 'parallel-gs'
    copy_tracked(pgs, ['.'], stage / 'app/deps/parallel-gs')
    for dep in ('raylib', 'imgui', 'rlimgui'):
        source = build / '_deps' / f'{dep}-src'
        if not source.exists():
            source = dep_root / f'{dep}-src'
        copy_tracked(source, ['.'], stage / 'app/deps' / f'{dep}-src')
    shutil.copy2(launcher, stage / 'usr/bin/dbziw_setup')
    shutil.copy2(uninstall, stage / 'usr/bin/dbziw-uninstall')
    shutil.copy2(recompiler, stage / 'tools/ps2_recomp')
    shutil.copytree(sdk, stage / 'tools/clang', symlinks=True)
    shutil.copytree(ffmpeg, stage / 'tools/ffmpeg', symlinks=True)
    (stage / 'tools/cmake/bin').mkdir(parents=True)
    (stage / 'tools/cmake/bin/cmake').symlink_to('../../clang/bin/cmake')
    (stage / 'tools/ninja').symlink_to('clang/bin/ninja')
    for prefix in (sdk / 'lib', ffmpeg / 'lib'):
        for library in prefix.glob('*.so*'):
            shutil.copy2(library, stage / 'usr/lib' / library.name)
    (stage / 'BUILD_ID').write_bytes(f'{version}\n'.encode())
    (stage / 'README.txt').write_text(README_LINUX.format(build=version))
    licenses = stage / 'LICENSES'
    licenses.mkdir()
    for label, path in {
        'DBZIW-GPL-3.0.txt': REPO / 'LICENSE',
        'PS2Recomp-GPL-3.0.txt': REPO / 'external/PS2Recomp/LICENSE',
        'parallel-gs-LGPL-3.0.txt': pgs / 'COPYING.LGPLv3',
        'Granite-MIT.txt': pgs / 'Granite/LICENSE',
        'volk-MIT.txt': pgs / 'Granite/third_party/volk/LICENSE.md',
        'FFmpeg-LGPL-2.1.txt': ffmpeg / 'licenses/ffmpeg/COPYING.LGPLv2.1',
    }.items():
        shutil.copy2(path, licenses / label)
    (licenses / 'TOOLCHAIN.txt').write_text('Compiler, SDK, CMake and Ninja copyright notices are in tools/clang/licenses.\n')
    icon = REPO / 'tools/release/dbziw.svg'
    desktop = REPO / 'tools/release/dbziw.desktop'
    env = {**os.environ, 'APPIMAGE_EXTRACT_AND_RUN': '1',
           'LD_LIBRARY_PATH': str(stage / 'usr/lib'), 'ARCH': 'x86_64'}
    subprocess.run([str(appimage_tools / 'linuxdeploy-x86_64.AppImage'), '--appdir', str(stage),
                    '-e', str(stage / 'usr/bin/dbziw_setup'), '-d', str(desktop), '-i', str(icon)], env=env, check=True)
    # linuxdeploy handles runtime dependencies; use our launcher to keep the release root explicit.
    app_run = stage / 'AppRun'
    app_run.unlink(missing_ok=True)
    app_run.write_text('''#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$root/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$root/usr/bin/dbziw_setup" --root "$root" "$@"
''')
    app_run.chmod(0o755)
    scan(stage, windows=False)
    archive = dist / f'{name}.AppImage'
    image_tool = appimage_tools / 'appimagetool-x86_64.AppImage'
    # Reuse the runtime already present in the approved tool download. The tool's
    # default is to fetch another runtime, which would break an offline build.
    offset = int(subprocess.check_output([str(image_tool), '--appimage-offset'], text=True).strip())
    runtime = build / 'appimage-runtime-x86_64'
    with image_tool.open('rb') as stream:
        runtime.write_bytes(stream.read(offset))
    # -u embeds the update information and has appimagetool's bundled zsyncmake write the .zsync, into the current
    # folder. appimagetool only warns when zsyncmake is missing, so the .zsync is checked here.
    zsync = dist / f'{archive.name}.zsync'
    zsync.unlink(missing_ok=True)
    subprocess.run([str(image_tool), '--no-appstream', '--runtime-file', str(runtime), '-u', UPDATE_INFORMATION,
                    str(stage), str(archive)], env=env, check=True, cwd=dist)
    try:
        zsync_file.check(zsync, archive)
    except (OSError, ValueError) as error:
        fail(f'no matching update file next to the AppImage: {error}')
    Path(str(archive) + '.sha256').write_text(f'{sha256(archive)}  {archive.name}\n')
    print(f'{archive} ({archive.stat().st_size / 2**20:.0f} MB)')
    return archive


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('target', choices=['windows', 'linux'])
    ap.add_argument('--build', required=True, type=Path, help='build folder with the setup launcher')
    ap.add_argument('--recomp-build', required=True, type=Path, help='build folder of the native recompiler')
    ap.add_argument('--test', action='store_true', help='allow a dirty tree; the package is named "-test"')
    ap.add_argument('--linux-sdk', type=Path)
    ap.add_argument('--ffmpeg-root', type=Path)
    ap.add_argument('--appimage-tools', type=Path, default=Path.home() / '.local/opt/appimage-tools')
    args = ap.parse_args()
    global TEST_PACKAGE
    TEST_PACKAGE = args.test
    if args.target == 'windows':
        windows(args.build.resolve(), args.recomp_build.resolve(), args.test)
    else:
        if not args.linux_sdk or not args.ffmpeg_root:
            ap.error('Linux requires --linux-sdk and --ffmpeg-root')
        linux(args.build.resolve(), args.recomp_build.resolve(), args.linux_sdk.resolve(),
              args.ffmpeg_root.resolve(), args.appimage_tools.resolve(), args.test)


if __name__ == '__main__':
    main()
