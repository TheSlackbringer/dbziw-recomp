"""Player help must describe the folder, the save choice and the ways to change them (DW5-E's test; no old installs in DBZIW)."""
import unittest

import make_release


class InstallationHelpTests(unittest.TestCase):
    def test_both_packages_describe_the_folder_the_modes_and_management(self):
        for platform, text in (('Windows', make_release.README_WINDOWS), ('Linux', make_release.README_LINUX)):
            with self.subTest(platform=platform):
                self.assertIn('DBZIW-Recomp', text)
                self.assertIn('dbziw-data', text)
                self.assertIn('portable', text.lower())
                self.assertIn('--manage', text)
                self.assertIn('--data-dir', text)
                self.assertIn('DBZIW_DATA_DIR', text)
                self.assertIn('LOCALAPPDATA' if platform == 'Windows' else 'XDG_DATA_HOME', text)
                self.assertIn('uninstall', text.lower())
                self.assertIn('C:\Games\DBZIW-Recomp' if platform == 'Windows' else '~/Games/DBZIW-Recomp', text)


if __name__ == '__main__':
    unittest.main()
