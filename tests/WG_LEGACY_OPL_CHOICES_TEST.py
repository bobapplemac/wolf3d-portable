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
            # Each path traverses the real submenus; no Docker/compiler executes.
            cases = [
                (['4'], 'dos-release', 'dbopl,silent,adlib', False),
                (['3', '5'], 'windows-win9x', 'dbopl,silent,adlib', False),
                (['3', '3'], 'windows-win2k', 'dbopl,silent,adlib', True),
                (['3', '4'], 'windows-xp', 'dbopl,silent,adlib', True),
                (['3', '1', '1'], 'windows-win7', 'nuked,dbopl,silent', True),
                (['3', '1', '2'], 'windows-llvm-win7', 'nuked,dbopl,silent', True),
                (['3', '2'], 'windows-win10', 'nuked,dbopl,silent', True),
                (['1', '1', '', ''], 'sdl3-release', 'nuked,dbopl,silent,adlib', True),
                (['1', '2', ''], 'console-release', 'nuked,dbopl,silent,adlib', True),
                (['2', '1', '1', '3'], 'portable', 'nuked,dbopl,silent,adlib', True),
                (['2', '1', '2', '1'], 'portable-sdl3', 'dbopl,silent,adlib', True),
                (['2', '1', '2', '2'], 'portable-console', 'dbopl,silent,adlib', True),
                (['2', '2', '1'], 'musl-sdl3', 'nuked,dbopl,silent,adlib', True),
                (['2', '2', '2'], 'musl-console', 'nuked,dbopl,silent,adlib', True),
            ]
            for path, target, recommended, jobs in cases:
                for custom in (False, True):
                    with self.subTest(target=target, custom=custom, path=path):
                        expected = 'nuked,dbopl,silent,adlib' if custom else recommended
                        answers = path + (['2', '2', ''] if custom else [''])
                        if jobs: answers.append('')
                        answers.append('n')
                        result = subprocess.run(['bash', str(script)], input='\n'.join(answers)+'\n',
                                                text=True, capture_output=True, env=env, timeout=10)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn('OPL drivers: ' + expected + ' (default: auto)', result.stdout)
                        command = result.stdout.replace('\\,', ',')
                        self.assertIn('invoke-build.sh ' + target + ' ', command)
                        self.assertIn('OPL_DRIVERS=' + expected, command)
                        if target.startswith('portable'):
                            self.assertIn('LINUX_ARCH=' + ('x86' if path[2] == '2' else 'x64'), command)
            for custom in (False, True):
                answers=['2','3'] + (['2','2',''] if custom else ['']) + ['', 'n']
                result=subprocess.run(['bash',str(script)],input='\n'.join(answers)+'\n',text=True,capture_output=True,env=env,timeout=10)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                self.assertIn('invoke-build.sh rh9-sdl1',result.stdout)
                self.assertIn('RH9_NUKED='+('ON' if custom else 'OFF'),result.stdout)
                self.assertIn('RH9_DBOPL=ON',result.stdout)
                self.assertIn('RH9_OPL_DEFAULT=auto',result.stdout)
                self.assertNotIn('adlib',result.stderr.split('Audio options')[-1])
            result = subprocess.run(['bash', str(script)], input='5\nn\n', text=True,
                                    capture_output=True, env=env, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn('Audio options', result.stderr)

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
