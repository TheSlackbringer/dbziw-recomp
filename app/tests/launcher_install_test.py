"""Disc-free tests of the launcher's real main, storage and pipeline, with a stand-in window and setup steps (DW5-E's launcher_install_test.py
at e4a7028c in DBZIW's form: every case except the legacy one, which DBZIW does not have)."""
import argparse, contextlib, ctypes, hashlib, json, os, pathlib, queue, shlex, shutil, subprocess, sys, tempfile, threading, unittest, uuid
P = pathlib.Path
CONFIG = None
DISC_NAME = 'Dragon Ball Z: Infinite World (PAL)'
GAME_ELF = 'authored synthetic non-retail bytes\n'


def put(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value.encode('utf-8'))


def hashes(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob('*') if p.is_file()}


class LauncherInstallTests(unittest.TestCase):

    def setUp(self):
        self.root = P(tempfile.mkdtemp(prefix='case-', dir=CONFIG.out)).resolve()
        self.package = self.root / 'package space ø 測試'
        self.package.mkdir()
        self.cwd = self.root / 'unrelated-cwd'
        self.cwd.mkdir()
        self.home = self.root / 'fake-user-data'
        self.home.mkdir()
        self.reports = self.root / 'reports'
        self.reports.mkdir()
        self.env = os.environ.copy()
        for key in ('DBZIW_DATA_DIR', 'APPIMAGE', 'DBZIW_FIXTURE_GUI_FOLDER', 'PS2X_MC_ROOT', 'PS2X_SETTINGS_FILE'):
            self.env.pop(key, None)
        for key in ('HOME', 'USERPROFILE', 'LOCALAPPDATA'):
            self.env[key] = str(self.home)
        self.env['XDG_CONFIG_HOME'] = str(self.home / 'config-home')
        self.env['XDG_DATA_HOME'] = str(self.home / 'data-home')
        self.env['DBZIW_FIXTURE_REPORT'] = str(self.reports)
        self.env['DBZIW_FIXTURE_GAME'] = str(CONFIG.game)
        self.suffix = '.exe' if os.name == 'nt' else ''
        self.launcher = self.package / ('DBZIW' + self.suffix)
        self.fill_package(self.package, self.launcher)
        self.original_user = hashes(self.home)
        self.expected_user = dict(self.original_user)
        self.serial = 0

    def fill_package(self, package, launcher):
        launcher.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(CONFIG.launcher, launcher)
        if os.name == 'nt':
            for dll in CONFIG.launcher.parent.glob('*.dll'):
                shutil.copy2(dll, launcher.parent / dll.name)
        put(package / 'app/CMakeLists.txt', '# synthetic package sources\n')
        put(package / 'recomp/dbz_iw.toml', '# synthetic packaged profile, must not be overwritten\n')
        put(package / 'external/keep', 'synthetic\n')
        put(package / 'BUILD_ID', 'fixture-v1\n')
        put(package / 'README.txt', 'fixture documentation')
        if os.name == 'nt':
            tools = ['tools/ps2_recomp.exe', 'tools/cmake/bin/cmake.exe', 'tools/ninja.exe', 'tools/llvm-mingw/bin/x86_64-w64-mingw32-clang++.exe']
        else:
            tools = ['tools/ps2_recomp', 'tools/cmake/bin/cmake', 'tools/ninja', 'tools/clang/bin/clang', 'tools/clang/bin/clang++',
                     'tools/ffmpeg/include/libavcodec/avcodec.h']
        for name in tools:
            put(package / name, 'synthetic tool\n')

    def tearDown(self):
        self.assertEqual(hashes(self.home), self.expected_user, 'only declared saves, settings and location records may change in user folders')

    def game_files(self, root):
        put(root / 'logs/game.log', '')
        put(root / 'game/SLES_553.47', GAME_ELF)
        put(root / 'game/dbziw.verified', f'dbziw install 1\n{DISC_NAME}\n1 files\n')

    def installed(self, root, mode='portable', save=True):
        """A finished install: game files plus a build that the launcher's own pipeline recorded, against this package."""
        self.game_files(root)
        if save:
            put(root / 'user-data/mc0/save', 'save bytes that must survive\n')
            put(root / 'user-data/dbziw.ini', 'settings bytes that must survive\n')
        put(root / 'install.json', json.dumps({'version': 1, 'save_mode': 'portable'}))
        self.launch('--no-window', '--rebuild', '--data-dir', root)
        self.assertEqual((root / 'build/built-from.txt').read_text(encoding='utf-8').strip(), (self.package_root() / 'BUILD_ID').read_text().strip())
        if mode == 'normal':
            put(root / 'install.json', json.dumps({'version': 1, 'save_mode': 'normal'}))
            shutil.rmtree(root / 'user-data', ignore_errors=True)

    def package_root(self):
        return self.launcher.parent if (self.launcher.parent / 'BUILD_ID').exists() else self.package

    def launch(self, *args, expected=0, game=False):
        """Wait for real child completion before relocating its executable or working folder."""
        self.serial += 1
        report = self.reports / 'game.json'
        report.unlink(missing_ok=True)
        env = self.env.copy()
        handle = ack = process_handle = None
        if os.name == 'nt' and game:
            kernel = ctypes.WinDLL('kernel32', use_last_error=True)
            kernel.CreateEventW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_wchar_p]
            kernel.CreateEventW.restype = ctypes.c_void_p
            kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
            kernel.OpenProcess.restype = ctypes.c_void_p
            kernel.SetEvent.argtypes = [ctypes.c_void_p]
            event = 'Local\\DBZIW-install-test-' + uuid.uuid4().hex
            handle = kernel.CreateEventW(None, True, False, event)
            ack = kernel.CreateEventW(None, True, False, event + '-ack')
            self.assertTrue(handle and ack)
            env['DBZIW_FIXTURE_EVENT'] = event
            env['DBZIW_FIXTURE_ACK_EVENT'] = event + '-ack'
        try:
            # File-backed output lets us wait for the launcher without waiting for pipe EOF from its detached child.
            log = self.root / f'launch-{self.serial}.log'
            with log.open('wb') as diagnostics:
                process = subprocess.run([str(self.launcher), *map(str, args)], cwd=self.cwd, env=env,
                                         stdout=diagnostics, stderr=subprocess.STDOUT, timeout=60)
            process.stdout = log.read_text(encoding='utf-8', errors='replace')
            process.stderr = ''
            self.assertEqual(process.returncode, expected, process.stdout)
            if game:
                if handle:
                    self.assertEqual(kernel.WaitForSingleObject(handle, 10000), 0, 'native detached child completed')
                self.assertTrue(report.is_file(), 'actual native child wrote report')
                result = json.loads(report.read_text(encoding='utf-8'))
                if handle:
                    process_handle = kernel.OpenProcess(0x00101000, False, result['pid'])
                    self.assertTrue(process_handle, 'retain the actual child before allowing its exit')
                    kernel.SetEvent(ack)
                    self.assertEqual(kernel.WaitForSingleObject(process_handle, 10000), 0, 'child exit completes before relocation')
                return result
            self.assertFalse(report.exists(), 'no game started for failure/build-only route')
            return process
        finally:
            if ack:
                kernel.SetEvent(ack)
                kernel.CloseHandle(ack)
            if process_handle:
                kernel.CloseHandle(process_handle)
            if handle:
                kernel.CloseHandle(handle)

    def check_game(self, report, root, user=None, config=None):
        user = user or root / 'user-data'
        config = config or user
        self.assertEqual(P(report['cwd']), root / 'game')
        self.assertEqual(report['args'][1:], ['./SLES_553.47'], 'the game gets its boot file')
        self.assertEqual(P(report['mc_root']), user / 'mc0')
        self.assertEqual(P(report['settings']), config / 'dbziw.ini')
        self.assertIn('fixture game output', (root / 'logs/game.log').read_text(encoding='utf-8'), 'the game writes its output to logs/game.log')

    def normal_paths(self):
        if os.name == 'nt':
            return self.home / 'DBZIW/user-data', self.home / 'DBZIW/config'
        return self.home / 'data-home/dbziw/user-data', self.home / 'config-home/dbziw/config'

    def expect_user(self, path, contents):
        self.expected_user[str(path.relative_to(self.home))] = hashlib.sha256(contents.encode('utf-8')).hexdigest()

    def expect_index(self, roots, preferred=None):
        _, config = self.normal_paths()
        quoted = ',\n    '.join(json.dumps(p.resolve().as_posix(), ensure_ascii=False) for p in roots)
        text = '{\n  "version": 1,\n  "installations": [\n    ' + quoted + '\n  ]'
        if preferred is not None:
            text += ',\n  "preferred": ' + json.dumps(preferred.resolve().as_posix(), ensure_ascii=False)
        self.expect_user(config / 'installations.json', text + '\n}\n')

    def move_package(self, target):
        self.package.rename(target)
        self.launcher = target / self.launcher.relative_to(self.package)
        self.package = target

    def test_default_fast_path_and_whole_install_move_then_rebuild(self):
        data = self.package / 'dbziw-data'
        self.installed(data)
        original = hashes(data / 'user-data')
        self.check_game(self.launch(game=True), data)
        moved = self.root / 'moved package ø 測試'
        self.move_package(moved)
        data = moved / 'dbziw-data'
        self.assertFalse((self.root / 'package space ø 測試').exists())
        self.check_game(self.launch(game=True), data)
        self.assertEqual(hashes(data / 'user-data'), original)
        self.launch('--no-window', '--rebuild')
        self.assertEqual(hashes(data / 'user-data'), original)
        self.assertEqual((moved / 'recomp/dbz_iw.toml').read_text(), '# synthetic packaged profile, must not be overwritten\n')
        self.check_game(self.launch(game=True), data)

    def test_same_invocation_gui_choice_and_next_run_fast_path(self):
        selected = self.root / 'custom ø 測試'
        self.installed(selected, mode='normal')
        original = (selected / 'game/SLES_553.47').read_bytes()
        user, config = self.normal_paths()
        self.env['DBZIW_FIXTURE_GUI_FOLDER'] = str(selected)
        self.check_game(self.launch(game=True), selected, user, config)
        self.expect_index([selected], selected)
        self.env.pop('DBZIW_FIXTURE_GUI_FOLDER')
        self.assertEqual((selected / 'build/built-from.txt').read_text(encoding='utf-8').strip(), 'fixture-v1')
        self.check_game(self.launch(game=True), selected, user, config)
        self.assertEqual((selected / 'game/SLES_553.47').read_bytes(), original)
        self.assertFalse((self.package / 'dbziw-data').exists())

    def test_opening_launcher_leaves_no_unused_default_folder(self):
        result = self.launch('--no-window', expected=1)
        self.assertIn('No disc image', result.stdout)
        self.assertFalse((self.package / 'dbziw-data').exists())

    def test_overrides_and_external_choice_survive_launcher_only_move(self):
        selected = self.root / 'remembered'
        self.installed(selected, mode='normal')
        user, config = self.normal_paths()
        self.check_game(self.launch('--use-installation', selected, game=True), selected, user, config)
        self.expect_index([selected], selected)
        envroot = self.root / 'environment ø 測試'
        cli = self.root / 'command ø 測試'
        self.installed(envroot, save=False)
        self.installed(cli, save=False)
        self.env['DBZIW_DATA_DIR'] = str(envroot)
        self.check_game(self.launch(game=True), envroot)
        self.check_game(self.launch('--data-dir', cli, game=True), cli)
        self.env.pop('DBZIW_DATA_DIR')
        self.move_package(self.root / 'launcher moved')
        self.check_game(self.launch(game=True), selected, user, config)

    def test_in_place_update_keeps_saves(self):
        data = self.package / 'dbziw-data'
        self.installed(data)
        saves = hashes(data / 'user-data')
        put(self.package / 'BUILD_ID', 'fixture-v2\n')
        self.assertIn('setup required', self.launch('--check-ready', expected=3).stdout)
        self.launch('--no-window')
        self.assertEqual((data / 'build/built-from.txt').read_text(encoding='utf-8').strip(), 'fixture-v2')
        self.assertEqual(hashes(data / 'user-data'), saves)
        self.assertIn('ready', self.launch('--check-ready').stdout)
        self.check_game(self.launch(game=True), data)

    def test_a_newer_date_alone_reads_the_package_once_and_stays_ready(self):
        data = self.package / 'dbziw-data'
        self.installed(data)
        stamp = (data / 'build/ready.stamp').read_bytes()
        os.utime(self.package / 'app/CMakeLists.txt', (1700000000, 1700000000))
        self.assertIn('ready', self.launch('--check-ready').stdout)
        self.assertNotEqual((data / 'build/ready.stamp').read_bytes(), stamp, 'the full check renews the stamp')
        self.assertIn('ready', self.launch('--check-ready').stdout)

    def test_changed_package_file_needs_a_rebuild_with_no_iso(self):
        data = self.package / 'dbziw-data'
        self.installed(data)
        put(self.package / 'app/CMakeLists.txt', '# a changed source of the same release\n')
        self.assertIn('setup required', self.launch('--check-ready', expected=3).stdout)
        self.launch('--no-window')
        self.check_game(self.launch(game=True), data)

    def test_malformed_missing_and_file_destinations_never_fall_back(self):
        _, config = self.normal_paths()
        index = config / 'installations.json'
        put(index, '{broken')
        self.expect_user(index, '{broken')
        result = self.launch('--no-window', expected=1)
        self.assertIn('error', result.stdout.lower())
        self.assertFalse((self.package / 'dbziw-data').exists())
        invalid = self.root / 'is-a-file'
        put(invalid, 'preserve')
        result = self.launch('--no-window', '--data-dir', invalid, expected=1)
        self.assertIn(invalid.name, result.stdout)
        self.assertFalse((self.package / 'game').exists())
        self.assertEqual(invalid.read_text(encoding='utf-8'), 'preserve')

    @contextlib.contextmanager
    def read_only(self, folder):
        if os.name == 'nt':
            import csv, io
            user = subprocess.run(['whoami', '/user', '/fo', 'csv', '/nh'], capture_output=True, text=True, check=True)
            sid = next(csv.reader(io.StringIO(user.stdout)))[1]
            result = subprocess.run(['icacls', str(folder), '/deny', '*' + sid + ':(OI)(CI)(WD,AD,WEA,WA)'], capture_output=True)
            self.assertEqual(result.returncode, 0, 'set test-only write-deny ACL')
            try:
                yield
            finally:
                result = subprocess.run(['icacls', str(folder), '/remove:d', '*' + sid], capture_output=True)
                self.assertEqual(result.returncode, 0, 'restore test-only ACL')
        else:
            mode = folder.stat().st_mode
            folder.chmod(0o555)
            try:
                yield
            finally:
                folder.chmod(mode)

    @unittest.skipIf(os.name != 'nt' and os.geteuid() == 0, 'root ignores folder permissions')
    def test_actual_write_denial_fails_before_work(self):
        selected = self.root / 'denied'
        selected.mkdir()
        with self.read_only(selected):
            probe = subprocess.run([sys.executable, '-c', 'import pathlib,sys; pathlib.Path(sys.argv[1]).write_text("forbidden")',
                                    str(selected / 'access-proof')], capture_output=True)
            self.assertNotEqual(probe.returncode, 0, 'test process really cannot write selected folder')
            self.launch('--no-window', '--data-dir', selected, expected=1)
        self.assertEqual(list(selected.iterdir()), [])

    @unittest.skipIf(os.name != 'nt' and os.geteuid() == 0, 'root ignores folder permissions')
    def test_manage_changes_saves_with_read_only_launcher_folder(self):
        installed = self.root / 'writable installation'
        self.installed(installed, mode='normal')
        user, _ = self.normal_paths()
        put(user / 'mc0/save', 'normal progress')
        self.expect_user(user / 'mc0/save', 'normal progress')
        with self.read_only(self.package):
            self.launch('--manage', '--no-window', '--use-installation', installed, '--save-mode', 'portable')
        self.assertEqual(json.loads((installed / 'install.json').read_text())['save_mode'], 'portable')
        self.assertEqual((installed / 'user-data/mc0/save').read_bytes(), (user / 'mc0/save').read_bytes())

    def test_manage_copies_portable_installation_without_launching(self):
        source = self.root / 'old installation ø'
        self.installed(source)
        original = hashes(source)
        self.launch('--manage', '--no-window', '--import-installation', source, '--save-mode', 'portable')
        copied = self.package / 'dbziw-data'
        self.assertEqual(hashes(source), original)
        self.assertEqual((copied / 'game/SLES_553.47').read_bytes(), (source / 'game/SLES_553.47').read_bytes())
        self.assertEqual((copied / 'user-data/mc0/save').read_bytes(), (source / 'user-data/mc0/save').read_bytes())
        self.assertEqual((copied / 'user-data/dbziw.ini').read_bytes(), (source / 'user-data/dbziw.ini').read_bytes())
        self.assertEqual(json.loads((copied / 'install.json').read_text())['save_mode'], 'portable')
        self.assertEqual((copied / 'build/built-from.txt').read_text().strip(), 'fixture-v1')
        self.assertFalse((copied / 'build/fixture-root.txt').read_text(encoding='utf-8') == '', 'the copy brought the build along')

    def test_normal_fresh_zip_and_portable_switch_preserve_progress(self):
        installed = self.package / 'dbziw-data'
        self.installed(installed, mode='normal')
        user, config = self.normal_paths()
        save = P('mc0/BESLES-55347/progress')
        put(user / save, 'normal progress')
        self.expect_user(user / save, 'normal progress')
        put(config / 'dbziw.ini', 'settings bytes')
        self.expect_user(config / 'dbziw.ini', 'settings bytes')
        self.check_game(self.launch(game=True), installed, user, config)
        self.expect_index([installed], installed)
        fresh = self.root / 'fresh download ø'
        shutil.copytree(self.package, fresh, ignore=shutil.ignore_patterns('dbziw-data'))
        self.package = fresh
        self.launcher = fresh / self.launcher.name
        self.check_game(self.launch(game=True), installed, user, config)
        self.assertFalse((fresh / 'dbziw-data').exists(), 'fresh ZIP reused the discovered installation')
        self.launch('--manage', '--no-window', '--save-mode', 'portable')
        self.assertEqual(json.loads((installed / 'install.json').read_text(encoding='utf-8'))['save_mode'], 'portable')
        portable = installed / 'user-data'
        self.assertEqual((portable / save).read_text(encoding='utf-8'), 'normal progress')
        self.assertEqual((portable / 'dbziw.ini').read_text(encoding='utf-8'), 'settings bytes')
        self.assertEqual((user / save).read_text(encoding='utf-8'), 'normal progress', 'original normal saves retained')
        self.check_game(self.launch(game=True), installed, portable, portable)
        put(portable / save, 'portable progress')
        before = (installed / 'install.json').read_bytes()
        failed = self.launch('--manage', '--no-window', '--save-mode', 'normal', expected=1)
        self.assertIn('conflict', failed.stdout.lower())
        self.assertEqual((installed / 'install.json').read_bytes(), before, 'failed switch retained portable mode')
        self.launch('--manage', '--no-window', '--save-mode', 'normal', '--save-conflict', 'source')
        self.assertEqual((user / save).read_text(encoding='utf-8'), 'portable progress')
        self.expect_user(user / save, 'portable progress')
        backups = list((user / save).parent.glob('progress.dbziw-backup-*'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_text(encoding='utf-8'), 'normal progress')
        self.expect_user(backups[0], 'normal progress')
        self.check_game(self.launch(game=True), installed, user, config)

    def test_mode_switch_rejects_a_running_native_game(self):
        installed = self.package / 'dbziw-data'
        self.installed(installed)
        metadata = (installed / 'install.json').read_bytes()
        process = subprocess.Popen([str(installed / 'build' / ('dbziw' + self.suffix)), 'hold'], cwd=self.cwd, env=self.env,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ready = queue.Queue()
        reader = threading.Thread(target=lambda: ready.put(process.stdout.readline()), daemon=True)
        reader.start()
        try:
            self.assertEqual(ready.get(timeout=10).strip(), b'ready', 'owned native game reached its input wait')
            failed = self.launch('--manage', '--no-window', '--save-mode', 'normal', expected=1)
            self.assertIn('Close the game', failed.stdout)
            self.assertEqual((installed / 'install.json').read_bytes(), metadata)
            self.assertEqual((installed / 'user-data/mc0/save').read_text(encoding='utf-8'), 'save bytes that must survive\n')
            _, errors = process.communicate(b'\n', timeout=30)
            self.assertEqual(process.returncode, 0, errors)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=30)
            reader.join(timeout=10)
            self.assertFalse(reader.is_alive(), 'owned native ready-reader completed')
            for stream in (process.stdin, process.stdout, process.stderr):
                stream.close()

    def test_custom_portable_update_keeps_metadata_inside_the_installation(self):
        selected = self.root / 'custom portable'
        self.installed(selected)
        self.env['DBZIW_FIXTURE_GUI_FOLDER'] = str(selected)
        self.launch('--no-window', '--rebuild')
        self.assertEqual(json.loads((selected / 'install.json').read_text())['save_mode'], 'portable')
        self.assertEqual((selected / 'user-data/mc0/save').read_text(), 'save bytes that must survive\n')

    def test_custom_normal_update_records_folder_in_user_profile(self):
        selected = self.root / 'custom normal'
        self.installed(selected, mode='normal')
        self.env['DBZIW_FIXTURE_GUI_FOLDER'] = str(selected)
        self.launch('--no-window', '--rebuild')
        self.expect_index([selected], selected)
        self.env.pop('DBZIW_FIXTURE_GUI_FOLDER')
        self.assertEqual(P(json.loads(self.launch('--status').stdout)['installation']), selected)

    def test_full_uninstall_helper_removes_installed_launcher(self):
        installed = self.root / 'DBZIW-Recomp'
        data = installed / 'dbziw-data'
        self.game_files(data)
        put(data / 'install.json', json.dumps({'version': 1, 'save_mode': 'portable'}))
        put(data / 'user-data/mc0/progress', 'portable progress')
        put(installed / 'README.txt', 'fixture documentation')
        if os.name == 'nt':
            payload = data / 'package'
            put(payload / 'BUILD_ID', 'fixture package')
            (payload / 'launcher').mkdir()
            shutil.copy2(CONFIG.launcher, payload / 'launcher/DBZIW.exe')
            put(payload / 'tools/packaged', 'installed tool')
            put(payload / 'tools/unrelated', 'user file')
            put(payload / 'package-files.txt', 'BUILD_ID\nlauncher/DBZIW.exe\ntools/packaged\n')
            installed_launcher = installed / 'DBZIW.exe'
            shutil.copy2(CONFIG.bootstrap, installed_launcher)
        else:
            installed_launcher = installed / 'DBZIW.AppImage'
            put(installed_launcher, 'fixture installed AppImage')
        work = self.root / 'uninstall worker'
        work.mkdir()
        helper = work / CONFIG.uninstaller.name
        shutil.copy2(CONFIG.uninstaller, helper)
        log = work / 'uninstall.log'
        hold = [sys.executable, '-c', 'import sys; sys.stdin.read()']
        with subprocess.Popen(hold, stdin=subprocess.PIPE) as wizard, subprocess.Popen(hold, stdin=subprocess.PIPE) as outer:
            with subprocess.Popen([str(helper), str(data), 'keep', str(wizard.pid), str(outer.pid), sys.executable, str(log)],
                                  cwd=work, env=self.env) as worker:
                try:
                    wizard.stdin.close()
                    self.assertEqual(wizard.wait(timeout=10), 0)
                    with self.assertRaises(subprocess.TimeoutExpired):
                        worker.wait(timeout=0.25)
                    self.assertTrue(installed_launcher.is_file())
                finally:
                    outer.stdin.close()
                try:
                    result = worker.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    worker.kill()
                    raise
        self.assertEqual(result, 0, log.read_text() if log.exists() else 'uninstall worker exit status')
        self.assertFalse(installed_launcher.exists())
        expected = {'dbziw-data/user-data/mc0/progress'}
        if os.name == 'nt':
            expected.add('dbziw-data/package/tools/unrelated')
        self.assertEqual({p.relative_to(installed).as_posix() for p in installed.rglob('*') if p.is_file()}, expected)
        self.assertEqual((data / 'user-data/mc0/progress').read_text(), 'portable progress')

    def test_custom_install_launcher_works_after_removing_download_and_moving_the_folder(self):
        download = self.root / 'download'
        download.mkdir()
        parent = self.root / 'chosen parent'
        installed = parent / 'DBZIW-Recomp'
        data = installed / 'dbziw-data'
        if os.name == 'nt':
            self.assertIsNotNone(CONFIG.bootstrap, 'native Windows bootstrap fixture')
            payload = download / 'dbziw-data/package'
            self.fill_package(payload, payload / 'launcher/DBZIW.exe')
            put(payload / 'LICENSE', 'fixture documentation')
            put(payload / 'LICENSES/x.txt', 'fixture documentation')
            put(payload / 'package-files.txt', 'BUILD_ID\n')
            put(download / 'README.txt', 'fixture documentation')
            source = download / 'DBZIW.exe'
            shutil.copy2(CONFIG.bootstrap, source)
            self.launcher = payload / 'launcher/DBZIW.exe'
            launcher = installed / 'DBZIW.exe'
        else:
            source = download / 'downloaded.AppImage'
            put(source, '#!/bin/sh\nexport APPIMAGE="$0"\nexec ' + shlex.quote(str(self.launcher)) + ' "$@"\n')
            source.chmod(0o755)
            self.env['APPIMAGE'] = str(source)
            launcher = installed / 'DBZIW.AppImage'
        self.game_files(data)
        put(data / 'install.json', json.dumps({'version': 1, 'save_mode': 'portable'}))
        put(data / 'user-data/mc0/progress', 'portable progress')
        self.env['DBZIW_FIXTURE_GUI_FOLDER'] = str(data)
        self.launch('--no-window', '--rebuild')
        self.env.pop('DBZIW_FIXTURE_GUI_FOLDER')
        self.assertEqual(launcher.read_bytes(), source.read_bytes())
        self.assertEqual((installed / 'README.txt').read_text(), 'fixture documentation')
        shutil.rmtree(download)
        self.env.pop('APPIMAGE', None)
        if os.name == 'nt':
            self.assertTrue((data / 'package/launcher/DBZIW.exe').is_file(), 'the package moved into the chosen folder')
            self.launcher = launcher
            self.check_game(self.launch(game=True), data)
            self.launch('--manage', '--no-window', '--save-mode', 'portable')
            self.assertEqual((data / 'user-data/mc0/progress').read_text(), 'portable progress')
            # The whole folder moves and keeps working, with no ISO.
            moved = self.root / 'other drive' / 'DBZIW-Recomp'
            moved.parent.mkdir()
            installed.rename(moved)
            self.launcher = moved / 'DBZIW.exe'
            self.check_game(self.launch(game=True), moved / 'dbziw-data')
            self.assertEqual((moved / 'dbziw-data/user-data/mc0/progress').read_text(), 'portable progress')

    def test_portable_copy_to_new_release_retains_mode_without_user_folder_writes(self):
        installed = self.package / 'dbziw-data'
        self.installed(installed)
        self.check_game(self.launch(game=True), installed)
        target = self.root / 'new portable release'
        shutil.copytree(self.package, target)
        self.package = target
        self.launcher = target / self.launcher.name
        self.check_game(self.launch(game=True), target / 'dbziw-data')
        self.assertEqual((target / 'dbziw-data/user-data/mc0/save').read_text(encoding='utf-8'), 'save bytes that must survive\n')
        self.assertEqual(hashes(self.home), self.original_user)

    def test_multiple_recorded_installations_require_selection(self):
        left = self.root / 'one'
        right = self.root / 'two'
        self.installed(left, mode='normal')
        self.installed(right, mode='normal')
        user, config = self.normal_paths()
        index = json.dumps({'version': 1, 'installations': [left.as_posix(), right.as_posix()]}, indent=2) + '\n'
        put(config / 'installations.json', index)
        self.expect_user(config / 'installations.json', index)
        failed = self.launch('--no-window', expected=1)
        self.assertIn('Several existing', failed.stdout)
        self.assertFalse((self.package / 'dbziw-data/game').exists())
        self.check_game(self.launch('--use-installation', left, game=True), left, user, config)
        self.expect_index([left, right], left)

    def test_fresh_update_uses_the_last_used_installation(self):
        left = self.root / 'older installation'
        right = self.root / 'active installation'
        for root in (left, right):
            self.installed(root, mode='normal')
        user, config = self.normal_paths()
        self.check_game(self.launch('--use-installation', left, game=True), left, user, config)
        self.expect_index([left], left)
        self.check_game(self.launch('--use-installation', right, game=True), right, user, config)
        self.expect_index([left, right], right)
        fresh = self.root / 'newly downloaded update'
        shutil.copytree(self.package, fresh, ignore=shutil.ignore_patterns('dbziw-data'))
        self.package = fresh
        self.launcher = fresh / self.launcher.name
        status = json.loads(self.launch('--status').stdout)
        self.assertEqual(P(status['installation']), right)
        self.check_game(self.launch(game=True), right, user, config)
        self.assertFalse((fresh / 'dbziw-data').exists())

    def test_status_is_json_and_help_names_every_option(self):
        status = json.loads(self.launch('--status').stdout)
        self.assertEqual(set(status), {'installation', 'save_mode', 'user_data', 'config', 'recorded'})
        self.assertFalse(status['recorded'])
        help_text = self.launch('--help').stdout
        for option in ('--iso', '--data-dir', '--no-window', '--install', '--rebuild', '--manage', '--status', '--save-mode', '--save-conflict',
                       '--use-installation', '--import-installation'):
            self.assertIn(option, help_text)
        self.launch('--not-an-option', expected=1)

    @unittest.skipIf(os.name == 'nt', 'Linux AppImage discovery and mount lifetime need a Linux native host')
    def test_original_appimage_and_mount_lifetime(self):
        outer = self.root / 'download'
        outer.mkdir()
        image = outer / 'DBZIW.AppImage'
        put(image, 'synthetic original AppImage identity')
        self.installed(outer / 'dbziw-data')
        self.env['APPIMAGE'] = str(image)
        self.check_game(self.launch(game=True), outer / 'dbziw-data')
        self.assertTrue((self.reports / 'game.json').exists(), 'launcher waited for child completion')


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser()
    parser.add_argument('--launcher', required=True, type=P)
    parser.add_argument('--game', required=True, type=P)
    parser.add_argument('--out', required=True, type=P)
    parser.add_argument('--bootstrap', type=P)
    parser.add_argument('--uninstaller', type=P, required=True)
    CONFIG, rest = parser.parse_known_args()
    CONFIG.launcher = CONFIG.launcher.resolve()
    CONFIG.game = CONFIG.game.resolve()
    CONFIG.uninstaller = CONFIG.uninstaller.resolve()
    CONFIG.bootstrap = CONFIG.bootstrap.resolve() if CONFIG.bootstrap else None
    CONFIG.out.mkdir(parents=True, exist_ok=True)
    CONFIG.out = CONFIG.out.resolve()
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)
