"""Packaging-only regression test; never load the real InpOut32 DLL."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CMAKE = os.environ.get('W3P_CMAKE', 'cmake')

class InpOutPackageTests(unittest.TestCase):
    def test_profiles_and_integrity(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            vendor = root / 'third_party/InpOut32'
            shutil.copytree(ROOT / 'third_party/InpOut32', vendor)
            cases = [('win2k', 'x86', 'gdi', 'ON', 'ON', True),
                     ('winxp', 'x86', 'gdi', 'ON', 'ON', True),
                     ('win2k', 'x86', 'gdi', 'OFF', 'ON', False),
                     ('winxp', 'x86', 'gdi', 'ON', 'OFF', False),
                     ('win9x', 'x86', 'gdi', 'ON', 'ON', False),
                     ('win7', 'x86', 'gdi', 'ON', 'ON', False),
                     ('winxp', 'x64', 'gdi', 'ON', 'ON', False),
                     ('win7', 'x64', 'gdi', 'ON', 'ON', False),
                     ('win7', 'x64', 'sdl3', 'ON', 'ON', False),
                     ('win10', 'x64', 'sdl3', 'ON', 'ON', False),
                     ('winxp', 'x86', 'sdl3', 'ON', 'ON', False)]
            def stage(index, case):
                platform, arch, backend, enabled, adlib, expected = case
                dist = root / ('package-' + str(index))
                (dist / 'DOCS/LICENSES').mkdir(parents=True)
                (dist / 'DOCS/BUILD.TXT').write_text(
                    f'Platform baseline: {platform}\nArchitecture: {arch}\nBackend: {backend}\n')
                (dist / 'DOCS/NOTICES.TXT').write_text('Base notices\n')
                (dist / 'README.TXT').write_text('Game\n')
                result = subprocess.run([CMAKE, '-DROOT=' + str(root), '-DDIST=' + str(dist),
                                         '-DENABLED=' + enabled, '-DADLIB=' + adlib,
                                         '-P', str(ROOT / 'cmake/W3PPackageInpOut32.cmake')],
                                        capture_output=True, text=True)
                return dist, result
            for index, case in enumerate(cases):
                with self.subTest(case=case):
                    dist, result = stage(index, case)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    included = case[-1]
                    self.assertFalse((dist / 'inpoutx64.dll').exists())
                    self.assertEqual((dist / 'inpout32.dll').exists(), included)
                    self.assertEqual((dist / 'DOCS/LICENSES/INPOUT32.TXT').exists(), included)
                    notices = (dist / 'DOCS/NOTICES.TXT').read_text()
                    self.assertEqual('InpOut32' in notices, included)
                    if included:
                        self.assertEqual((dist / 'inpout32.dll').read_bytes(), (vendor / 'inpout32.dll').read_bytes())
                        self.assertEqual((dist / 'DOCS/LICENSES/INPOUT32.TXT').read_bytes(), (vendor / 'LICENSE.txt').read_bytes())
                        self.assertIn(hashlib.sha256((vendor / 'inpout32.dll').read_bytes()).hexdigest(),
                                      (dist / 'DOCS/BUILD.TXT').read_text())
                        self.assertIn('Administrator', (dist / 'README.TXT').read_text())
            with (vendor / 'inpout32.dll').open('ab') as file:
                file.write(b'tampered')
            dist, result = stage('tampered', cases[0])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('SHA-256 mismatch', result.stderr)
            self.assertFalse((dist / 'inpout32.dll').exists())
            (vendor / 'inpout32.dll').unlink()
            dist, result = stage('missing', cases[0])
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((dist / 'inpout32.dll').exists())

if __name__ == '__main__':
    unittest.main()
