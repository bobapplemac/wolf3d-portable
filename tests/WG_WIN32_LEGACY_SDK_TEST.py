"""Compile the legacy Win32 host with and without the newer SDK metric."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class LegacySDKTests(unittest.TestCase):
    def test_monitor_metric_compatibility(self):
        compiler = os.environ.get('W3P_MINGW_CC')
        if not compiler:
            compiler = shutil.which('x86_64-w64-mingw32-gcc')
        if not compiler and Path(r'C:\msys64\ucrt64\bin\gcc.exe').exists():
            compiler = r'C:\msys64\ucrt64\bin\gcc.exe'
        if not compiler:
            self.skipTest('MinGW compiler required; set W3P_MINGW_CC')
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='legacy-sdk-', dir=ROOT / 'build') as temporary:
            base = Path(temporary)
            source = base / 'host.c'
            env = dict(os.environ)
            env['PATH'] = str(Path(compiler).parent) + os.pathsep + env['PATH']
            host = (ROOT / 'platforms/win32/WG_WIN32.c').as_posix()
            for mode in ('current-sdk', 'missing-metric', 'existing-definition'):
                with self.subTest(mode=mode):
                    prefix = '#include <windows.h>\n'
                    if mode != 'current-sdk':
                        prefix += '#undef SM_CMONITORS\n'
                    expected = 80
                    if mode == 'existing-definition':
                        prefix += '#define SM_CMONITORS 123\n'
                        expected = 123
                    source.write_text(prefix + '#include "' + host + '"\n'
                                      '#if SM_CMONITORS != %d\n#error Unexpected metric value\n#endif\n' % expected)
                    result = subprocess.run([compiler, '-fsyntax-only', '-std=c89',
                                             '-DWG_LEGACY_WIN32=1', '-DUNICODE', '-D_UNICODE',
                                             '-I', str(ROOT / 'lib/wolf3d/include'), str(source)],
                                            env=env, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

if __name__ == '__main__':
    unittest.main()
