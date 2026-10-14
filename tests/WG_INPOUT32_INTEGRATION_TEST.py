"""Exercise the real x86 loader against an explicitly supplied fake InpOut DLL.
Compile WG_INPOUT32_FAKE.c with i686 MinGW -shared -Wl,--kill-at.
Never pass the genuine InpOut DLL to this test.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('package', type=Path)
p.add_argument('fake_dll', type=Path)
a = p.parse_args()
with tempfile.TemporaryDirectory(prefix='wolf-inpout-') as folder:
    root = Path(folder)
    for name in ('wolf3d.exe', 'wolf3d.dll'):
        shutil.copy2(a.package / name, root / name)
    def run(*args, expected=0):
        result = subprocess.run([str(root / 'wolf3d.exe'), '--no-config', '--no-pause', *args], cwd=root,
                                capture_output=True, text=True, timeout=15)
        assert result.returncode == expected, result.stdout + result.stderr
        return result.stdout + result.stderr
    assert 'missing or inaccessible' in run('--diag', '--opl', 'adlib', expected=1)
    text = run('--diag', '--opl', 'auto', '--sample-rate', 'auto')
    assert 'dbopl: selected; preferred 22050 Hz' in text
    assert 'missing or inaccessible' in text
    assert 'Available OPL drivers' in run('--diag', '--opl', 'nuked', expected=1)
    assert 'device 11025 Hz' in run('--diag', '--opl', 'dbopl', '--sample-rate', '11025')
    assert 'Invalid sample rate' in run('--diag', '--sample-rate', '7000', expected=1)
    (root / 'wolf3d.ini').write_text('--opl adlib\n--sample-rate auto\n')
    # Config-only strict selection and CLI override use the real launcher merge.
    def configured(*args):
        return subprocess.run([str(root / 'wolf3d.exe'), '--no-pause', '--diag', *args],
                              cwd=root, capture_output=True, text=True, timeout=15)
    assert configured().returncode == 1
    overridden = configured('--opl', 'dbopl', '--sample-rate', '11025')
    assert overridden.returncode == 0 and 'device 11025 Hz' in overridden.stdout
    (root / 'wolf3d.ini').unlink()
    shutil.copy2(a.fake_dll, root / 'inpout32.dll')
    marker = root / 'inpout-loaded.marker'
    run('--help')
    run('--diag', '--opl', 'dbopl')
    assert not marker.exists(), 'Non-native requests loaded the DLL'
    assert 'adlib: selected; preferred 22050 Hz' in run('--diag', '--opl', 'adlib')
    assert marker.exists(), 'Native probe did not load fake DLL'
    assert 'adlib: selected' in run('--diag', '--opl', 'auto')
print('Native OPL packaged loader: auto fallback, strict selection, config precedence and fake native probe passed.')
