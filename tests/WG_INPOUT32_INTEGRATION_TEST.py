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
    def run(*args):
        result = subprocess.run([str(root / 'wolf3d.exe'), '--no-config', '--no-pause', *args], cwd=root,
                                capture_output=True, text=True, timeout=15, check=True)
        return result.stdout + result.stderr
    assert 'cannot load inpout32.dll' in run('--diag', '--opl', 'adlib')
    shutil.copy2(a.fake_dll, root / 'inpout32.dll')
    marker = root / 'inpout-loaded.marker'
    run('--help')
    run('--diag')
    run('--diag', '--opl', 'dbopl')
    assert not marker.exists(), 'Non-native requests loaded the DLL'
    assert 'OPL detected at 0x388' in run('--diag', '--opl', 'adlib')
    assert marker.exists(), 'Native probe did not load fake DLL'
print('Native OPL packaged loader: explicit-only loading and simulated chip probe passed.')
