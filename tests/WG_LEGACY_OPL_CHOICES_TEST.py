"""Linux guided-menu and Make recipe regressions. No compiler/Docker execution."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]

class LegacyOplChoices(unittest.TestCase):
    def test_guided_choices(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ('lib/wolf3d/CMakeLists.txt', 'third_party/SDL3/CMakeLists.txt'):
                p = root / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.touch()
            script = root / 'scripts/linux/configure-build.sh'
            script.parent.mkdir(parents=True)
            script.write_text((ROOT / 'scripts/linux/configure-build.sh').read_text())
            bin_dir = root / 'bin'
            bin_dir.mkdir()
            for name in ('git', 'gcc', 'clang', 'cmake', 'make', 'docker'):
                stub = bin_dir / name
                stub.write_text('#!/bin/sh\nexit ' + ('1' if name == 'git' else '0') + '\n')
                stub.chmod(0o755)
            env = dict(os.environ, PATH=str(bin_dir) + ':' + os.environ['PATH'])
            for selection, target, default in ((11, 'dos-release', 'auto'), (12, 'windows-win9x', 'auto'),
                                                (13, 'windows-win2k', 'auto'), (14, 'windows-xp', 'auto'),
                                                (1, 'sdl3-release', 'auto'), (2, 'console-release', 'auto'),
                                                (5, 'portable', 'auto'), (8, 'musl-sdl3', 'auto'), (9, 'musl-console', 'auto')):
                choices = (('', 'dbopl,silent,adlib'), ('2', 'nuked,dbopl,silent,adlib')) if selection >= 11 else (('', 'nuked,dbopl,silent,adlib'), ('2', 'nuked,dbopl,silent'))
                for choice, expected in choices:
                    with self.subTest(target=target, drivers=expected):
                        answers = [str(selection)]
                        if selection in (1, 2): answers.append('')  # compiler
                        if selection == 1: answers.append('')  # SDL source
                        answers += [choice, '']
                        if selection not in (11, 12):
                            answers.append('')  # parallel jobs
                        answers.append('n')
                        result = subprocess.run(['bash', str(script)], input='\n'.join(answers)+'\n',
                                                text=True, capture_output=True, env=env, timeout=10)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn('OPL drivers: ' + expected + ' (default: ' + default + ')', result.stdout)
                        command = result.stdout.replace('\\,', ',')
                        prefix = {11:'DOS_', 12:'', 13:'WIN2K_', 14:'WINXP_'}.get(selection, '')
                        self.assertIn(prefix + 'OPL_DRIVERS=' + expected, command)
                        self.assertIn(prefix + 'OPL_DEFAULT=' + default, command)

    def test_linux_and_modern_windows_defaults(self):
        for target in ('configure-sdl3', 'configure-console'):
            for drivers, value in ((None, 'ON'), ('nuked,dbopl,silent', 'OFF')):
                args = ['make', '-n', target] + ([] if drivers is None else ['OPL_DRIVERS=' + drivers])
                output = subprocess.check_output(args, cwd=ROOT, text=True)
                self.assertIn('-DWG_ENABLE_OPL_ADLIB=' + value, output)
        output = subprocess.check_output(['make', '-n', 'windows-win7'], cwd=ROOT, text=True)
        self.assertIn('W3P_WINDOWS_CROSS_OPL_DRIVERS="nuked,dbopl,silent"', output)

    def test_make_defaults_and_overrides(self):
        for target, prefix, default in (('windows-win2k', 'WIN2K_', 'auto'), ('windows-xp', 'WINXP_', 'auto'),
                                         ('windows-win9x', 'WIN9X_', 'auto')):
            for drivers in (None, 'nuked,dbopl,silent,adlib'):
                args = ['make', '-n', target]
                if drivers:
                    args += [prefix + 'OPL_DRIVERS=' + drivers]
                result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, check=True)
                self.assertIn('OPL_DRIVERS="' + (drivers or 'dbopl,silent,adlib') + '"', result.stdout)
                self.assertIn('DEFAULT_OPL="' + default + '"', result.stdout)
        result = subprocess.run(['make', '-n', 'windows-xp', 'OPL_DRIVERS=silent', 'OPL_DEFAULT=silent'],
                                cwd=ROOT, text=True, capture_output=True, check=True)
        self.assertIn('OPL_DRIVERS="silent"', result.stdout)
        self.assertIn('DEFAULT_OPL="silent"', result.stdout)

if __name__ == '__main__':
    unittest.main()
