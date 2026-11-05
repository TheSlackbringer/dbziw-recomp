"""The Windows package layout (DW5-E's test_make_release.py in DBZIW's form). Offline; no game data, external tools or network."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import make_release as release


class WindowsPackageTest(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name)
        self.repo = self.root / 'repo'
        self.build = self.root / 'build'
        self.recomp = self.root / 'compiler'
        (self.recomp / 'ps2xRecomp').mkdir(parents=True)
        self.build.mkdir()
        self.repo.mkdir()
        (self.build / 'dbziw_setup.exe').write_bytes(b'MZsynthetic setup')
        (self.build / 'DBZIW-bootstrap.exe').write_bytes(b'MZsynthetic bootstrap')
        (self.build / 'dbziw-uninstall.exe').write_bytes(b'MZsynthetic uninstall helper')
        (self.recomp / 'ps2xRecomp/ps2_recomp.exe').write_bytes(b'MZsynthetic recompiler')
        license_paths = ('LICENSE', 'external/PS2Recomp/LICENSE', 'app/deps/parallel-gs/COPYING.LGPLv3', 'app/deps/parallel-gs/Granite/LICENSE',
                         'app/deps/parallel-gs/Granite/third_party/volk/LICENSE.md')
        for name in license_paths:
            (self.repo / name).parent.mkdir(parents=True, exist_ok=True)
            (self.repo / name).write_text('synthetic license')
        deps = self.root / 'deps'
        for name in ('raylib/LICENSE', 'raylib/src/external/glfw/LICENSE.md', 'imgui/LICENSE.txt', 'rlimgui/LICENSE'):
            (deps / name).parent.mkdir(parents=True, exist_ok=True)
            (deps / name).write_text('synthetic license')
        llvm = self.root / 'llvm'
        llvm.mkdir()
        (llvm / 'LICENSE.TXT').write_text('synthetic license')
        ffmpeg = self.root / 'ffmpeg-min'
        (ffmpeg / 'licenses/ffmpeg').mkdir(parents=True)
        (ffmpeg / 'licenses/ffmpeg/COPYING.LGPLv2.1').write_text('synthetic license')
        with zipfile.ZipFile(self.root / 'ffmpeg-min.zip', 'w') as archive:
            archive.writestr('lib/x', 'synthetic')
        archives = {}
        for name, entries in {'LLVM_MINGW_WIN_ZIP': {'llvm/bin/clang++.exe': b'MZsynthetic'},
                              'CMAKE_WIN_ZIP': {'cmake/bin/cmake.exe': b'MZsynthetic'},
                              'NINJA_WIN_ZIP': {'ninja.exe': b'MZsynthetic'}}.items():
            archives[name] = self.root / f'{name}.zip'
            with zipfile.ZipFile(archives[name], 'w') as archive:
                for entry, data in entries.items():
                    archive.writestr(entry, data)
        self.closure = {}
        for name, value in {'REPO': self.repo, 'LLVM_MINGW_HOST': llvm, 'FFMPEG_MIN_WINDOWS': ffmpeg, **archives}.items():
            self.start(patch.object(release, name, value))
        self.start(patch.object(release, 'disc_file_names', return_value={'SLES_553.47'}))
        self.start(patch.object(release, 'version_for', return_value='fixture'))
        self.start(patch.object(release, 'apache_text', return_value='synthetic'))
        self.start(patch.object(release, 'dep_source', side_effect=lambda build, dep: deps / dep))
        self.start(patch.object(release, 'copy_tracked', side_effect=self.copy_sources))
        self.start(patch.object(release, 'dll_closure', side_effect=lambda exe, search: self.closure.get(Path(exe).name, [])))

    def start(self, patcher):
        patcher.start()
        self.addCleanup(patcher.stop)

    @staticmethod
    def copy_sources(repo, paths, stage, skip=()):
        if 'app' in paths:
            for name in ('app/CMakeLists.txt', 'recomp/dbz_iw.toml', 'tools/windows/llvm-mingw.cmake', 'LICENSE'):
                target = stage / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('synthetic source')
        return 4

    def test_only_the_starter_and_the_readme_sit_outside_dbziw_data(self):
        with zipfile.ZipFile(release.windows(self.build, self.recomp, True)) as contents:
            names = contents.namelist()
            prefix = 'DBZIW-fixture-windows-x64/'
            top = {name[len(prefix):].split('/')[0] for name in names}
            self.assertEqual(top, {'DBZIW.exe', 'README.txt', 'dbziw-data'})
            self.assertEqual(contents.read(prefix + 'DBZIW.exe'), b'MZsynthetic bootstrap')
            package = prefix + 'dbziw-data/package/'
            self.assertEqual(contents.read(package + 'launcher/DBZIW.exe'), b'MZsynthetic setup')
            self.assertEqual(contents.read(package + 'launcher/dbziw-uninstall.exe'), b'MZsynthetic uninstall helper')
            self.assertEqual(contents.read(package + 'tools/ps2_recomp.exe'), b'MZsynthetic recompiler')
            self.assertFalse(any('user-data/' in name or name.endswith('/install.json') for name in names))

    def test_the_inventory_lists_exactly_the_packaged_files(self):
        with zipfile.ZipFile(release.windows(self.build, self.recomp, True)) as contents:
            package = 'DBZIW-fixture-windows-x64/dbziw-data/package/'
            names = {name[len(package):] for name in contents.namelist() if name.startswith(package)}
            listed = set(contents.read(package + 'package-files.txt').decode('utf-8').splitlines())
            self.assertEqual(listed, names - {'package-files.txt'})

    def test_the_checksum_names_the_zip(self):
        archive = release.windows(self.build, self.recomp, True)
        checksum = Path(str(archive) + '.sha256')
        self.assertEqual(checksum.read_text(), f'{release.sha256(archive)}  {archive.name}\n')

    def test_the_starter_and_the_helper_must_need_only_system_dlls(self):
        for name in ('DBZIW-bootstrap.exe', 'dbziw-uninstall.exe'):
            with self.subTest(name):
                self.closure = {name: [self.root / 'libc++.dll']}
                with self.assertRaisesRegex(SystemExit, 'system DLLs'):
                    release.windows(self.build, self.recomp, True)

    def test_a_missing_program_stops_packaging(self):
        for path in (self.build / 'DBZIW-bootstrap.exe', self.build / 'dbziw-uninstall.exe', self.recomp / 'ps2xRecomp/ps2_recomp.exe'):
            with self.subTest(path.name):
                saved = path.read_bytes()
                path.unlink()
                with self.assertRaises(SystemExit):
                    release.windows(self.build, self.recomp, True)
                path.write_bytes(saved)


class LinuxSdkTest(unittest.TestCase):
    def test_the_sdk_keeps_the_static_cxx_archive_of_the_uninstall_helper(self):
        # DW5-E e4a7028c: the helper links libstdc++.a from the exported sysroot; verify_package's SDK rebuild proves it on Linux.
        self.assertIn("source.name == 'libstdc++.a'", (Path(release.__file__).parent / 'export_linux_sdk.py').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
