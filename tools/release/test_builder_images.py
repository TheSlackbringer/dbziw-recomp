"""The release builder images (007 step 5): each is pinned by digest, the jobs use the pinned images, and the release jobs download nothing."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
RELEASE = REPO / 'tools/release'
DIGEST = re.compile(r'^ghcr\.io/theslackbringer/[a-z0-9-]+@sha256:[0-9a-f]{64}$')
WORKFLOW = (REPO / '.github/workflows/build.yml').read_text(encoding='utf-8')


def job(name):
    """The text of one job in build.yml."""
    match = re.search(rf'^  {name}:\n(.*?)(?=^  [a-z-]+:\n|\Z)', WORKFLOW, re.M | re.S)
    if not match:
        raise AssertionError(f'no job {name}')
    return match.group(1)


def pin(name):
    path = RELEASE / name
    return path.read_text(encoding='utf-8').strip() if path.exists() else ''


class BuilderImageTest(unittest.TestCase):
    def test_the_toolchain_is_the_approved_image(self):
        # The compiler and libc++ in DBZIW's own image, by its digest (you, 2026-11-05: "Dont use DWXLs image. create its own").
        self.assertRegex(pin('linux-toolchain-image'), DIGEST)
        self.assertIn('/dbziw-release-linux@', pin('linux-toolchain-image'))

    def test_both_builder_images_are_pinned_by_digest(self):
        for name, image in (('linux-builder-image', 'dbziw-release-linux'), ('windows-builder-image', 'dbziw-release-windows')):
            with self.subTest(name):
                self.assertRegex(pin(name), DIGEST, 'publish with builder-image.yml, then write the digest it prints')
                self.assertIn(f'/{image}@', pin(name))

    def test_the_jobs_use_the_pinned_images(self):
        self.assertIn('podman pull "$(cat tools/release/linux-builder-image)"', job('linux-package'))
        self.assertIn(f'image: {pin("windows-builder-image")}\n', job('windows-build'))

    def test_the_windows_job_can_pull_its_image(self):
        # A private image on ghcr.io: without the login and packages: read the job cannot start.
        text = job('windows-build')
        self.assertIn('credentials:\n        username: ${{ github.actor }}\n        password: ${{ secrets.GITHUB_TOKEN }}\n', text)
        self.assertIn('packages: read', text)

    def test_the_release_jobs_contain_no_download_step(self):
        for name in ('linux-package', 'windows-build'):
            with self.subTest(name):
                text = job(name)
                for word in ('curl ', 'wget ', 'urllib', 'urlretrieve', 'apt.llvm.org', 'pacman '):
                    self.assertNotIn(word, text)

    def test_the_package_is_checked_on_both_systems(self):
        self.assertIn('verify_package.py --root dist/squashfs-root --platform linux --smoke', job('linux-package-check'))
        self.assertIn('--platform windows --smoke', job('windows-test'))
        self.assertIn('launcher_install_test.py', job('windows-test'))
        for needed in ('linux-package-check', 'windows-test'):
            self.assertIn(needed, job('draft').split('\n')[1])


def game_data_step():
    """The commands of the linux job's game-data step, as CI runs them."""
    match = re.search(r'- name: No game data in the repository\n\s+run: \|\n((?:          .*\n)+)', job('linux'))
    if not match:
        raise AssertionError('no game-data step in the linux job')
    return ''.join(line[10:] for line in match.group(1).splitlines(True))


@unittest.skipUnless(shutil.which('sh') and shutil.which('git'), 'needs sh and git')
class GameDataStepTest(unittest.TestCase):
    """Runs the step itself: it once passed without checking anything, because git refused the repository and `|| true` hid that."""

    def setUp(self):
        scratch = tempfile.TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        self.root = Path(scratch.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        # The step writes to git's global settings: a throwaway file, never the real ones.
        self.env = {**os.environ, 'GIT_CONFIG_GLOBAL': str(self.root / 'gitconfig'), 'GIT_CONFIG_NOSYSTEM': '1'}

    def git(self, *args):
        subprocess.run(['git', *args], cwd=self.repo, env=self.env, check=True, capture_output=True)

    def track(self, name):
        path = self.repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('made up\n', encoding='utf-8')
        self.git('add', '--', name)

    def step(self, script=None, cwd=None, **env):
        return subprocess.run(['sh', '-e', '-c', script or game_data_step()], cwd=cwd or self.repo, env={**self.env, **env},
                              capture_output=True, text=True)

    def test_a_clean_repository_passes(self):
        self.git('init', '-q')
        self.track('app/source.cpp')
        result = self.step()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_a_tracked_game_file_fails(self):
        self.git('init', '-q')
        self.track('app/source.cpp')
        self.track('game/SLES_553.47')
        result = self.step()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('game/SLES_553.47', result.stdout)

    def test_git_failing_fails_the_step(self):
        outside = self.root / 'not-a-repository'
        outside.mkdir()
        result = self.step(cwd=outside)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_an_empty_listing_fails_the_step(self):
        self.git('init', '-q')
        result = self.step()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('listed nothing', result.stdout)

    def test_the_step_trusts_the_checkout_of_another_owner(self):
        # CI's container checks out as another user; git's own test switch reproduces the refusal seen there.
        self.git('init', '-q')
        self.track('game/SLES_553.47')
        without = ''.join(line + '\n' for line in game_data_step().splitlines() if 'safe.directory' not in line)
        refused = self.step(without, GIT_TEST_ASSUME_DIFFERENT_OWNER='1')
        self.assertNotEqual(refused.returncode, 0, refused.stdout + refused.stderr)
        self.assertIn('dubious ownership', refused.stderr)
        caught = self.step(GIT_TEST_ASSUME_DIFFERENT_OWNER='1')
        self.assertEqual(caught.returncode, 1, caught.stdout + caught.stderr)
        self.assertIn('game/SLES_553.47', caught.stdout)


if __name__ == '__main__':
    unittest.main()
