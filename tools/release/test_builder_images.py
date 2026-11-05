"""The release builder images (007 step 5): each is pinned by digest, the jobs use the pinned images, and the release jobs download nothing."""
from pathlib import Path
import re
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


if __name__ == '__main__':
    unittest.main()
