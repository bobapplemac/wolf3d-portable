"""Developer-only help inventory and optional packaged-executable checks."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--package', type=Path)
args = parser.parse_args()
source = (root/'platforms/WG_HELP.c').read_text()
help_source = source.split('static void WG_WriteBackendHelp', 1)[1].split('#define WG_DISCOVERY_PROFILE_COUNT', 1)[0]
engine = (root/'lib/wolf3d/src/WOLF3D.c').read_text()
engine_help = engine.split('const char *wolf3d_GetCommandLineHelp', 1)[1].split('\n}', 1)[0]
# Every omission must have an explicit reason; never infer hidden status from a name.
hidden = {'--sdl3-video-smoke': 'Internal automated presentation smoke test; not a player option.'}
options = set()
for path in (root/'platforms').rglob('*'):
    if path.suffix in ('.c', '.h'):
        options.update(re.findall(r'"(--[a-z][a-z0-9-]*)"', path.read_text()))
missing = sorted(option for option in options if option not in hidden and option not in help_source+engine_help)
assert not missing, 'Public options missing from help: '+', '.join(missing)
assert set(hidden) <= options, 'Remove stale hidden-option exceptions'

if args.package:
    with tempfile.TemporaryDirectory(prefix='wolf-help-') as folder:
        stage = Path(folder)
        for file in args.package.iterdir():
            if file.is_file():
                shutil.copy2(file, stage/file.name)
        exe = next(p for p in stage.iterdir() if p.name.lower() in ('wolf3d', 'wolf3d.exe'))
        def run(*flags):
            result = subprocess.run([str(exe), '--no-config', '--no-pause', *flags], cwd=stage,
                                    capture_output=True, text=True, timeout=15)
            assert result.returncode == 0, result.stdout+result.stderr
            return result.stdout
        before = run('--help')
        assert run('/?') == before
        backends = re.search(r'Compiled video backends: ([^\r\n]+)', before).group(1).split()
        required = {'gdi': ['--window-scale', '--fullscreen-mode'],
                    'sdl1': ['--sdl-render', '--gl-acceleration', '--gl-library', '--window-scale'],
                    'sdl3': ['--sdl3-help', '--fullscreen-mode'],
                    'kms-fbdev': ['--video', '--alsa-device', '--input-device'], 'vga': []}
        for backend in backends:
            for option in required[backend]+['--backend', '--opl', '--sample-rate']:
                assert option in before, option
        # Help must describe all compiled backends even without optional SDL.
        for file in stage.iterdir():
            if file.name.lower() in ('sdl.dll', 'sdl3.dll'):
                file.rename(file.with_suffix('.absent'))
        assert run('--help') == before
        for backend in backends:
            text = run('--backend', backend, '/?')
            for other in backends:
                for option in required[other]:
                    assert option in text, (backend, option)
print('Help inventory and requested package checks passed.')
