"""Reject SDL1-only options on incompatible packaged backends."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('package', type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='wolf backend options ') as directory:
    root = Path(directory)
    for source in args.package.iterdir():
        if source.is_file(): shutil.copy2(source, root / source.name)
    exe = next(p for p in root.iterdir() if p.name.lower() in ('wolf3d', 'wolf3d.exe'))
    config = root / ('wolf3d.ini' if exe.suffix.lower() == '.exe' else 'wolf3d.conf')
    def run(*options, success=True):
        p = subprocess.run([str(exe), '--no-pause', *options], cwd=root,
                           capture_output=True, text=True, timeout=20)
        text = p.stdout + p.stderr
        assert (p.returncode == 0) == success, (options, text)
        return text
    help_text = run('--no-config', '--help')
    backends = re.search(r'Compiled video backends: ([^\r\n]+)', help_text).group(1).split()
    for backend in backends:
        if backend == 'sdl1': continue
        for option, value in (('--sdl-render', 'opengl'), ('--gl-acceleration', 'auto'),
                              ('--gl-library', 'missing.dll'), ('--sdl-video', 'directx'),
                              ('--sdl-scale', 'hardware')):
            message = option + " is not supported by backend '" + backend + "'"
            assert message in run('--no-config', '--backend', backend, option, value, success=False)
            assert message in run('--no-config', '--backend', backend, option, value, '--diag', success=False)
            config.write_text('--backend ' + backend + '\n' + option + ' ' + value + '\n')
            assert message in run('--diag', success=False)
            assert 'Usage:' in run('--help')  # Help remains available for repair.
            config.unlink()
    if 'sdl3' in backends:
        # Check auto selection too, and validate before optional DLL loading.
        for source in root.iterdir():
            if source.name.lower() == 'sdl3.dll': source.rename(source.with_suffix('.saved'))
        for selection in ('auto', 'sdl3'):
            assert '--sdl-render is not supported' in run('--backend', selection,
                                                         '--sdl-render', 'opengl', success=False)
print('Backend-specific CLI/config rejection, diagnostics, and help checks passed.')
