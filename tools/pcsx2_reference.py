#!/usr/bin/env python3
"""Capture reference screenshots of the original game in PCSX2, with nothing shown on the desktop.

When unsure how the original renders or behaves, compare the port against PCSX2 instead of guessing (issue 017,
from DW5's tools/pcsx2_reference.py, DW5 3f32379 and 38c1fe3; the Windows path from 020's PCSX2 driving).

  Linux    the owner's PCSX2 AppImage on a private Xvfb display; keys go to that display only (never send synthetic
           keys to the real desktop).
  Windows  a throwaway portable copy of the installed PCSX2 on a separate Windows desktop that is never shown
           (CreateDesktop); keys are posted to PCSX2's own windows (PostMessage), never sent as global input.

PCSX2 runs from a throwaway copy of the owner's settings (BIOS files copied, memory-card copies), so their settings
and saves are never touched. Software renderer (accurate reference), PCSX2's own F8 screenshot hotkey, game
controllers off (only the scripted keys reach it), no sound (Linux: the dbziw_codex_tests null sink; Windows:
PCSX2's Null audio backend).

  tools/pcsx2_reference.py --out <dir> --seconds 120 [--interval 0.5] [--keys "8:CROSS,15:START:0.2"]
                           [--iso <file>] [--mc-folder <saves> | --mc-file <card.ps2>]

Keys: <seconds after launch>:<key>[:<hold seconds>]. <key> is a pad button in the port's names (CROSS, CIRCLE,
SQUARE, TRIANGLE, START, SELECT, UP, DOWN, LEFT, RIGHT, L1, R1, L2, R2) or a keyboard key as an X keysym name (x,
Return, F8; on Linux combinations with +, e.g. Shift_L+F8 for a one-frame GS dump, Control_L+Shift_L+F8 to start or
stop a multi-frame dump; Windows takes single keys only, since PCSX2 reads modifiers from the real keyboard). The
copied config binds the pad like the port's keyboard: Cross X, Circle C, Square Z, Triangle V, Start Return, D-pad
arrows, L1 Q, R1 E; Select Backspace (PCSX2's Tab is its turbo hotkey), L2 1 and R2 3 (PCSX2 does not tell the left
and right Shift keys apart).
--mc-folder copies a folder of saves (the port's memory-card layout, e.g. a test run's mc0) into the throwaway config
as PCSX2's slot 1 folder card, with a formatted card header when it has none; --mc-file uses a copy of a .ps2 card
file. Without either, the owner's cards are copied. The card stays in <out> after the run.
Screenshots land in <out>/snaps (PCSX2 names them by wall-clock time; sort by name); PCSX2's log is <out>/pcsx2.log.
The output is game-derived: keep it out of Git (notes/ or a scratch folder).

Requires: Linux: Xvfb, python3-xlib, Pillow for --xshots, the AppImage and a BIOS set up in ~/.config/PCSX2.
Windows: PCSX2 installed (default C:\\Program Files\\PCSX2), a BIOS set up in Documents\\PCSX2. Both: the
player's disc image in discs/ (or --iso).
"""
import argparse
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WINDOWS = os.name == 'nt'
if WINDOWS:
    DEFAULT_APP = os.path.join(os.environ.get('ProgramFiles', r'C:\Program Files'), 'PCSX2', 'pcsx2-qt.exe')
    OWNER_CFG = os.path.join(os.path.expanduser('~'), 'Documents', 'PCSX2')
else:
    DEFAULT_APP = os.path.expanduser('~/Games/psx2/pcsx2-v2.8.2-linux-appimage-x64-Qt.AppImage')
    OWNER_CFG = os.path.expanduser('~/.config/PCSX2')
# Pad button -> (PCSX2 binding, X keysym of that key): the port's keyboard map (AGENTS.md "Running the game by hand").
PAD = {'CROSS': ('Cross', 'x'), 'CIRCLE': ('Circle', 'c'), 'SQUARE': ('Square', 'z'), 'TRIANGLE': ('Triangle', 'v'),
       'START': ('Start', 'Return'), 'SELECT': ('Select', 'BackSpace'), 'UP': ('Up', 'Up'), 'DOWN': ('Down', 'Down'),
       'LEFT': ('Left', 'Left'), 'RIGHT': ('Right', 'Right'), 'L1': ('L1', 'q'), 'R1': ('R1', 'e'),
       'L2': ('L2', '1'), 'R2': ('R2', '3')}
QT_KEY = {'BackSpace': 'Backspace'}  # PCSX2's own key names where they differ from X keysyms


def set_key(ini, section, key, value):
    m = re.search(r'^\[' + re.escape(section) + r'\]\n', ini, re.M)
    if not m:
        return ini + f'\n[{section}]\n{key} = {value}\n'
    end = ini.find('\n[', m.end())
    end = len(ini) if end < 0 else end
    body = ini[m.end():end]
    if re.search(r'^' + re.escape(key) + r' = .*$', body, re.M):
        body = re.sub(r'^' + re.escape(key) + r' = .*$', lambda _: f'{key} = {value}', body, flags=re.M)
    else:
        body = f'{key} = {value}\n' + body
    return ini[:m.end()] + body + ini[end:]


def drop_keys(ini, section, keys):
    m = re.search(r'^\[' + re.escape(section) + r'\]\n', ini, re.M)
    if not m:
        return ini
    end = ini.find('\n[', m.end())
    end = len(ini) if end < 0 else end
    body = re.sub(r'^(?:' + '|'.join(map(re.escape, keys)) + r') = .*\n?', '', ini[m.end():end], flags=re.M)
    return ini[:m.end()] + body + ini[end:]


def get_key(ini, section, key):
    m = re.search(r'^\[' + re.escape(section) + r'\]\n(.*?)(?=^\[|\Z)', ini, re.M | re.S)
    v = re.search(r'^' + re.escape(key) + r' = (.*)$', m.group(1), re.M) if m else None
    return v.group(1).strip() if v else None


def folder_card_superblock():
    """Block 0 of a formatted 8 MB PS2 memory card, as PCSX2 keeps it in a folder card's _pcsx2_superblock (8 KiB;
    PCSX2 counts the card as formatted when byte 0x16 is the 'o' of the magic). The standard values of an 8 MB card:
    512-byte pages, 2 pages per cluster, 16 per erase block, 8192 clusters, data from cluster 41 to 8135, the root
    directory at cluster 0, backup blocks 1023/1022, the indirect FAT cluster 8, no bad blocks, PS2 card."""
    sb = bytearray(b'\xff' * 8192)
    struct.pack_into('<28s12sHHHHIIIIII8x', sb, 0, b'Sony PS2 Memory Card Format ', b'1.2.0.0', 512, 2, 16, 0xFF00,
                     8192, 41, 8135, 0, 1023, 1022)
    struct.pack_into('<32I', sb, 0x50, 8, *([0] * 31))
    struct.pack_into('<32I', sb, 0xD0, *([0xFFFFFFFF] * 32))
    struct.pack_into('<BB', sb, 0x150, 2, 0x52)
    return bytes(sb)


def make_config(cfg, out, owner_cfg, mc_folder=None, mc_file=None):
    """Writes the throwaway PCSX2 config into `cfg` (inis/, bios/, memcards/) from the owner's, which is only read."""
    for sub in ('inis', 'bios', 'memcards'):
        os.makedirs(os.path.join(cfg, sub), exist_ok=True)
    os.makedirs(os.path.join(out, 'snaps'), exist_ok=True)
    ini = open(os.path.join(owner_cfg, 'inis', 'PCSX2.ini'), encoding='utf-8').read().replace('\r\n', '\n')
    # The BIOS files are copied: PCSX2 writes the BIOS's settings file (.NVM) next to it.
    bios_dir = get_key(ini, 'Folders', 'Bios') or 'bios'
    if not os.path.isabs(bios_dir):
        bios_dir = os.path.normpath(os.path.join(owner_cfg, bios_dir))
    bios = get_key(ini, 'Filenames', 'BIOS')
    if not bios or not os.path.isfile(os.path.join(bios_dir, bios)):
        sys.exit(f'no BIOS set up in {owner_cfg} (PCSX2.ini [Filenames] BIOS)')
    stem = os.path.splitext(bios)[0]
    for path in glob.glob(os.path.join(bios_dir, glob.escape(stem) + '*')):
        if os.path.isfile(path):
            shutil.copyfile(path, os.path.join(cfg, 'bios', os.path.basename(path)))
    ini = set_key(ini, 'Folders', 'Bios', os.path.join(cfg, 'bios'))
    ini = set_key(ini, 'Folders', 'MemoryCards', os.path.join(cfg, 'memcards'))
    ini = set_key(ini, 'Folders', 'Snapshots', os.path.join(out, 'snaps'))
    # The disc image is started directly: no game-list folders, so PCSX2 does not scan the owner's folders.
    ini = drop_keys(ini, 'GameList', ('Paths', 'RecursivePaths'))
    ini = set_key(ini, 'EmuCore/GS', 'Renderer', '13')
    for key, value in (('PauseOnFocusLoss', 'false'), ('ConfirmShutdown', 'false'), ('StartFullscreen', 'false'),
                       ('SetupWizardIncomplete', 'false')):
        ini = set_key(ini, 'UI', key, value)
    for source in ('SDL', 'XInput', 'DInput', 'Mouse'):  # only the scripted keys reach the game
        ini = set_key(ini, 'InputSources', source, 'false')
    ini = set_key(ini, 'Pad1', 'Type', 'DualShock2')
    for button, keysym in PAD.values():
        ini = set_key(ini, 'Pad1', button, 'Keyboard/' + QT_KEY.get(keysym, keysym.upper() if len(keysym) == 1 else keysym))
    ini = set_key(ini, 'Hotkeys', 'Screenshot', 'Keyboard/F8')
    if WINDOWS:
        ini = set_key(ini, 'SPU2/Output', 'Backend', 'Null')
    if mc_file:
        # Slot 1: a copy of a PCSX2 card file (.ps2); slot 2 off.
        shutil.copyfile(mc_file, os.path.join(cfg, 'memcards', 'Mcd001.ps2'))
    elif mc_folder:
        # Slot 1: a copy of a folder of PS2 saves as a PCSX2 folder card (a directory at the slot path); slot 2 off.
        card = os.path.join(cfg, 'memcards', 'Mcd001.ps2')
        shutil.copytree(mc_folder, card, copy_function=shutil.copyfile)
        for root, dirs, files in os.walk(card):
            for name in dirs + files:
                os.chmod(os.path.join(root, name), 0o755 if name in dirs else 0o644)
        os.chmod(card, 0o755)
        superblock = os.path.join(card, '_pcsx2_superblock')
        if not os.path.exists(superblock) or os.path.getsize(superblock) != 8192:
            open(superblock, 'wb').write(folder_card_superblock())
    else:
        for card in glob.glob(os.path.join(owner_cfg, 'memcards', '*.ps2')):
            if os.path.isfile(card):
                shutil.copy2(card, os.path.join(cfg, 'memcards'))
    if mc_file or mc_folder:
        ini = set_key(ini, 'MemoryCards', 'Slot1_Enable', 'true')
        ini = set_key(ini, 'MemoryCards', 'Slot1_Filename', 'Mcd001.ps2')
        ini = set_key(ini, 'MemoryCards', 'Slot2_Enable', 'false')
    open(os.path.join(cfg, 'inis', 'PCSX2.ini'), 'w', encoding='utf-8').write(ini)


def keysyms_of(key):
    """'CROSS' -> ['x']; 'Shift_L+F8' -> ['Shift_L', 'F8']."""
    return [PAD[k.upper()][1] if k.upper() in PAD else k for k in key.split('+')]


def run_linux(args, iso, out, keys):
    home = os.path.join(out, 'pcsx2home')
    make_config(os.path.join(home, 'PCSX2'), out, args.owner_config, args.mc_folder, args.mc_file)
    xvfb = subprocess.Popen(['Xvfb', args.display, '-screen', '0', '1280x1024x24', '-nolisten', 'tcp'],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.5)
    os.environ['DISPLAY'] = args.display
    from Xlib import X, XK, display
    from Xlib.ext import xtest

    env = dict(os.environ, DISPLAY=args.display, QT_QPA_PLATFORM='xcb', XDG_CONFIG_HOME=home,
               PULSE_SINK='dbziw_codex_tests', VK_DRIVER_FILES='/nonexistent.json', VK_ICD_FILENAMES='/nonexistent.json')
    log = open(os.path.join(out, 'pcsx2.log'), 'w')
    proc = subprocess.Popen([args.app, '-batch', '-fastboot', '--', iso], env=env, stdout=log, stderr=subprocess.STDOUT)
    print(f'PCSX2 pid {proc.pid} started {time.strftime("%Y-%m-%d %H:%M:%S")} on {args.display}', flush=True)
    d = display.Display(args.display)
    d.set_error_handler(lambda *a: None)
    root = d.screen().root
    pid_atom = d.intern_atom('_NET_WM_PID')

    def descends(pid):
        while pid > 1:
            if pid == proc.pid:
                return True
            try:
                pid = int(open('/proc/%d/stat' % pid).read().rsplit(')', 1)[1].split()[1])
            except OSError:
                return False
        return False

    def game_window():
        stack = [root]
        while stack:
            w = stack.pop()
            try:
                name = w.get_wm_name() or ''
                pid = w.get_full_property(pid_atom, X.AnyPropertyType)
                stack.extend(w.query_tree().children)
            except Exception:
                continue
            if pid and descends(pid.value[0]) and name and 'PCSX2' not in name and 'Qt' not in name \
                    and w.get_attributes().map_state == X.IsViewable:
                return w
        return None

    def press(key, hold=0.08):
        win = game_window()
        if win is None:
            return False
        win.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        codes = [d.keysym_to_keycode(XK.string_to_keysym(k)) for k in keysyms_of(key)]
        for code in codes:
            xtest.fake_input(d, X.KeyPress, code)
        d.sync()
        time.sleep(hold)
        for code in reversed(codes):
            xtest.fake_input(d, X.KeyRelease, code)
        d.sync()
        return True

    def xshot(index):
        from PIL import Image
        geometry = root.get_geometry()
        raw = root.get_image(0, 0, geometry.width, geometry.height, X.ZPixmap, 0xFFFFFFFF)
        Image.frombytes('RGB', (geometry.width, geometry.height), raw.data, 'raw', 'BGRX').save(
            os.path.join(out, 'xshots', '%04d.png' % index))

    if args.xshots:
        os.makedirs(os.path.join(out, 'xshots'), exist_ok=True)
    try:
        drive(args, keys, press, lambda: proc.poll() is None, xshot if args.xshots else None)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            proc.kill()
        xvfb.terminate()


def drive(args, keys, press, alive, xshot=None):
    """Sends the keys at their times and an F8 screenshot every interval while PCSX2 runs, for args.seconds."""
    start = time.time()
    next_shot = start + args.interval
    shot_index = 0
    while time.time() - start < args.seconds and alive():
        now = time.time()
        while keys and keys[0][0] <= now - start:
            t, k, hold = keys.pop(0)
            print('%.1fs key %s: %s' % (now - start, k, 'sent' if press(k, hold) else 'no window yet'), flush=True)
        if now >= next_shot:
            press('F8')
            if xshot:
                xshot(shot_index)
            shot_index += 1
            next_shot += args.interval
        time.sleep(0.02)


def run_windows(args, iso, out, keys):
    import ctypes
    from ctypes import wintypes
    user32 = ctypes.WinDLL('user32', use_last_error=True)
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    enum_proc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.CreateDesktopW.restype = wintypes.HANDLE
    user32.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_void_p, wintypes.DWORD,
                                      wintypes.DWORD, ctypes.c_void_p]
    user32.CloseDesktop.argtypes = [wintypes.HANDLE]
    user32.EnumDesktopWindows.argtypes = [wintypes.HANDLE, enum_proc, wintypes.LPARAM]
    user32.EnumChildWindows.argtypes = [wintypes.HWND, enum_proc, wintypes.LPARAM]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]

    class StartupInfo(ctypes.Structure):
        _fields_ = [('cb', wintypes.DWORD), ('lpReserved', wintypes.LPWSTR), ('lpDesktop', wintypes.LPWSTR),
                    ('lpTitle', wintypes.LPWSTR), ('dwX', wintypes.DWORD), ('dwY', wintypes.DWORD),
                    ('dwXSize', wintypes.DWORD), ('dwYSize', wintypes.DWORD), ('dwXCountChars', wintypes.DWORD),
                    ('dwYCountChars', wintypes.DWORD), ('dwFillAttribute', wintypes.DWORD),
                    ('dwFlags', wintypes.DWORD), ('wShowWindow', wintypes.WORD), ('cbReserved2', wintypes.WORD),
                    ('lpReserved2', ctypes.c_void_p), ('hStdInput', wintypes.HANDLE),
                    ('hStdOutput', wintypes.HANDLE), ('hStdError', wintypes.HANDLE)]

    class ProcessInformation(ctypes.Structure):
        _fields_ = [('hProcess', wintypes.HANDLE), ('hThread', wintypes.HANDLE), ('dwProcessId', wintypes.DWORD),
                    ('dwThreadId', wintypes.DWORD)]

    # A throwaway portable PCSX2: the program copied, portable.ini beside it, so all its settings, BIOS, cards,
    # logs and caches live in the copy and the owner's Documents\PCSX2 is only read.
    app_dir = os.path.dirname(os.path.abspath(args.app))
    cfg = os.path.join(out, 'pcsx2')
    exe = os.path.join(cfg, os.path.basename(args.app))

    def tidy():
        """Keeps the settings, logs and the memory card; drops the program copy and the BIOS copy."""
        log = os.path.join(cfg, 'logs', 'emulog.txt')
        if os.path.isfile(log):
            shutil.copyfile(log, os.path.join(out, 'pcsx2.log'))
        for name in os.listdir(cfg) if os.path.isdir(cfg) else []:
            if name not in ('inis', 'logs', 'memcards'):
                path = os.path.join(cfg, name)
                shutil.rmtree(path, ignore_errors=True) if os.path.isdir(path) else os.remove(path)

    try:
        shutil.copytree(app_dir, cfg, ignore=shutil.ignore_patterns('unins*', 'updater*', 'portable.*'))
        open(os.path.join(cfg, 'portable.ini'), 'w').close()
        make_config(cfg, out, args.owner_config, args.mc_folder, args.mc_file)
    except BaseException:
        tidy()
        raise

    vk = {'Return': 0x0D, 'BackSpace': 0x08, 'Tab': 0x09, 'Escape': 0x1B, 'space': 0x20, 'Left': 0x25, 'Up': 0x26,
          'Right': 0x27, 'Down': 0x28, **{f'F{i}': 0x6F + i for i in range(1, 13)},
          **{chr(c): c - 0x20 for c in range(0x61, 0x7B)}, **{str(i): 0x30 + i for i in range(10)}}
    extended = {0x25, 0x26, 0x27, 0x28}
    desktop_name = f'dbziw-pcsx2-{os.getpid()}'
    desktop = user32.CreateDesktopW(desktop_name, None, None, 0, 0x10000000, None)  # GENERIC_ALL
    if not desktop:
        tidy()
        sys.exit(f'CreateDesktop failed (error {ctypes.get_last_error()})')
    si = StartupInfo(cb=ctypes.sizeof(StartupInfo), lpDesktop='WinSta0\\' + desktop_name)
    pi = ProcessInformation()
    cmdline = ctypes.create_unicode_buffer(subprocess.list2cmdline([exe, '-batch', '-nogui', '-fastboot', '--', iso]))
    if not kernel32.CreateProcessW(exe, cmdline, None, None, False, 0, None, cfg, ctypes.byref(si), ctypes.byref(pi)):
        error = ctypes.get_last_error()
        user32.CloseDesktop(desktop)
        tidy()
        sys.exit(f'CreateProcess failed (error {error})')
    print(f'PCSX2 pid {pi.dwProcessId} started {time.strftime("%Y-%m-%d %H:%M:%S")} on the hidden desktop '
          f'{desktop_name}', flush=True)

    def windows():
        found = []

        def add(hwnd, _):
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == pi.dwProcessId:
                found.append(hwnd)
                user32.EnumChildWindows(hwnd, enum_proc(child), 0)
            return True

        def child(hwnd, _):
            found.append(hwnd)
            return True
        user32.EnumDesktopWindows(desktop, enum_proc(add), 0)
        return found

    def post(code, down):
        scan = user32.MapVirtualKeyW(code, 0)
        lparam = 1 | (scan << 16) | ((1 << 24) if code in extended else 0) | (0 if down else (3 << 30))
        targets = windows()
        for hwnd in targets:
            user32.PostMessageW(hwnd, 0x0100 if down else 0x0101, code, lparam)  # WM_KEYDOWN / WM_KEYUP
        return bool(targets)

    def press(key, hold=0.08):
        names = keysyms_of(key)
        if len(names) != 1 or names[0] not in vk:
            print(f'key {key}: not supported on Windows (single keys only: {", ".join(sorted(vk))})', flush=True)
            return False
        if not post(vk[names[0]], True):
            return False
        time.sleep(hold)
        post(vk[names[0]], False)
        return True

    try:
        drive(args, keys, press, lambda: kernel32.WaitForSingleObject(pi.hProcess, 0) != 0)
    finally:
        for hwnd in windows():
            user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE
        if kernel32.WaitForSingleObject(pi.hProcess, 15000) != 0:
            kernel32.TerminateProcess(pi.hProcess, 1)
            kernel32.WaitForSingleObject(pi.hProcess, 5000)
        kernel32.CloseHandle(pi.hThread)
        kernel32.CloseHandle(pi.hProcess)
        user32.CloseDesktop(desktop)
        tidy()


def running(names):
    if WINDOWS:
        tasks = subprocess.run(['tasklist'], capture_output=True, text=True).stdout.lower()
        return [n for n in names if n.lower() + '.exe' in tasks]
    return [n for n in names if subprocess.run(['pgrep', '-x', n], capture_output=True).returncode == 0]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', required=True)
    ap.add_argument('--seconds', type=float, required=True)
    ap.add_argument('--interval', type=float, default=0.5)
    ap.add_argument('--keys', default='', help='comma list of <seconds>:<key>[:<hold seconds>]')
    ap.add_argument('--iso', default=None, help='the disc image (default: the one in discs/)')
    ap.add_argument('--app', default=DEFAULT_APP, help=f'PCSX2 (default {DEFAULT_APP})')
    ap.add_argument('--owner-config', default=OWNER_CFG, help=f'PCSX2 settings to copy, only read (default {OWNER_CFG})')
    ap.add_argument('--display', default=':77', help='Linux: the private Xvfb display')
    ap.add_argument('--mc-folder', default=None,
                    help='a folder of PS2 saves (the port\'s card layout) used as slot 1, as a PCSX2 folder card')
    ap.add_argument('--mc-file', default=None, help='a PCSX2 card file (.ps2) whose copy is slot 1')
    ap.add_argument('--xshots', action='store_true',
                    help='Linux: also capture the whole private display every interval (PCSX2\'s own menus are not '
                         'in its screenshots)')
    args = ap.parse_args()

    iso = args.iso or next(iter(sorted(glob.glob(os.path.join(REPO, 'discs', '*.iso')))), None)
    if not iso or not os.path.exists(iso):
        sys.exit('no disc image (pass --iso)')
    if not os.path.isfile(args.app):
        sys.exit(f'PCSX2 not found: {args.app} (pass --app)')
    if args.xshots and WINDOWS:
        sys.exit('--xshots: Linux only')
    busy = running(['dbziw', 'pcsx2-qt'])
    if busy:
        sys.exit(f'already running: {", ".join(busy)} (one game at a time)')
    out = os.path.abspath(args.out)
    if os.path.exists(out) and os.listdir(out):
        sys.exit(f'{out} is not empty: use a new output folder')
    os.makedirs(out, exist_ok=True)
    keys = []
    for item in filter(None, args.keys.split(',')):
        parts = item.split(':')
        keys.append((float(parts[0]), parts[1], float(parts[2]) if len(parts) > 2 else 0.1))
    keys.sort()
    (run_windows if WINDOWS else run_linux)(args, os.path.abspath(iso), out, keys)
    shots = sorted(glob.glob(os.path.join(out, 'snaps', '**', '*.png'), recursive=True))
    print('%d screenshots in %s' % (len(shots), os.path.join(out, 'snaps')))


if __name__ == '__main__':
    main()
